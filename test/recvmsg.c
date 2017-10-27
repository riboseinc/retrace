#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#define SERVER_ADDRESS				"127.0.0.1"
#define SERVER_PORT					7005

#define TEST_MESSAGE				"This is test message!"

/* option structure */
struct prog_opt {
	int mode;						/* 0 - client, 1 - server */

	int srv_port;
	int proto;						/* 0 - UDP, 1 - TCP */

	int sock;
	struct sockaddr_in addr;
};

/*
 * print usage message
 */

static void usage(int exit_code)
{
	fprintf(stderr, "recvmsg [-m client|server] [-i port] [-p udp|tdp]\n");
	exit(exit_code);
}

/*
 * test recvmsg()
 */

static void test_sendmsg(struct prog_opt *opt)
{
	int sock;
	struct sockaddr_in addr;

	struct msghdr msg;
	struct iovec iov[1];

	ssize_t sent_len;

	/* if protocol is TCP, then try to connect */
	if (opt->proto == 1 &&
		connect(opt->sock, (const struct sockaddr *) &opt->addr, sizeof(opt->addr)) != 0) {
		fprintf(stderr, "Couldn't connect to remote %s:%d\n", SERVER_ADDRESS, opt->srv_port);
		return;
	}

	/* set message data */
	iov[0].iov_base = TEST_MESSAGE;
	iov[0].iov_len = strlen(TEST_MESSAGE);

	memset(&msg, 0, sizeof(msg));
	if (opt->proto == 0) {
		msg.msg_name = &opt->addr;
		msg.msg_namelen = sizeof(struct sockaddr_in);
	}
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;

	/* send message */
	sent_len = sendmsg(opt->sock, &msg, 0);
	if (sent_len < 0)
		fprintf(stderr, "sendmsg() failed(err:%d)\n", errno);
	else
		fprintf(stderr, "sendmsg() success(sent_len:%d)\n", sent_len);
}

/*
 * test recvmsg()
 */

static void test_recvmsg(struct prog_opt *opt)
{
	struct iovec iov[1];
	char buf[512];

	struct msghdr msg;
	ssize_t recv_len;

	struct sockaddr_in addr;

	int clnt_sock;

	/* bind on address */
	if (bind(opt->sock, (const struct sockaddr *) &opt->addr, sizeof(struct sockaddr_in)) != 0) {
		fprintf(stderr, "bind() failed(err:%d)\n", errno);
		return;
	}

	/* if protocol is TCP, then listen accept the connection */
	if (opt->proto == 1) {
		struct sockaddr_in clnt_addr;
		socklen_t addr_len = sizeof(struct sockaddr_in);

		/* listen on local */
		if (listen(opt->sock, 5) != 0) {
			fprintf(stderr, "listen() failed(err:%d)\n", errno);
			return;
		}

		/* accept the connection */
		clnt_sock = accept(opt->sock, (struct sockaddr *) &clnt_addr, &addr_len);
		if (clnt_sock < 0) {
			fprintf(stderr, "accept() failed(err:%d)\n", errno);
			return;
		}

		fprintf(stderr, "accepted the connection from client\n");
	}

	/* receive buffer */
	iov[0].iov_base = buf;
	iov[0].iov_len = sizeof(buf);

	memset(&msg, 0, sizeof(msg));
	if (opt->proto == 0) {
		msg.msg_name = &addr;
		msg.msg_namelen = sizeof(addr);
	}
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;

	recv_len = recvmsg(opt->proto == 0 ? opt->sock : clnt_sock, &msg, 0);
	if (recv_len < 0)
		fprintf(stderr, "recvmsg() failed(err:%d)\n", errno);
	else
		fprintf(stderr, "recvmsg() success(recv_len:%d)\n", recv_len);
}

/*
 * main function
 */

int main(int argc, char *argv[])
{
	struct prog_opt opt;

	/* initialize option */
	memset(&opt, 0, sizeof(opt));

	/* parse arguments */
	if (argc > 1) {
		int op;

		while ((op = getopt(argc, argv, "m:i:p:h")) != -1) {
			switch (op) {
			case 'm':
				if (strcmp(optarg, "client") == 0)
					opt.mode = 0;
				else if (strcmp(optarg, "server") == 0)
					opt.mode = 1;
				else
					usage(-1);

				break;

			case 'i':
				opt.srv_port = atoi(optarg);
				break;

			case 'p':
				if (strcmp(optarg, "udp") == 0)
					opt.proto = 0;
				else if (strcmp(optarg, "tcp") == 0)
					opt.proto = 1;
				else
					usage(-1);

				break;

			case 'h':
				usage(0);
				break;

			default:
				usage(-1);
				break;
			}
		}
	}

	/* check port number is specified */
	if (opt.srv_port == 0)
		opt.srv_port = SERVER_PORT;

	/* create socket */
	if (opt.proto == 0)
		opt.sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	else
		opt.sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (opt.sock < 0) {
		fprintf(stderr, "Couldn't create UDP socket(err:%d)\n", errno);
		exit(-1);
	}

	/* set socket address of server to connect */
	memset(&opt.addr, 0, sizeof(opt.addr));
	opt.addr.sin_family = AF_INET;
	inet_pton(AF_INET, SERVER_ADDRESS, &opt.addr.sin_addr);
	opt.addr.sin_port = htons(opt.srv_port);

	/* run main procedure */
	if (opt.mode == 0)
		test_sendmsg(&opt);
	else
		test_recvmsg(&opt);

	/* close socket */
	close(opt.sock);

	return 0;
}
