#ifndef __DIR_H__
#define __DIR_H__

typedef DIR *(*rtr_fdopendir_t)(int fd);

typedef int (*rtr_readdir_r_t)(DIR *dirp, struct dirent *entry, struct dirent **result);
typedef long (*rtr_telldir_t)(DIR *dirp);
typedef void (*rtr_seekdir_t)(DIR *dirp, long loc);
typedef void (*rtr_rewinddir_t)(DIR *dirp);

typedef int (*rtr_dirfd_t)(DIR *dirp);

rtr_fdopendir_t         real_fdopendir;
rtr_readdir_r_t         real_readdir_r;
rtr_telldir_t           real_telldir;
rtr_seekdir_t           real_seekdir;
rtr_rewinddir_t         real_rewinddir;
rtr_dirfd_t             real_dirfd;

#endif /* __DIR_H__ */
