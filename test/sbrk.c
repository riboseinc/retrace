#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <unistd.h>

static void test_sbrk(void)
{
	void *p, *request;

	p = sbrk(0);
	request = sbrk(1024);

	if (request == (void *) -1)
		return;

	brk(p);
}

int main(void)
{
	int i;

	for (i = 0; i < 1000; i++)
		test_sbrk();
}
