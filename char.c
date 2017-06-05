#include "common.h"
#include "file.h"

#include "char.h"

int
putc(int c, FILE *stream)
{
    real_putc = dlsym(RTLD_NEXT, "putc");
    real_fileno = dlsym(RTLD_NEXT, "fileno");
    int fd = real_fileno(stream);

    trace_printf(1, "putc(");
    if (c == '\n')
        trace_printf(0, "%s\\n%s", VAR, RST);
    else if (c == '\t')
        trace_printf(0, "%s\\t%s", VAR, RST);
    else if (c == '\r')
        trace_printf(0, "%s\\r%s", VAR, RST);
    else if (c == '\0')
        trace_printf(0, "%s(nullterm)%s", VAR, RST);
    else
        trace_printf(0, "%c", c);

    trace_printf(0, ", %d);\n", fd);

    return real_putc(c, stream);
}

int
toupper(int c)
{
    real_toupper = dlsym(RTLD_NEXT, "toupper");
    trace_printf(1, "toupper(\"%s\");\n", &c);
    return real_toupper(c);
}

int
tolower(int c)
{
    real_tolower = dlsym(RTLD_NEXT, "tolower");
    trace_printf(1, "tolower(\"%c\");\n", &c);
    return real_tolower(c);
}
