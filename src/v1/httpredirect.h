#ifndef __RETRACE_HTTP_REDIRECT_H__
#define __RETRACE_HTTP_REDIRECT_H__

#include <sys/socket.h>

#define RTR_HTTP_SNIFFLEN 1024

struct rtr_http_redirect_info {
	char sniffbuf[RTR_HTTP_SNIFFLEN + 1];
	off_t sniffoff;
	const char *dir;
	int filefd, sniffing;
	size_t remainder;
};

struct rtr_http_redirect_info *rtr_setup_http_redirect(const struct sockaddr *addr);
void rtr_http_sniff_request(int fd, const void *buf, size_t len);
size_t rtr_http_redirect_response(int fd, void *buf, size_t len, int flags);

#endif
