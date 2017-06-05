#include "common.h"
#include "id.h"

int
setuid(uid_t uid)
{
    real_setuid = dlsym(RTLD_NEXT, "setuid");
    trace_printf(1, "setuid(%d);\n", uid);
    return real_setuid(uid);
}

int
seteuid(uid_t uid)
{
    real_seteuid = dlsym(RTLD_NEXT, "seteuid");
    trace_printf(1, "seteuid(%d);\n", uid);
    return real_seteuid(uid);
}

int
setgid(gid_t gid)
{
    real_setgid = dlsym(RTLD_NEXT, "setgid");
    trace_printf(1, "setgid(%d);\n", gid);
    return real_setgid(gid);
}

gid_t
getgid()
{
    real_getgid = dlsym(RTLD_NEXT, "getgid");
    trace_printf(1, "getgid();\n");
    return real_getgid();
}

gid_t
getegid()
{
    real_getegid = dlsym(RTLD_NEXT, "getegid");
    trace_printf(1, "getegid();\n");
    return real_getegid();
}

uid_t
getuid()
{
    real_getuid = dlsym(RTLD_NEXT, "getuid");
    trace_printf(1, "getuid();\n");
    return real_getuid();
}

uid_t
geteuid()
{
    real_geteuid = dlsym(RTLD_NEXT, "geteuid");
    trace_printf(1, "geteuid();\n");
    return real_geteuid();
}

pid_t
getpid(void)
{
    real_getpid = dlsym(RTLD_NEXT, "getpid");
    trace_printf(1, "getpid();\n");
    return real_getpid();
}

pid_t
getppid(void)
{
    real_getppid = dlsym(RTLD_NEXT, "getppid");
    trace_printf(1, "getppid(); [%d]\n", real_getppid());
    return real_getppid();
}
