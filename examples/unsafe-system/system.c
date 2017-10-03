#include <unistd.h>
#include <stdlib.h>

int
main()
{
	setuid(0);
	system("id");
	return(0);
}
