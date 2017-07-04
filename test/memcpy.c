#include <stdio.h>
#include <string.h>

int main(void)
{
	char p[36];
	char *q = p + 10;

	memset(p, '1', sizeof(p));

	memcpy(q, p, 20);
}
