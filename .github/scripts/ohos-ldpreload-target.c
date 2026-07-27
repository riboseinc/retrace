/*
 * Target program for OHOS end-to-end LD_PRELOAD smoke test.
 *
 * Calls a handful of libc symbols that retrace intercepts (getuid, getgid,
 * geteuid, getegid). When run under LD_PRELOAD=libretrace_v2.so with a
 * JSON config that activates log_params for these symbols, retrace will
 * emit log lines to stderr (or the configured logger destination).
 *
 * The workflow runs this binary under LD_PRELOAD inside dockerharmony and
 * checks that the log output contains the expected function name.
 *
 * Exit codes: 0 = program ran successfully (whether or not tracing is on).
 */

#include <unistd.h>
#include <stdio.h>

int main(void)
{
    uid_t uid  = getuid();
    gid_t gid  = getgid();
    uid_t euid = geteuid();
    gid_t egid = getegid();

    printf("uid=%u gid=%u euid=%u egid=%u\n",
           (unsigned)uid, (unsigned)gid, (unsigned)euid, (unsigned)egid);
    return 0;
}
