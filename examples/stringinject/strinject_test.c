#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <unistd.h>

#define TEST_FREAD				0
#define TEST_FWRITE				0
#define TEST_SEND				1

#define TEST_NUM				1

#define TEST_SRV_ADDR				"172.16.72.128"
#define TEST_SRV_PORT				80

/*
 * test for fwrite() string injection
 */

static void test_fwrite(void)
{
	int i;

	for (i = 0; i < TEST_NUM; i++) {
		FILE *fp;
		char fpath[128];

		const char buf[] = "AABBCCDDEEFF";

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

		len = fread(read_buf, 1, sizeof(read_buf), fp);
		fclose(fp);

		read_buf[len] = '\0';

		fprintf(stderr, "read buffer is '%s'\n", read_buf);
	}
}

/*
 * test for send() string injection
 */

static void test_send(void)
{
	int sock;
	struct sockaddr_in addr;

	char msg[512] = "AABBCCDD";

	/* create socket */
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		fprintf(stderr, "Couldn't create socket\n");
		return;
	}

	/* connect to server */
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(TEST_SRV_ADDR);
	addr.sin_port = htons(TEST_SRV_PORT);

	if (connect(sock, (struct sockaddr *) &addr, sizeof(struct sockaddr_in)) < 0) {
		fprintf(stderr, "Couldn't connect to %s:%d\n", TEST_SRV_ADDR, TEST_SRV_PORT);
		close(sock);

		return;
	}

	if (send(sock, msg, strlen(msg), 0) <= 0)
		fprintf(stderr, "Couldn't send message to %s:%d\n", TEST_SRV_ADDR, TEST_SRV_PORT);
	else
		fprintf(stderr, "Success to send message to %s:%d\n", TEST_SRV_ADDR, TEST_SRV_PORT);

	close(sock);
}

int main(void)
{
#if TEST_FWRITE
	test_fwrite();
#endif

#if TEST_FREAD
	test_fread();
#endif

#if TEST_SEND
	test_send();
#endif
}
