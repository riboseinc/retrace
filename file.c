#include "common.h"
#include "file.h"

int
stat(const char *path, struct stat *buf)
{
    real_stat = dlsym(RTLD_NEXT, "stat");

    trace_printf(1, "stat(\"%s\", \"\");\n", path);
    return real_stat(path, buf);
}

int
chmod(const char *path, mode_t mode)
{
    real_chmod = dlsym(RTLD_NEXT, "chmod");

    trace_printf(1, "chmod(\"%s\", %o);\n", path, mode);
    return real_chmod(path, mode);
}

int
fchmod(int fd, mode_t mode)
{
    real_fchmod = dlsym(RTLD_NEXT, "fchmod");

    trace_printf(1, "fchmod(%d, %o);\n", fd, mode);
    return real_fchmod(fd, mode);
}

int
fileno(FILE *stream)
{
    real_fileno = dlsym(RTLD_NEXT, "fileno");
    int fd = real_fileno(stream);

    trace_printf(1, "fileno(%d);\n", fd);
    return real_fileno(stream);
}

int
fseek(FILE *stream, long offset, int whence)
{
    real_fseek = dlsym(RTLD_NEXT, "fseek");
    real_fileno = dlsym(RTLD_NEXT, "fileno");
    int fd = real_fileno(stream);

    trace_printf(1, "fseek(%d, %lx, ", fd, offset);

    if (whence == 0)
        trace_printf(0, "SEEK_SET");
    else if (whence == 1)
        trace_printf(0, "SEEK_CUR");
    else if (whence == 2)
        trace_printf(0, "SEEK_END");
    else
        trace_printf(0, "UNDEFINED");

    trace_printf(0, ");\n");

    return real_fseek(stream, offset, whence);
}

int
fclose(FILE *stream)
{
    real_fclose = dlsym(RTLD_NEXT, "fclose");
    real_fileno = dlsym(RTLD_NEXT, "fileno");
    int fd = real_fileno(stream);

    trace_printf(1, "fclose(%d);\n", fd);
    return real_fclose(stream);
}

FILE *
fopen(const char *file, const char *mode)
{
    real_fopen = dlsym(RTLD_NEXT, "fopen");
    real_fileno = dlsym(RTLD_NEXT, "fileno");
    int fd = 0;

    FILE *ret = real_fopen(file, mode);

    if (ret)
        fd = real_fileno(ret);

    trace_printf(1, "fopen(\"%s\", \"%s\"); [%d]\n", file, mode, fd);

    return (ret);
}

DIR *
opendir(const char *dirname)
{
    real_opendir = dlsym(RTLD_NEXT, "opendir");
    trace_printf(1, "opendir(\"%s\");\n", dirname);
    return real_opendir(dirname);
}

int
closedir(DIR *dirp)
{
    real_closedir = dlsym(RTLD_NEXT, "closedir");
    trace_printf(1, "closedir();\n");
    return real_closedir(dirp);
}

int
close(int fd)
{
    real_close = dlsym(RTLD_NEXT, "close");
    trace_printf(1, "close(%d);\n", fd);
    return real_close(fd);
}

int
dup(int oldfd)
{
    real_dup = dlsym(RTLD_NEXT, "dup");
    trace_printf(1, "dup(%d)\n", oldfd);
    return real_dup(oldfd);
}

int
dup2(int oldfd, int newfd)
{
    real_dup2 = dlsym(RTLD_NEXT, "dup2");
    trace_printf(1, "dup2(%d, %d)\n", oldfd, newfd);
    return real_dup2(oldfd, newfd);
}
