#include <sys/types.h>
#include <unistd.h>


int main (void)
{
	setuid(111);
	seteuid(222);
	setgid(333);
	getgid();
	getuid();
	geteuid();
	getpid();
	getppid();

	return 0;
}

