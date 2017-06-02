#include "common.h"
#include "env.h"

int
unsetenv(const char *name) {
	real_unsetenv = dlsym(RTLD_NEXT, "unsetenv");
	trace_printf(1, "unsetenv(\"%s\");\n", name);
	return real_unsetenv(name);
}

int
putenv(char *string) {
	real_putenv = dlsym(RTLD_NEXT, "putenv");
	trace_printf(1, "putenv(\"%s\");\n", string);
	return real_putenv(string);
}

char
*getenv(const char *envname) {
	real_getenv = dlsym(RTLD_NEXT, "getenv");
	char *env = real_getenv(envname);
	trace_printf(1, "getenv(\"%s\"); [%s]\n", envname, env);
	return real_getenv(envname);
}
