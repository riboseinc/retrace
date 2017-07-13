/* getenv.c example for retrace getenv fuzzing
 *
 * this source contains two vulnerabilities:
 * 1) a buffer overflow
 * 2) a format string bug
 *
 * Compile: gcc getenv.c -o getenv
 *
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

int
main()
{
	const char *s = getenv("FOOBAR");
	char buf[128];

	if (s == NULL) {
		printf("environment variable 'FOOBAR' unset\n");
		exit(1);
	}

	/* this test might result in a segfault due to a buffer overflow */
	printf("strcpy(buf(%d), FOOBAR);\n", sizeof(buf));
	strcpy(buf, s);
	printf("FOOBAR [bof] | %s\n", buf);

	printf("FOOBAR [fmt] | ");
	/* this test is for format string */
	printf(s);
	printf("\n");

	return(0);
}
