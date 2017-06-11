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
typedef int (*rtr_open_t)(const char *pathname, int flags, ...);
typedef size_t (*rtr_fread_t)(void *ptr, size_t size, size_t nmemb, FILE *stream);
typedef size_t (*rtr_fwrite_t)(const void *ptr, size_t size, size_t nmemb, FILE *stream);
typedef int (*rtr_fputc)(int c, FILE *stream);
typedef int (*rtr_fputs)(const char *s, FILE *stream);
typedef int (*rtr_fgetc)(FILE *stream);
typedef char*(*rtr_fgets)(char *s, int size, FILE *stream);

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
rtr_open_t     real_open;
rtr_fread_t    real_fread;
rtr_fwrite_t   real_fwrite;
rtr_fputc      real_fputc;
rtr_fputs      real_fputs;
rtr_fgetc      real_fgetc;
rtr_fgets      real_fgets;

#endif /* __RETRACE_FILE_H__ */
