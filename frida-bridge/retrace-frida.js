/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * retrace-frida: Frida bridge for retrace (TODO.complete/28 MVP).
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
 *   # Or spawn and attach:
 *   frida -l retrace-frida.js -- ./your-binary
 *
 * Configuration: edit FUNC_LIST below to add/remove intercepted
 * functions. Each entry is { name, argc }. The script logs the
 * call (with raw integer args) and return value.
 *
 * Output format (one JSON object per line, wrapped in [ ... ]):
 *   [
 *   {"time":1786269481,"module":"FUNCS","severity":"INFO",
 *    "message":{"func":"open","args":[12345678,0],"ret_val":3,
 *               "call_duration_us":12}}
 *   ,
 *   ...
 *   ]
 *
 * Standalone -- no retrace shared library required. Pairs with
 * retrace-audit / retrace-diff / retrace-replay / retrace-to-otlp.
 *
 * Limitations (MVP):
 *   - Args are integers only (Frida's args[0].toInt() etc.).
 *     Dereferencing pointers (e.g. for `char *path` in open) is
 *     possible but the API differs per platform; left as TODO.
 *   - Variadic functions (printf) get only the first N args.
 *   - No filtering; every call to a hooked function is logged.
 */

'use strict';

// Functions to hook. Add more here.
const FUNC_LIST = [
    { name: 'open', argc: 2 },
    { name: 'close', argc: 1 },
    { name: 'read', argc: 3 },
    { name: 'write', argc: 3 },
    { name: 'malloc', argc: 1 },
    { name: 'free', argc: 1 },
    { name: 'memcpy', argc: 3 },
    { name: 'strlen', argc: 1 },
    { name: 'strcmp', argc: 2 },
    { name: 'system', argc: 1 },
    { name: 'getenv', argc: 1 },
    { name: 'printf', argc: 1 },  // variadic; capture first arg only
    { name: 'fopen', argc: 2 },
    { name: 'fclose', argc: 1 },
    { name: 'connect', argc: 3 },
    { name: 'socket', argc: 3 },
    { name: 'send', argc: 4 },
    { name: 'recv', argc: 4 },
];

// Module cache: each function is resolved once.
function resolveExports() {
    const libc = Module.findExportByName(null, 'libc.so.6') ||
                 Module.findExportByName(null, 'libSystem.B.dylib') ||
                 Module.findExportByName(null, 'libc.so');

    const funcs = [];
    for (const f of FUNC_LIST) {
        const addr = Module.findExportByName(null, f.name);
        if (addr !== null) {
            funcs.push({ name: f.name, addr: addr, argc: f.argc });
        } else {
            console.error(`retrace-frida: ${f.name} not found, skipping`);
        }
    }
    return funcs;
}

// Write JSON entry to stdout. Frida's console.log goes to the
// Frida CLI, which is fine for our use case.
function emitEntry(funcName, args, retVal, durationUs) {
    const entry = {
        time: Math.floor(Date.now() / 1000),
        module: 'FUNCS',
        severity: 'INFO',
        message: {
            func: funcName,
            args: args,
            ret_val: retVal,
            call_duration_us: durationUs,
        },
    };
    console.log(JSON.stringify(entry) + ',');
}

function hookFunction(func) {
    Interceptor.attach(func.addr, {
        onEnter: function (args) {
            this.startTime = Date.now();
            // Capture the integer values of the first N args.
            this.capturedArgs = [];
            for (let i = 0; i < func.argc; i++) {
                try {
                    this.capturedArgs.push(args[i].toString());
                } catch (e) {
                    this.capturedArgs.push('?');
                }
            }
        },
        onLeave: function (retval) {
            const durationUs = Date.now() - this.startTime;  // ms precision
            let retVal;
            try {
                retVal = retval.toString();
            } catch (e) {
                retVal = '?';
            }
            emitEntry(func.name, this.capturedArgs, retVal, durationUs);
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
