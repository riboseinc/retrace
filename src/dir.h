#ifndef __DIR_H__
#define __DIR_H__

typedef DIR *(*rtr_opendir_t)(const char *dirname);
typedef int (*rtr_closedir_t)(DIR *dirp);

typedef DIR *(*rtr_fdopendir_t)(int fd);

typedef int (*rtr_readdir_r_t)(DIR *dirp, struct dirent *entry, struct dirent **result);
typedef long (*rtr_telldir_t)(DIR *dirp);
typedef void (*rtr_seekdir_t)(DIR *dirp, long loc);
typedef void (*rtr_rewinddir_t)(DIR *dirp);

typedef int (*rtr_dirfd_t)(DIR *dirp);

RETRACE_DECL(opendir);
RETRACE_DECL(closedir);
RETRACE_DECL(fdopendir);
RETRACE_DECL(readdir_r);
RETRACE_DECL(telldir);
RETRACE_DECL(seekdir);
RETRACE_DECL(rewinddir);
RETRACE_DECL(dirfd);

#endif /* __DIR_H__ */
