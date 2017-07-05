#ifndef __RETRACE_EXIT_H__
#define __RETRACE_EXIT_H__

typedef void (*rtr_exit_t)(int status) __attribute__((noreturn));
typedef int (*rtr_atexit_t)(void (*function)(void));
typedef int (*rtr_on_exit_t)(void (*function)(int, void *), void *arg);
typedef void (*rtr__exit_t)(int status);
typedef int (*rtr___cxa_atexit_t)(void (*function)(void));

RETRACE_DECL(exit);
RETRACE_DECL(atexit);
RETRACE_DECL(on_exit);
RETRACE_DECL(_exit);
RETRACE_DECL(__cxa_atexit);

#endif /* __RETRACE_EXIT_H__ */
