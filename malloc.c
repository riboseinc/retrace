#include "common.h"
#include "malloc.h"

void
free(void *mem)
{
	real_free = dlsym(RTLD_NEXT, "free");
	trace_printf(1, "free(%p);\n", mem);
	real_free(mem);
}

void *
malloc(size_t bytes)
{
	void *p;

	real_malloc = dlsym(RTLD_NEXT, "malloc");
	p = real_malloc(bytes);
	trace_printf(1, "malloc(%d); [%p]\n", bytes, p);

	return p;
}
