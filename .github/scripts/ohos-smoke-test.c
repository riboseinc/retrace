/*
 * OHOS smoke test for retrace library loadability.
 *
 * The retrace public API (retrace.h) is currently scaffold-only; the
 * actual entry point is `retrace_engine_wrapper`, an internal symbol
 * invoked by per-function trampolines when the library is LD_PRELOADed
 * into a target process. The symbol is hidden by default
 * (CMAKE_C_VISIBILITY_PRESET=hidden), so the smoke test cannot dlsym it.
 *
 * What we can verify is:
 *   1. The .so is a valid ELF that dlopen can load under musl/OHOS.
 *   2. The library constructor runs to completion without crashing.
 *      The constructor resolves real libc symbols (retrace_real_impls_init),
 *      scans linker sections for prototypes and actions, and parses the
 *      default JSON config. A segfault during dlopen means one of those
 *      steps failed under musl's TLS/dynamic linker semantics.
 *
 * Functional verification (actual libc interception) is done by the
 * LD_PRELOAD end-to-end test in the workflow.
 *
 * Build:
 *   $OHOS_CLANG --target=aarch64-linux-ohos --sysroot=$OHOS_SYSROOT \
 *       -O2 -L build-ohos/src/v2 -Wl,-rpath='$ORIGIN' \
 *       -o build-ohos/ohos-smoke-test \
 *       .github/scripts/ohos-smoke-test.c -ldl
 *
 * Exit codes: 0 = pass, 2 = FAIL, 3 = setup error.
 */

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void)
{
    void *handle = dlopen("./libretrace.so.2", RTLD_NOW | RTLD_LOCAL);
    if (handle == NULL) {
        fprintf(stderr, "FAIL: dlopen(libretrace.so.2) failed: %s\n", dlerror());
        return 2;
    }
    printf("dlopen(libretrace.so.2): OK (constructor ran)\n");
    printf("OK\n");
    fflush(stdout);

    /* _exit skips atexit handlers and pthread key destructors. The
     * destructor path segfaults under QEMU/musl emulation; it does not
     * reproduce on real OHOS hardware and is not exercised by the
     * LD_PRELOAD usage model (which never unloads the library).
     * Functional verification is done by the LD_PRELOAD end-to-end test
     * in the workflow. */
    _exit(0);
}
