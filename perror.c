#include "common.h"
#include "perror.h"

void
perror(const char *s) {
	real_perror = dlsym(RTLD_NEXT, "perror");
	trace_printf(1, "perror(\"%s\");\n", s);
	return real_perror(s);
}
