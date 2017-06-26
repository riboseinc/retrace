#ifndef __RETRACE_PIPE_H__
#define __RETRACE_PIPE_H__

typedef int (*rtr_pipe_t)(int pipefd[2]);

RETRACE_DECL(pipe);

#ifndef __APPLE__

typedef int (*rtr_pipe2_t)(int pipefd[2], int flags);

RETRACE_DECL(pipe2);

#endif

#endif /* __RETRACE_PIPE_H__ */
