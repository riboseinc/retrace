#ifndef __SCANF_H__
#define __SCANF_H__

typedef int (*rtr_scanf_t)(const char *format, ...);
typedef int (*rtr_fscanf_t)(FILE *stream, const char *format, ...);
typedef int (*rtr_sscanf_t)(const char *str, const char *format, ...);
typedef int (*rtr_vscanf_t)(const char *format, va_list ap);
typedef int (*rtr_vsscanf_t)(const char *str, const char *format, va_list ap);
typedef int (*rtr_vfscanf_t)(FILE *stream, const char *format, va_list ap);

RETRACE_DECL(scanf);
RETRACE_DECL(fscanf);
RETRACE_DECL(sscanf);
RETRACE_DECL(vscanf);
RETRACE_DECL(vsscanf);
RETRACE_DECL(vfscanf);

#endif  /* __SCANF_H__ */