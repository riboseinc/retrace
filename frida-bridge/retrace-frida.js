/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * retrace-frida: Frida bridge for retrace (TODO.complete/28).
 *
 * A Frida script that hooks libc functions and emits retrace-
 * compatible JSON to stdout. The output is consumable by every
 * retrace tool that reads JSON logs: retrace-audit, retrace-diff,
 * retrace-replay, retrace-to-otlp.
 *
 * Use cases:
 *   - You can't use LD_PRELOAD (e.g. iOS, or a static binary).
 *   - You want to attach to an already-running process.
 *   - You want to use Frida's existing ecosystem (scripts, tools).
 *
 * Usage:
 *   frida -l retrace-frida.js -n YourProcess
 *   frida -l retrace-frida.js -f ./your-binary
 *   frida -l retrace-frida.js -- ./your-binary
 *
 * Configuration: edit FUNC_LIST below. Each entry has a name,
 * argc, and an optional `stringArgs` array listing the 0-based
 * argument indices that should be dereferenced as C strings
 * (e.g. for `open(const char *path, int flags)`, stringArgs=[0]
 * makes the script read up to 256 bytes at the path pointer
 * and include the string in the args array).
 *
 * Output format (one JSON object per line, wrapped in [ ... ]):
 *   [
 *   {"time":1786269481,"module":"FUNCS","severity":"INFO",
 *    "message":{"func":"open","args":["/etc/passwd",0],
 *               "ret_val":3,"call_duration_us":12}}
 *   ,
 *   ...
 *   ]
 *
 * Pairs with: retrace-audit / retrace-diff / retrace-replay / retrace-to-otlp.
 */

'use strict';

/*
 * Functions to hook. Add more here.
 *
 * `stringArgs` (TODO.complete/28 P1) lists the 0-based argument
 * indices that should be dereferenced as NUL-terminated C
 * strings. Frida's Memory.readUtf8String handles the read
 * safely (returns null if the pointer is invalid). The string
 * is truncated to 256 chars to bound the output size.
 *
 * Functions not in the stringArgs map get raw integer args
 * only (the MVP behavior).
 */
const STRING_ARG_MAX = 256;

const FUNC_LIST = [
    { name: 'open',     argc: 2, stringArgs: [0] },          /* path */
    { name: 'openat',   argc: 3, stringArgs: [1] },          /* pathname (after dfd) */
    { name: 'openat2',  argc: 4, stringArgs: [1] },
    { name: 'creat',    argc: 2, stringArgs: [0] },
    { name: 'access',   argc: 2, stringArgs: [0] },
    { name: 'stat',     argc: 2, stringArgs: [0] },
    { name: 'lstat',    argc: 2, stringArgs: [0] },
    { name: 'unlink',   argc: 1, stringArgs: [0] },
    { name: 'mkdir',    argc: 2, stringArgs: [0] },
    { name: 'rmdir',    argc: 1, stringArgs: [0] },
    { name: 'chmod',    argc: 2, stringArgs: [0] },
    { name: 'chown',    argc: 3, stringArgs: [0] },
    { name: 'truncate', argc: 2, stringArgs: [0] },
    { name: 'rename',   argc: 2, stringArgs: [0, 1] },       /* oldpath, newpath */
    { name: 'symlink',  argc: 2, stringArgs: [0, 1] },       /* target, linkpath */
    { name: 'readlink', argc: 3, stringArgs: [0] },
    { name: 'fopen',    argc: 2, stringArgs: [0, 1] },       /* pathname, mode */
    { name: 'freopen',  argc: 3, stringArgs: [0, 1] },
    { name: 'system',   argc: 1, stringArgs: [0] },          /* command */
    { name: 'getenv',   argc: 1, stringArgs: [0] },          /* name */
    { name: 'setenv',   argc: 3, stringArgs: [0, 1] },       /* name, value */
    { name: 'unsetenv', argc: 1, stringArgs: [0] },
    { name: 'execve',   argc: 3, stringArgs: [0, 1] },       /* pathname, argv */
    { name: 'execl',    argc: 2, stringArgs: [0] },
    { name: 'execlp',   argc: 2, stringArgs: [0] },
    { name: 'execle',   argc: 2, stringArgs: [0] },
    { name: 'execvp',   argc: 2, stringArgs: [0] },
    { name: 'execvpe',  argc: 3, stringArgs: [0] },
    { name: 'dlopen',   argc: 2, stringArgs: [0] },          /* filename */
    { name: 'dlsym',    argc: 2, stringArgs: [1] },          /* symbol (2nd arg) */
    /* No stringArgs -- integer-only: */
    { name: 'close',    argc: 1 },
    { name: 'read',     argc: 3 },
    { name: 'write',    argc: 3 },
    { name: 'malloc',   argc: 1 },
    { name: 'free',     argc: 1 },
    { name: 'memcpy',   argc: 3 },
    { name: 'strlen',   argc: 1, stringArgs: [0] },          /* s */
    { name: 'strcmp',   argc: 2, stringArgs: [0, 1] },       /* s1, s2 */
    { name: 'strcpy',   argc: 2, stringArgs: [1] },          /* src (don't deref dest) */
    { name: 'strncpy',  argc: 3, stringArgs: [1] },
    { name: 'strcat',   argc: 2, stringArgs: [1] },
    { name: 'printf',   argc: 1 },                            /* variadic; fmt is hard to deref cleanly */
    { name: 'fclose',   argc: 1 },
    { name: 'connect',  argc: 3 },
    { name: 'socket',   argc: 3 },
    { name: 'send',     argc: 4 },
    { name: 'recv',     argc: 4 },
];

