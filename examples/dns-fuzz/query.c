// this code originates from: http://www.binarytides.com/dns-query-code-in-c-with-linux-sockets/
// it's slightly modified for use in the dns-fuzzing example for Retrace

// DNS Query Program on Linux
// Author : Silver Moon (m00n.silv3r@gmail.com)
// Dated : 29/4/2009

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

#define T_A 1     // Ipv4 address
#define T_NS 2    // Nameserver
#define T_CNAME 5 // canonical name
#define T_SOA 6   // start of authority zone
#define T_PTR 12  // domain name pointer
#define T_MX 15   // Mail server
#define T_PORT 53 // Name server port

#define BUFSIZE 100

// DNS header structure
struct DNS_HEADER {
	unsigned short id;         // identification number

	unsigned char rd:1;        // recursion desired
	unsigned char tc:1;        // truncated message
	unsigned char aa:1;        // authoritive answer
	unsigned char opcode:4;    // purpose of message
	unsigned char qr:1;        // query/response flag

	unsigned char rcode:4;     // response code
	unsigned char cd:1;        // checking disabled
	unsigned char ad:1;        // authenticated data
	unsigned char z:1;         // its z! reserved
	unsigned char ra:1;        // recursion available

	unsigned short q_count;	   // number of question entries
	unsigned short ans_count;  // number of answer entries
	unsigned short auth_count; // number of authority entries
	unsigned short add_count;  // number of resource entries
};

// Constant sized fields of query structure
struct QUESTION {
	unsigned short qtype;
	unsigned short qclass;
};

// Constant sized fields of the resource record structure
#pragma pack(push, 1)
struct R_DATA {
	unsigned short type;
	unsigned short _class;
	unsigned int ttl;
	unsigned short data_len;
};
#pragma pack(pop)

// Pointers to resource record contents
struct RES_RECORD {
	unsigned char *name;
	struct R_DATA *resource;
	unsigned char *rdata;
};

// Structure of a Query
typedef struct {
	unsigned char *name;
	struct QUESTION *ques;
} QUERY;

unsigned char
*ReadName(unsigned char *reader, unsigned char *buffer, int *count)
{
	unsigned char *name;
	unsigned int p = 0, jumped = 0, offset;
	int i, j;
	size_t len;

	*count = 1;
	name = (unsigned char *)malloc(256);
	name[0] = '\0';

	// read the names in 3www6google3com format
	while (*reader != 0) {
		if (*reader >= 192) {
			offset = (*reader) * 256 + *(reader + 1) - 49152; // 49152 = 11000000 00000000
			reader = buffer + offset - 1;
			jumped = 1; // we have jumped to another location so counting won't go up
		} else {
			name[p++] = *reader;
		}

		reader = reader + 1;

		if (jumped == 0)
			*count = *count + 1; // if we haven't jumped to another location then we can count up
	}

	name[p] = '\0';

	if (jumped == 1)
		*count = *count + 1;

	// now convert 3www6google3com0 to www.google.com
	len = strlen((const char *)name);
	for (i = 0; i < len; i++) {
		p = name[i];

		for (j = 0; j < (int)p; j++) {
			name[i] = name[i + 1];
			i = i + 1;
		}

		name[i] = '.';
	}
	name[i - 1] = '\0'; // remove the last dot

	return(name);
}

void
ChangetoDnsNameFormat(unsigned char *dns, char *host)
{
	int lock = 0, i;

	strcat(host, ".");

	for (i = 0; i < strlen((char *)host); i++) {
		if (host[i] != '.')
			continue;

		*dns++ = i - lock;

		for (; lock < i; lock++)
			*dns++ = host[lock];

		lock++;	// or lock=i+1;
	}

	*dns++ = '\0';
}

/*
 * Perform a DNS query by sending a packet
 */
