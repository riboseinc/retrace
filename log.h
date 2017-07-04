#ifndef __RETRACE_LOG_H__
#define __RETRACE_LOG_H__

typedef void (*rtr_openlog_t)(const char *ident, int option, int facility);
typedef void (*rtr_syslog_t)(int priority, const char *format, ...);
typedef void (*rtr_closelog_t)(void);
typedef void (*rtr_vsyslog_t)(int priority, const char *format, va_list ap);
typedef int (*rtr_setlogmask_t)(int mask);


RETRACE_DECL(openlog);
RETRACE_DECL(syslog);
RETRACE_DECL(closelog);
RETRACE_DECL(vsyslog);
RETRACE_DECL(setlogmask);

#endif /* __RETRACE_LOG_H__ */
