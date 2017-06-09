
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <dirent.h>

#include <unistd.h>

int main()
{
        int dir_fd;
        DIR *dirp;

        struct dirent prev_dir, *dir;

        // open directory
        dir_fd = open("/var", O_DIRECTORY);
        if (dir_fd < 0)
        {
                fprintf(stderr, "could not open /tmp directory\n");
                return -1;
        }

        // get dirent pointer
        dirp = fdopendir(dir_fd);
        if (!dirp)
        {
                fprintf(stderr, "could not get directory pointer from descriptor\n");
                return -1;
        }

        while (readdir_r(dirp, &prev_dir, &dir) == 0)
        {
                // check next directory pointer
                if (!dir)
                        break;

                // get current directory position
                long loc = telldir(dirp);
                seekdir(dirp, loc);
        }

        rewinddir(dirp);

        // close directory descriptor
        close(dir_fd);

        return 0;
}
