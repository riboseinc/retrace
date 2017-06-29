#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <netinet/in.h>

static int sock = -1;

#define LISTEN_PORT					8081
#define UNIX_SOCK_PATH				"test_unix_socket"

static void test_socksrv_inet(void)
{
	struct sockaddr_in addr;
	int reuse = 1;
	int clientfd;
	char *s = "this\n\t IS a test buffer";

	// create socket
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("sock_srv");
		return;
	}
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse));

	// bind socket
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(LISTEN_PORT);

	if (bind(sock, (struct sockaddr *) &addr, sizeof(struct sockaddr_in)) < 0) {
		perror("sock_srv");
		return;
	}
#if 0
	if (listen(sock, 1) < 0) {
		perror("sock_srv");
		return;
	}

	clientfd = accept(sock, (struct sockaddr *)NULL, NULL);

	if (clientfd > 0) {
		send(clientfd, s, strlen(s), 0);
		sendto(clientfd, s, strlen(s), 0, NULL, 0);

		recv(clientfd, s, strlen(s), 0);
	}
#endif
}

static void test_socksrv_unix(void)
{
	struct sockaddr_un addr;

	// create socket
	sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock < 0) {
		fprintf(stderr, "could not create socket.\n");
		return;
	}

	// bind socket
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, UNIX_SOCK_PATH);

	if (bind(sock, (struct sockaddr *) &addr, sizeof(struct sockaddr_un)) < 0)
		fprintf(stderr, "could not bind socket.\n");
}

int main(void)
{
	test_socksrv_inet();
	test_socksrv_unix();

	/* close socket */
	if (sock > 0)
		close(sock);

	return 0;
}
