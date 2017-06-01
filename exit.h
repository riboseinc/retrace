#ifndef __RETRACE_EXIT_H__
#define __RETRACE_EXIT_H__

static void (*real_exit)(int status) __attribute__ ((noreturn));

#endif /* __RETRACE_EXIT_H__ */
