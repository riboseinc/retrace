#ifndef __RETRACE_EXIT_H__
#define __RETRACE_EXIT_H__

typedef void (*rtr_exit_t)(int status) __attribute__((noreturn));

rtr_exit_t real_exit;

#endif /* __RETRACE_EXIT_H__ */
