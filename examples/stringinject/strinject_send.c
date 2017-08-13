#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <arpa/inet.h>

int main(void)
{
	int sock;
	struct sockaddr_in server;
	char message[] = "testtesttest";

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == -1) {
		printf("Could not create socket");
		exit(1);
	}

	printf("socket created\n");

	server.sin_addr.s_addr = inet_addr("127.0.0.1");
	server.sin_family = AF_INET;
	server.sin_port = htons(80);
	if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
		printf("connect failed\n");
		exit(1);
	}

	printf("connected\n");
	if (send(sock, message, strlen(message), 0) < 0) {
		printf("send failed\n");
		return 1;
	}

	printf("sent\n");
	close(sock);

	return(0);
}
