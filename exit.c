#include "common.h"
#include "exit.h"

void
exit(int status)
{
    real_exit = dlsym(RTLD_NEXT, "exit");
    trace_printf(1, "exit(%s%d%s);\n", VAR, status, RST);
    real_exit(status);
}
