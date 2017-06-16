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
typedef int (*rtr_fputc_t)(int c, FILE *stream);
typedef int (*rtr_fputs_t)(const char *s, FILE *stream);
typedef int (*rtr_fgetc_t)(FILE *stream);
typedef char*(*rtr_fgets_t)(char *s, int size, FILE *stream);
typedef void (*rtr_strmode_t)(int mode, char *bp);

RETRACE_DECL(fopen);
RETRACE_DECL(fclose);
RETRACE_DECL(fseek);
RETRACE_DECL(fileno);
RETRACE_DECL(chmod);
RETRACE_DECL(fchmod);
RETRACE_DECL(stat);
RETRACE_DECL(dup);
RETRACE_DECL(dup2);
RETRACE_DECL(close);
RETRACE_DECL(umask);
RETRACE_DECL(mkfifo);
RETRACE_DECL(open);
RETRACE_DECL(fread);
RETRACE_DECL(fwrite);
RETRACE_DECL(fputc);
RETRACE_DECL(fputs);
RETRACE_DECL(fgetc);
RETRACE_DECL(fgets);
RETRACE_DECL(strmode);

#endif /* __RETRACE_FILE_H__ */
