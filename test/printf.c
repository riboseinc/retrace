#include <stdio.h>
#include <stdarg.h>

void more_tests(char *fmt, ...)
{
	char buf[1024];
	va_list ap;

	buf[0] = 0;

	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	va_start(ap, fmt);
	vdprintf(1, fmt, ap);
	va_end(ap);

	va_start(ap, fmt);
	vsprintf(buf, fmt, ap);
	va_end(ap);

	va_start(ap, fmt);
	vsnprintf(buf, 42, fmt, ap);
	va_end(ap);
}

int main(void)
{
	char buf[1024];

	buf[0] = 0;

	printf("Test %d %s\n", 42, "forty two");

	sprintf(buf, "test %d %s\n", 1, "hello");

	fprintf(stdout, "TEST %x\n", 42);

	dprintf(1, "Another test %o\n", 42);

	snprintf(buf, 20, "%s Helloooo\n", "string");

	more_tests("Format String %u\n", 42);

	return 0;
}
