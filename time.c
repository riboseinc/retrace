#include "common.h"
#include "time.h"

char
*ctime_r(const time_t *timep, char *buf) {
	real_ctime_r = dlsym(RTLD_NEXT, "ctime_r");
	trace_printf(1, "ctime_r();\n");
	return real_ctime_r(timep, buf);
}

char
*ctime(const time_t *timep) {
	real_ctime = dlsym(RTLD_NEXT, "ctime");
	trace_printf(1, "ctime();\n");
	return real_ctime(timep);
}
