#include "common.h"
#include "str.h"

char *
strstr(const char *s1, const char *s2)
{
    real_strstr = dlsym(RTLD_NEXT, "strstr");

    trace_printf(1, "strstr(\"");
    trace_printf_str(s1);
    trace_printf(0, "\", \"");
    trace_printf_str(s2);
    trace_printf(0, "\");\n");

    return real_strstr(s1, s2);
}

size_t
strlen(const char *s)
{
    real_strlen = dlsym(RTLD_NEXT, "strlen");

    size_t len = real_strlen(s);

    trace_printf(1, "strlen(\"%s\"); [len: %zu]\n", s, len);

    return len;
}

int
strncmp(const char *s1, const char *s2, size_t n)
{
    real_strncmp = dlsym(RTLD_NEXT, "strncmp");

    trace_printf(1, "strncmp(\"");
    trace_printf_str(s1);
    trace_printf(0, "\", \"");
    trace_printf_str(s2);
    trace_printf(0, "\", %zu);\n", n);

    return real_strncmp(s1, s2, n);
}

int
strcmp(const char *s1, const char *s2)
{
    real_strcmp = dlsym(RTLD_NEXT, "strcmp");

    trace_printf(1, "strcmp(\"");
    trace_printf_str(s1);
    trace_printf(0, "\", \"");
    trace_printf_str(s2);
    trace_printf(0, "\");\n");

    return real_strcmp(s1, s2);
}

char *
strncpy(char *s1, const char *s2, size_t n)
{
    real_strncpy = dlsym(RTLD_NEXT, "strncpy");
    real_strlen = dlsym(RTLD_NEXT, "strlen");

    size_t len = real_strlen(s2);

    trace_printf(1, "strncpy(%p, \"", s2);
    trace_printf_str(s2);
    trace_printf(0, "\" [len: %d], %zu);\n", len, n);

    return real_strncpy(s1, s2, n);
}

char *
strcat(char *s1, const char *s2)
{
    real_strcat = dlsym(RTLD_NEXT, "strcat");
    real_strlen = dlsym(RTLD_NEXT, "strlen");

    size_t len = real_strlen(s2);

    trace_printf(1, "strcat(%p, \"", s1);
    trace_printf_str(s2);
    trace_printf(0, "\"); [len %zu]\n", len);

    return real_strcat(s1, s2);
}

char *
strncat(char *s1, const char *s2, size_t n)
{
    real_strncat = dlsym(RTLD_NEXT, "strncat");
    real_strlen = dlsym(RTLD_NEXT, "strlen");

    size_t len = real_strlen(s2) + 1;

    trace_printf(1, "strncat(%p, \"", s1);
    trace_printf_str(s2);
    trace_printf(0, "\", %zu\"); [len: %zu]\n", len, n);

    return real_strncat(s1, s2, n);
}

char *
strcpy(char *s1, const char *s2)
{
    real_strcpy = dlsym(RTLD_NEXT, "strcpy");
    real_strlen = dlsym(RTLD_NEXT, "strlen");

    size_t len = real_strlen(s2);

    trace_printf(1, "strcpy(%p, \"", s1);
    trace_printf_str(s2);
    trace_printf(0, "\" [%zu]);\n", len);

    return real_strcpy(s1, s2);
}
