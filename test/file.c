/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

int main(void)
{
	FILE *f;
	char *s1 = "This is a test string :)";
	char buf[1024];
	char carray[] = {'x', 'y', 'z'};

	chmod("retracetest.deleteme123", 777);
	fchmod(42, 444);

	close(42);
	dup(42);
	dup2(42, 43);
	umask(777);

	mkfifo("/dev/null", 777);

	open("/dev/null", O_APPEND);

	f = fopen("retracetest.deleteme", "w+");

	if (f) {
		fileno(f);
		printf("Opened file\n");
		fwrite(carray, sizeof(char), sizeof(carray), f);
		fputs(s1,  f);
		fputc('6', f);
		putc('6', f);

		fseek(f, 0, SEEK_SET);

		rewind(f);

		fread(buf, strlen(s1), 1, f);

		rewind(f);

		fgets(buf, strlen(s1), f);
		fgetc(f);

		fclose(f);
	}

#ifdef __APPLE__
	strmode(717, buf);
#endif

	f = fopen("/etc/passwd", "w+");
	if (f != NULL)
		fclose(f);

	return 0;
}
