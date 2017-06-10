#include <stdio.h>
#include <string.h>

int main(void)
{
	FILE *f;
	char *s = "This is a test string :)";
	char buf[1024];

	f = fopen ("retracetest.deleteme", "w+");

	if (f) {
		fwrite (s, strlen(s), 1, f);
		fputs (s,  f);
		fputc ('6', f);

		rewind (f);

		fread(buf, strlen(s), 1, f);

		rewind (f);

		fgets (buf, strlen(s), f);
		fgetc (f);

		fclose (f);
	}

	return 0;
}
