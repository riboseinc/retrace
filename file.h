#ifndef __RETRACE_FILE_H__
#define __RETRACE_FILE_H__

typedef FILE *(*rtr_fopen_t)(const char *filename, const char *mode);
typedef int (*rtr_fclose_t)(FILE *stream);
typedef int (*rtr_fseek_t)(FILE *stream, long offset, int whence);
typedef int (*rtr_fileno_t)(FILE *stream);
typedef int (*rtr_chmod_t)(const char *path, mode_t mode);
typedef int (*rtr_fchmod_t)(int fd, mode_t mode);
typedef int (*rtr_stat_t)(const char *path, struct stat *buf);
typedef int (*rtr_dup_t)(int oldfd);
typedef int (*rtr_dup2_t)(int oldfd, int newfd);
typedef int (*rtr_close_t)(int fd);
typedef mode_t (*rtr_umask_t)(mode_t mask);

typedef int (*rtr_mkfifo_t)(const char *pathname, mode_t mode);

rtr_fopen_t    real_fopen;
rtr_fclose_t   real_fclose;
rtr_fseek_t    real_fseek;
rtr_fileno_t   real_fileno;
rtr_chmod_t    real_chmod;
rtr_fchmod_t   real_fchmod;
rtr_stat_t     real_stat;
rtr_dup_t      real_dup;
rtr_dup2_t     real_dup2;
rtr_close_t    real_close;
rtr_umask_t    real_umask;
rtr_mkfifo_t   real_mkfifo;

#endif /* __RETRACE_FILE_H__ */
