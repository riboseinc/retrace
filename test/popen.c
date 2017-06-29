#include <stdio.h>

int main(void)
{
	FILE *f = popen("/bin/true", "r");

	if (f)
		pclose(f);

	return 0;
}
