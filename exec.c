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

	trace_printf(1, "execve(\"%s\"", argv[0]);

	for (i = 0;; i++) {
		if (argv[i] == NULL)
			break;

		trace_printf(0, ", \"%s\"", argv[i]);
	}

	trace_printf(0, ", envp);\n");

	trace_printf(1, "char *envp[]=\n");
	trace_printf(1, "{\n");

	for (i = 0;; i++) {
		if (envp[i] == NULL)
			break;

		trace_printf(1, "\t\"%s\",\n", envp[i]);
	}
	trace_printf(1, "\t0\n");
	trace_printf(1, "}\n");

	return real_execve(path, argv, envp);

	return(0);
}
