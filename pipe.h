#ifndef __RETRACE_PIPE_H__
#define __RETRACE_PIPE_H__

typedef int (*rtr_pipe_t)(int pipefd[2]);
typedef int (*rtr_pipe2_t)(int pipefd[2], int flags);

RETRACE_DECL(pipe);
RETRACE_DECL(pipe2);

#endif /* __RETRACE_PIPE_H__ */
