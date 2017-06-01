#include "common.h"
#include "sock.h"

int
connect(int fd, const struct sockaddr *address, socklen_t len) {
	real_connect = dlsym(RTLD_NEXT, "connect");

	trace_printf(1, "connect(%d, \"%hu.%hu.%hu.%hu:%hu\", %zu);\n",
	    fd,
	    (unsigned short)address->sa_data[2]&0xFF,
	    (unsigned short)address->sa_data[3]&0xFF,
	    (unsigned short)address->sa_data[4]&0xFF,
	    (unsigned short)address->sa_data[5]&0xFF,
	    (256 * address->sa_data[0]) + address->sa_data[1],
	    len);

	return real_connect(fd, address, len);
}

int
bind(int fd, const struct sockaddr *address, socklen_t len) {
	real_bind = dlsym(RTLD_NEXT, "bind");

	trace_printf(1, "bind(%d, \"%hu.%hu.%hu.%hu:%hu\", %zu);\n",
	    fd,
	    (unsigned short)address->sa_data[2]&0xFF,
	    (unsigned short)address->sa_data[3]&0xFF,
	    (unsigned short)address->sa_data[4]&0xFF,
	    (unsigned short)address->sa_data[5]&0xFF,
	    (256 * address->sa_data[0]) + address->sa_data[1],
	    len);

	return real_bind(fd, address, len);
}

int
atoi(const char *str) {
	real_atoi = dlsym(RTLD_NEXT, "atoi");
	trace_printf(1, "atoi(%s);\n", str);
	return real_atoi(str);
}