void
ngethostbyname(char *host, int query_type, char *server)
{
	unsigned char buf[65536], *qname, *reader;
	int i, j, stop, s;

	struct sockaddr_in a;

	struct RES_RECORD answers[20], auth[20], addit[20]; // the replies from the DNS server
	struct sockaddr_in dest;

	struct DNS_HEADER *dns = NULL;
	struct QUESTION *qinfo = NULL;

	s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP); // UDP packet for DNS queries
	if (s == -1) {
		perror("socket failed");
		exit(1);
	}

	dest.sin_family = AF_INET;
	dest.sin_port = htons(T_PORT);
	dest.sin_addr.s_addr = inet_addr(server); // DNS server

	// Set the DNS structure to standard queries
	dns = (struct DNS_HEADER *)&buf;

	dns->id = (unsigned short)htons(getpid());
	dns->qr = 0;     // This is a query
	dns->opcode = 0; // This is a standard query
	dns->aa = 0;     // Not Authoritative
	dns->tc = 0;     // This message is not truncated
	dns->rd = 1;     // Recursion Desired
	dns->ra = 0;     // Recursion not available
	dns->z = 0;
	dns->ad = 0;
	dns->cd = 0;
	dns->rcode = 0;
	dns->q_count = htons(1); // we have only 1 question
	dns->ans_count = 0;
	dns->auth_count = 0;
	dns->add_count = 0;

	// point to the query portion
	qname = (unsigned char *)&buf[sizeof(struct DNS_HEADER)];

	ChangetoDnsNameFormat(qname, host);
	qinfo = (struct QUESTION *)&buf[sizeof(struct DNS_HEADER) + (strlen((const char *)qname) + 1)];	// fill it
	qinfo->qtype = htons(query_type); // type of the query, A, MX, CNAME, NS etc
	qinfo->qclass = htons(1);

	if (sendto(s, (char *)buf,
	     sizeof(struct DNS_HEADER) + (strlen((const char *)qname) + 1) +
	     sizeof(struct QUESTION), 0, (struct sockaddr *)&dest,
	     sizeof(dest)) == -1) {
		perror("sendto failed");
		exit(1);
	}

	i = sizeof(dest);

	if (recvfrom(s, (char *)buf, 65536, 0, (struct sockaddr *)&dest, (socklen_t *) &i) < 0) {
		perror("recvfrom failed");
		exit(1);
	}

	dns = (struct DNS_HEADER *)buf;

	// move ahead of the dns header and the query field
	reader = &buf[sizeof(struct DNS_HEADER) + (strlen((const char *)qname) + 1) +
		 sizeof(struct QUESTION)];

	// Start reading answers
	stop = 0;

	for (i = 0; i < ntohs(dns->ans_count); i++) {
		answers[i].name = ReadName(reader, buf, &stop);
		reader = reader + stop;

		answers[i].resource = (struct R_DATA *)(reader);
		reader = reader + sizeof(struct R_DATA);

		if (ntohs(answers[i].resource->type) == 1) {
			answers[i].rdata = (unsigned char *)malloc(ntohs(answers[i].resource->data_len));

			for (j = 0; j < ntohs(answers[i].resource->data_len); j++)
				answers[i].rdata[j] = reader[j];

			answers[i].rdata[ntohs(answers[i].resource->data_len)] = '\0';

			reader = reader + ntohs(answers[i].resource->data_len);
		} else {
			answers[i].rdata = ReadName(reader, buf, &stop);
			reader = reader + stop;
		}
	}

	// read authorities
	for (i = 0; i < ntohs(dns->auth_count); i++) {
		auth[i].name = ReadName(reader, buf, &stop);
		reader += stop;

		auth[i].resource = (struct R_DATA *)(reader);
		reader += sizeof(struct R_DATA);

		auth[i].rdata = ReadName(reader, buf, &stop);
		reader += stop;
	}

	// read additional
	for (i = 0; i < ntohs(dns->add_count); i++) {
		addit[i].name = ReadName(reader, buf, &stop);
		reader += stop;

		addit[i].resource = (struct R_DATA *)(reader);
		reader += sizeof(struct R_DATA);

		if (ntohs(addit[i].resource->type) == 1) {
			addit[i].rdata = (unsigned char *)malloc(ntohs(addit[i].resource->data_len));

			for (j = 0; j < ntohs(addit[i].resource->data_len); j++)
				addit[i].rdata[j] = reader[j];

			addit[i].rdata[ntohs(addit[i].resource->data_len)] = '\0';
			reader += ntohs(addit[i].resource->data_len);
		} else {
			addit[i].rdata = ReadName(reader, buf, &stop);
			reader += stop;
		}
	}

	// print answers
	for (i = 0; i < ntohs(dns->ans_count); i++) {
		printf("%s ", answers[i].name);

		// IPv4 address
		if (ntohs(answers[i].resource->type) == T_A) {
			long *p;

			p = (long *)answers[i].rdata;
			a.sin_addr.s_addr = (*p);
			printf("has IPv4 address: %s", inet_ntoa(a.sin_addr));
		}

		if (ntohs(answers[i].resource->type) == 5)
			printf("has alias name: %s", answers[i].rdata);

		printf("\n");
	}

	// print authorities
	for (i = 0; i < ntohs(dns->auth_count); i++) {
		printf("%s ", auth[i].name);

		if (ntohs(auth[i].resource->type) == 2)
			printf("has nameserver: %s", auth[i].rdata);

		printf("\n");
	}

	// print additional resource records
	for (i = 0; i < ntohs(dns->add_count); i++) {
		printf("%s ", addit[i].name);

		if (ntohs(addit[i].resource->type) == 1) {
			long *p;

			p = (long *)addit[i].rdata;
			a.sin_addr.s_addr = (*p);
			printf("has IPv4 address: %s", inet_ntoa(a.sin_addr));
		}

		printf("\n");
	}
}

int
main(int argc, char *argv[])
{
	char hostname[BUFSIZE];
	char server[BUFSIZE];

	if (argc < 3) {
		printf("usage: %s <nameserver> <hostname>\n", argv[0]);
		exit(1);
	}

	snprintf(server, sizeof(server), "%s", argv[1]);
	snprintf(hostname, sizeof(hostname), "%s", argv[2]);

	ngethostbyname(hostname, T_A, server);

	return(0);
}
