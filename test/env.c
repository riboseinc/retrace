#include <stdlib.h>

int main (void)
{
	putenv("RETRACE_TEST=1");
	getenv ("RETRACE_TEST");
	unsetenv("RETRACE_TEST");

	return 0;
}

