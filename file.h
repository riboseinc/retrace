#ifndef __RETRACE_FILE_H__
#define __RETRACE_FILE_H__

FILE *(*real_fopen)(const char *filename, const char *mode);
DIR *(*real_opendir)(const char *dirname);
int (*real_fclose)(FILE *stream);
int (*real_closedir)(DIR *dirp);
int (*real_fseek)(FILE *stream, long offset, int whence);
int (*real_fileno)(FILE *stream);
int (*real_chmod)(const char *path, mode_t mode);
int (*real_fchmod)(int fd, mode_t mode);
int (*real_stat)(const char *path, struct stat *buf);
int (*real_dup)(int oldfd);
int (*real_dup2)(int oldfd, int newfd);
int (*real_close)(int fd);

#endif /* __RETRACE_FILE_H__ */
