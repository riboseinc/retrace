#ifndef __RETRACE_PRINTF_H__
#define __RETRACE_PRINTF_H__
typedef int (*rtr_printf_t)(const char *fmt, ...);
typedef int (*rtr_fprintf_t)(FILE *stream, const char *fmt, ...);
typedef int (*rtr_dprintf_t)(int fd, const char *fmt, ...);
typedef int (*rtr_sprintf_t)(char *str, const char *fmt, ...);
typedef int (*rtr_snprintf_t)(char *str, size_t size, const char *fmt, ...);
typedef int (*rtr_vprintf_t)(const char *fmt, va_list ap);
typedef int (*rtr_vfprintf_t)(FILE *stream, const char *fmt, va_list ap);
typedef int (*rtr_vdprintf_t)(int fd, const char *fmt, va_list ap);
typedef int (*rtr_vsprintf_t)(char *str, const char *fmt, va_list ap);
typedef int (*rtr_vsnprintf_t)(char *buf, size_t size, const char *fmt, va_list ap);

RETRACE_DECL(printf);
RETRACE_DECL(fprintf);
RETRACE_DECL(dprintf);
RETRACE_DECL(sprintf);
RETRACE_DECL(snprintf);
RETRACE_DECL(vprintf);
RETRACE_DECL(vfprintf);
RETRACE_DECL(vdprintf);
RETRACE_DECL(vsprintf);
RETRACE_DECL(vsnprintf);

#endif /* __RETRACE_PRINTF_H__ */
