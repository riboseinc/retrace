#include "common.h"
#include "exec.h"

int
system(const char *command) {
	real_system = dlsym(RTLD_NEXT, "system");
	trace_printf(1, "system(\"%s\");\n", command);
	return real_system(command);
}

int
execve(const char *path, char *const argv[], char *const envp[]) {
	real_execve = dlsym(RTLD_NEXT, "execve");

	int i;

	// XXX: 16, this needs to be calculated runtime...
	for(i = 0; i < 16 + 2; i++) {
		trace_printf(1, "execve(\"%s\"); [arg %d]", argv[i], i);

		if (argv[i] == NULL)
			return real_execve(path, argv, envp);
	}

	trace_printf(0, "\n");

	return real_execve(path, argv, envp);
	return(0);
}
