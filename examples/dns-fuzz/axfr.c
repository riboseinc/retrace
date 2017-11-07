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

/* axfr.c zone transfer utility
 * no output, returns 0 or 1
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define QTYPE_AXFR	0xfc
#define QCLASS_AXFR	0x01

/* axfr query header */
struct axfr_query {
	unsigned short id;

	unsigned short rd:1;
	unsigned short tc:1;
	unsigned short aa:1;
	unsigned short opcode:4;
	unsigned short qr:1;

	unsigned short rcode:4;
	unsigned short cd:1;
	unsigned short ad:1;
	unsigned short z:1;
	unsigned short ra:1;

	unsigned short qdcount;
	unsigned short ancount;
	unsigned short nscount;
	unsigned short arcount;
};

/*
 * build qname
 */

static void build_qname(const char *domain, unsigned char **qname, int *qname_len)
{
	const char *p1, *p2 = domain;

	unsigned char *q = NULL;
	int i, q_len = 0;

	while (1) {
		int len;

		p1 = strchr(p2, '.');

		/* allocate buffer */
		if (!p1)
			len = strlen(p2);
		else
			len = p1 - p2;

		q = realloc(q, q_len + len + 1);

		/* set buffer for each domain */
		q[q_len] = (unsigned char) len;
		memcpy(&q[q_len + 1], p2, len);

		q_len += len + 1;

		if (!p1)
			break;

		p2 = p1 + 1;
	}

	q = realloc(q, q_len + 1);
	q[q_len] = 0x00;

	*qname = q;
	*qname_len = q_len + 1;
}

/*
 * build axfr query
 */

static void build_axfr_request(const char *domain, unsigned char **query, int *query_len)
{
	struct axfr_query query_hdr;

	unsigned char *qname;
	int qname_len;

	unsigned short qtype;
	unsigned short qclass;

	unsigned char *q;
	int q_len = 0;

	/* init query header */
	memset(&query_hdr, 0, sizeof(struct axfr_query));

	query_hdr.id = random() % 0xFFFF;
	query_hdr.qdcount = htons(1);

	/* set qname */
	build_qname(domain, &qname, &qname_len);

	/* set qtype and class */
	qtype = htons(QTYPE_AXFR);
	qclass = htons(QCLASS_AXFR);

	/* allocate and set buffer */
	q = (unsigned char *) malloc(sizeof(struct axfr_query) +
					qname_len + 2 * sizeof(unsigned short));

	memcpy(q, &query_hdr, sizeof(struct axfr_query));
	q_len += sizeof(struct axfr_query);

	memcpy(q + q_len, qname, qname_len);
	q_len += qname_len;

	memcpy(q + q_len, &qtype, sizeof(unsigned short));
	q_len += sizeof(unsigned short);

	memcpy(q + q_len, &qclass, sizeof(unsigned short));
	q_len += sizeof(unsigned short);

	/* free qname buffer */
	free(qname);

	*query = q;
	*query_len = q_len;
}

/*
 * main function
 */

int main(int argc, char *argv[])
{
	int sock;
	struct sockaddr_in addr;

	unsigned char *query;
	int query_len;

	unsigned short len;

	unsigned char resp[512];

	/* check arguments */
	if (argc != 3) {
		fprintf(stderr, "Usage: %s <nameserver> <domain>\n", argv[0]);
		exit(-1);
	}

	/* connect to DNS server */
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		fprintf(stderr, "socket() failed.(err:%d)\n", errno);
		return -1;
	}

	/* connect to DNS server */
	memset(&addr, 0, sizeof(addr));

	addr.sin_family = AF_INET;
	if (inet_pton(AF_INET, argv[1], &addr.sin_addr) < 1) {
		fprintf(stderr, "inet_pton(%s) failed.(err:%d)\n", argv[1], errno);
		close(sock);

		return -1;
	}
	addr.sin_port = htons(53);

	if (connect(sock, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) != 0) {
		fprintf(stderr, "connect() failed(err:%d)\n", errno);
		close(sock);

		return -1;
	}

	/* build DNS forward request */
	build_axfr_request(argv[2], &query, &query_len);

	/* send length of query */
	len = htons(query_len);
	if (write(sock, (void *) &len, 2) == -1) {
		fprintf(stderr, "write() failed(err:%d)\n", errno);
		close(sock);

		return -1;
	}

	/* send query to DNS server */
	if (sendto(sock, (void *) query, query_len, 0, NULL, 0) < 0) {
		fprintf(stderr, "sendto() failed(err:%d)\n", errno);
		close(sock);

		return -1;
	}

	/* receive response from DNS server */
	if (recvfrom(sock, resp, sizeof(resp), 0, NULL, 0) < 0) {
		fprintf(stderr, "recvfrom() failed(err:%d)\n", errno);
		close(sock);

		return -1;
	}

	/* close socket */
	close(sock);

	return 0;
}
