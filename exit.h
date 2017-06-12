#ifndef __RETRACE_EXIT_H__
#define __RETRACE_EXIT_H__

typedef void (*rtr_exit_t)(int status) __attribute__((noreturn));

RETRACE_DECL(exit);

#endif /* __RETRACE_EXIT_H__ */
