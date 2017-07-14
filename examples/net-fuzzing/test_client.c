
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <string.h>

#define TEST_COUNT			3

static void test_inet(void)
{
	int ret;
	int sockfd = 0;

	struct addrinfo hints;
	struct addrinfo *result, *rp;

	const char *request = "GET / HTTP/1.1\r\nUser-Agent: Wget/1.17.1 (linux-gnu)\r\nAccept: */*\r\n"
			"Accept-Encoding: identity\r\nHost: www.google.com\r\nConnection: Keep-Alive\r\n\r\n";

	int request_len, nbytes_total = 0;
	char buf[4096];

	/* obtain address(es) matching host/port */
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = 0;
	hints.ai_protocol = 0;

	ret = getaddrinfo("www.google.com", "http", &hints, &result);
	if (ret != 0) {
		fprintf(stderr, "getaddrinfo() failed, %s\n", gai_strerror(ret));
		return;
	}

	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sockfd > 0)
			break;
	}

	/* check socket descriptor */
	if (sockfd < 0) {
		fprintf(stderr, "Could not create socket, %s\n", strerror(errno));
		return;
	}

	ret = connect(sockfd, rp->ai_addr, rp->ai_addrlen);
	if (ret < 0) {
		fprintf(stderr, "connect() failed, %s\n", strerror(errno));
		close(sockfd);

		return;
	}

	request_len = strlen(request);
	while (nbytes_total < request_len) {
		int nbytes_sent;

		nbytes_sent = send(sockfd, request + nbytes_total, request_len - nbytes_total, 0);
		if (nbytes_sent < 0) {
			fprintf(stderr, "send() function has failed.\n");
			close(sockfd);

			return;
		}

		fprintf(stderr, "Sent '%d' bytes\n", nbytes_sent);
		nbytes_total += nbytes_sent;
	}

	close(sockfd);
}

int main(int argc, char *argv[])
{
	int i;

	for (i = 0; i < TEST_COUNT; i++)
		test_inet();

	return 0;
}
