/* http-server.c - only tested on macOS */

#include <sys/types.h>
#include <sys/uio.h>

#include <netdb.h>
#include <netinet/in.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
	struct sockaddr_in serv_addr, addr;
	int sockbind, sockaccept, port, len, n;
	char buf[128];
	char overflow[90];
	char http[256] = "HTTP/1.1 302 Found\nContent-Length: 35\n\n<html><body><p>hi</p></body></html>";

	sockbind = socket(AF_INET, SOCK_STREAM, 0);
	if (sockbind < 0) {
		perror("socket");
		exit(1);
	}

	bzero((char *) &serv_addr, sizeof(serv_addr));
	port = 8080;

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(port);

	if (bind(sockbind, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		perror("bind");
		exit(1);
	}

	listen(sockbind, 5);
	len = sizeof(addr);

	sockaccept = accept(sockbind, (struct sockaddr *)&addr, (unsigned int *)&len);
	if (sockaccept < 0) {
		perror("accept");
		exit(1);
	}

	bzero(buf, sizeof(buf));

	n = read(sockaccept, buf, sizeof(buf) - 1);
	if (n < 0) {
		perror("read");
		exit(1);
	}

	printf("buf[%lu]: %s\n", strlen(buf), buf);

	if (strlen(buf) > sizeof(overflow)) {
		printf("triggering BOF:\n");
		strcpy(overflow, buf);
		/* should not be reached */
		printf("overflow: [%s]\n", overflow);
	}

	n = write(sockaccept, http, strlen(http));
	if (n < 0) {
		perror("write");
		exit(1);
	}

	return(0);
}
