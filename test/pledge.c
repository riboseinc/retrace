#include <unistd.h> 

int main(void)
{
#ifdef __OpenBSD__
	const char *paths[] = {"test1", "42", NULL};

	pledge("stdio", paths); 
#endif /* __OpenBSD */

	return 0;
}
