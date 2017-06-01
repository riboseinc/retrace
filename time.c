#include "common.h"
#include "rtr-time.h"

char
*ctime_r(const time_t *timep, char *buf) {
	real_ctime_r = dlsym(RTLD_NEXT, "ctime_r");
	trace_printf(1, "ctime_r(\"%s\", \"%s\");\n", timep, buf);
	return real_ctime_r(timep, buf);
}

char
*ctime(const time_t *timep) {
	real_ctime = dlsym(RTLD_NEXT, "ctime");
	trace_printf(1, "ctime(\"%s\");\n", timep);
	return real_ctime(timep);
}
