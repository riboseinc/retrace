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
#include <stdarg.h>
#include <unistd.h>

int scanf_test(void)
{
	FILE oldstdin;
	char buf[1024];
	int fd[2];

	pipe(fd);
	oldstdin = *stdin;
	*stdin = *fdopen(fd[0], "r");
	write(fd[1], "string123 ", strlen("string123 "));
	scanf("%s", buf);
	fclose(stdin);
	*stdin = oldstdin;
	close(fd[1]);

	printf("%s\n", buf);

	return 0;
}

int fscanf_test(void)
{
	char str1[10], str2[10], str3[10];
	int year;
	FILE *fp;

	fp = fopen("scanf_test.txt", "w+");
	fputs("We are in 2012", fp);

	rewind(fp);
	fscanf(fp, "%s %s %s %d", str1, str2, str3, &year);

	printf("Read String1 |%s|\n", str1);
	printf("Read String2 |%s|\n", str2);
	printf("Read String3 |%s|\n", str3);
	printf("Read Integer |%d|\n", year);

	fclose(fp);

	return(0);
}

int sscanf_test(void)
{
	int day, year;
	char weekday[20], month[20], dtm[100];

	strcpy(dtm, "Saturday March 25 1989");
	sscanf(dtm, "%s %s %d  %d", weekday, month, &day, &year);

	printf("%s %d, %d = %s\n", month, day, year, weekday);

	return(0);
}

void GetMatchesVscanf(const char *format, ...)
{
	va_list args;

	va_start(args, format);
	vscanf(format, args);
	va_end(args);
}

int vscanf_test(void)
{
	FILE oldstdin;
	char buf[1024];
	int fd[2];

	pipe(fd);
	oldstdin = *stdin;
	*stdin = *fdopen(fd[0], "r");
	write(fd[1], "string12 ", strlen("string12 "));
	GetMatchesVscanf("%s", buf);
	fclose(stdin);
	*stdin = oldstdin;
	close(fd[1]);

	printf("%s\n", buf);

	return 0;
}

void ReadStuff(FILE *stream, const char *format, ...)
{
	va_list args;

	va_start(args, format);
	vfscanf(stream, format, args);
	va_end(args);
}

int vfscanf_test(void)
{
	FILE *pFile;
	int val;
	char str[100];

	pFile = fopen("myfile.txt", "r");

	if (pFile != NULL) {
		ReadStuff(pFile, " %s %d ", str, &val);
		printf("I have read %s and %d", str, val);
		fclose(pFile);
	}

	return 0;
}

void GetMatchesSscanf(const char *str, const char *format, ...)
{
	va_list args;

	va_start(args, format);
	vsscanf(str, format, args);
	va_end(args);
}

int vsscanf_test(void)
{
	int val;
	char buf[100];

	GetMatchesSscanf("99 bottles of beer on the wall", " %d %s ", &val, buf);

	printf("Product: %s\nQuantity: %d\n", buf, val);

	return 0;
}

int main(void)
{
	scanf_test();

	fscanf_test();

	sscanf_test();

	vscanf_test();

	vfscanf_test();

	vsscanf_test();

	return 0;
}
