#include <stdlib.h>

int main (void)
{
	void *p = malloc (42);

	free (p);

        return 0;
}

