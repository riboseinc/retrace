#include "common.h"
#include "file.h"
#include "popen.h"

FILE *
popen(const char *command, const char *type)
{
	FILE *ret;

	real_popen = dlsym(RTLD_NEXT, "popen");
	real_fileno = dlsym(RTLD_NEXT, "fileno");

	ret = real_popen(command, type);
	trace_printf(1, "popen(\"%s\", \"%s\"); [%d]\n", command, type, real_fileno(ret));

	return ret;
}

int
pclose(FILE *stream)
{
	int ret;

	real_pclose = dlsym(RTLD_NEXT, "pclose");
	real_fileno = dlsym(RTLD_NEXT, "fileno");

	ret = real_pclose(stream);
	trace_printf(1, "pclose(%d); [%d]\n", real_fileno(stream), ret);

	return ret;
}
