#include <fcntl.h>
#include <unistd.h>

int main (void)
{
	int fd[2];

	pipe (fd);
	pipe2(fd, 42);

        return 0;
}

