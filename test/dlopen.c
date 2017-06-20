#include <dlfcn.h>
#include <stdio.h>

void test(void)
{
	printf("Test function\n");
}

int main(void)
{
	void *handle;

	handle = dlopen(NULL, RTLD_LAZY);

	printf("Dlopen returned %p\n", handle);
	if (handle) {
		dlsym(handle, "test");

		dlsym(handle, "test1");

		printf("error: %s\n", dlerror());

		dlclose(handle);
	}

	return 0;
}
