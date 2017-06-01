#include "common.h"
#include "sock.h"

int
connect(int fd, const struct sockaddr *address, socklen_t len) {
	real_connect = dlsym(RTLD_NEXT, "connect");
	int i;
	int port;

	trace_printf(1, "connect(%d, \"\", %zu); [addr: ", fd, len);

	for(i = 2; i < 6; i++) {
		trace_printf(0, "%hu",
		    (unsigned short)address->sa_data[i]&0xFF);

		if (i < 5)
			trace_printf(0, ".");
	}

	port = (256 * address->sa_data[0]) + address->sa_data[1];

//	trace_printf(0, "[%hu][%hu]]\n", (unsigned short)address->sa_data[0]&0xFF,(unsigned short)address->sa_data[1]&0xFF);
	trace_printf(0, ":{%d}]\n", port);

	return real_connect(fd, address, len);
}

int
bind(int fd, struct sockaddr *address, socklen_t len) {
	real_bind = dlsym(RTLD_NEXT, "bind");
	int i;

	trace_printf(1, "bind(%d, \"\", %zu); [addr: %s", fd, len);

	for(i = 2; i < 6; i++) {
		trace_printf(0, "%hu", (unsigned short)address->sa_data[i]&0xFF);

		if (i < 5)
			trace_printf(0, ".");
	}

	trace_printf(0, "]\n");

	return real_bind(fd, address, len);
}

int
atoi(const char *str) {
	real_atoi = dlsym(RTLD_NEXT, "atoi");
	trace_printf(1, "atoi(%s);\n", str);
	return real_atoi(str);
}