function resolveExports() {
    const funcs = [];
    for (const f of FUNC_LIST) {
        const addr = Module.findExportByName(null, f.name);
        if (addr !== null) {
            funcs.push({
                name: f.name,
                addr: addr,
                argc: f.argc,
                stringArgs: f.stringArgs || [],
            });
        } else {
            console.error(`retrace-frida: ${f.name} not found, skipping`);
        }
    }
    return funcs;
}

/*
 * Safely read a NUL-terminated string from a user pointer.
 * Returns the string (truncated to STRING_ARG_MAX chars) or
 * the original integer if the read fails (invalid pointer,
 * unreadable page, etc.).
 */
function readStringArg(ptrVal) {
    if (ptrVal === null || ptrVal.isNull())
        return '0x0';
    try {
        const s = Memory.readUtf8String(ptrVal, STRING_ARG_MAX);
        return s !== null ? s : ptrVal.toString();
    } catch (e) {
        /* The pointer was unreadable. Fall back to the integer
         * representation so the consumer at least knows the
         * pointer value.
         */
        return ptrVal.toString();
    }
}

function emitEntry(funcName, args, retVal, durationMs) {
    const entry = {
        time: Math.floor(Date.now() / 1000),
        module: 'FUNCS',
        severity: 'INFO',
        message: {
            func: funcName,
            args: args,
            ret_val: retVal,
            call_duration_us: durationMs * 1000,
        },
    };
    console.log(JSON.stringify(entry) + ',');
}

function hookFunction(func) {
    Interceptor.attach(func.addr, {
        onEnter: function (args) {
            this.startTime = Date.now();
            this.capturedArgs = [];
            for (let i = 0; i < func.argc; i++) {
                if (func.stringArgs.indexOf(i) >= 0) {
                    this.capturedArgs.push(readStringArg(args[i]));
                } else {
                    try {
                        this.capturedArgs.push(args[i].toString());
                    } catch (e) {
                        this.capturedArgs.push('?');
                    }
                }
            }
        },
        onLeave: function (retval) {
            const durationMs = Date.now() - this.startTime;
            let retVal;
            try {
                retVal = retval.toString();
            } catch (e) {
                retVal = '?';
            }
            emitEntry(func.name, this.capturedArgs, retVal, durationMs);
        },
    });
}

function main() {
    console.log('[');
    const funcs = resolveExports();
    for (const f of funcs) {
        hookFunction(f);
    }
    console.error(
        `retrace-frida: hooked ${funcs.length} functions. ` +
        'Logging to stdout (retrace JSON format).'
    );
}

main();
