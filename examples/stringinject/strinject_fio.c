#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_FREAD				0
#define TEST_FWRITE				1

#define TEST_NUM				5

/*
 * test for fwrite() string injection
 */

static void test_fwrite(void)
{
	int i;

	for (i = 0; i < TEST_NUM; i++) {
		FILE *fp;
		char fpath[128];

		const char buf[] = "abcdefghijklmnopqrstuvwxyz";

		/* set file path */
		snprintf(fpath, sizeof(fpath), "/tmp/strinject_fwrite_test_%d", i);

		fp = fopen(fpath, "w");
		if (!fp)
			break;

		fwrite(buf, 1, strlen(buf), fp);
		fclose(fp);
	}
}

/*
 * test for fread() string injection
 */

static void test_fread(void)
{
	FILE *fp;
	int i;

	const char buf[] = "abcdefghijklmnopqrstuvwxyz";

	/* write test string into file */
	fp = fopen("/tmp/strinject_fread_test", "w");
	if (!fp)
		return;

	fwrite(buf, 1, strlen(buf), fp);
	fclose(fp);

	for (i = 0; i < TEST_NUM; i++) {
		char read_buf[512];
		size_t len = strlen(buf);

		fp = fopen("/tmp/strinject_fread_test", "r");
		if (!fp)
			break;

		len = fread(read_buf, 1, len, fp);
		fclose(fp);

		read_buf[len] = '\0';

		fprintf(stderr, "read buffer is '%s'\n", read_buf);
	}
}

int main(void)
{
#if TEST_FWRITE
	test_fwrite();
#endif

#if TEST_FREAD
	test_fread();
#endif
}
