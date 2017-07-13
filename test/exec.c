#include <stdlib.h>
#include <unistd.h>

int main(void)
{
	char *env[] = { "VAR1=VALUE", "VAR2=VALUE2",  "TEST=TESTVALUE", NULL};
	char *args[] = {"param1", "param2", "param3", NULL};

	system("/bin/false");

	execl("/bin/shouldntexitinanysystem", "/bin/false", "parameter1", (char *) NULL);

	execv("/bin/shouldntexitinanysystem", args);


	execle("/bin/shouldntexitinanysystem", "/bin/shouldntexitinanysystem", "ARG1", "ARG2", NULL, env);

	execve("/bin/shouldntexitinanysystem", args, env);

	execlp("/bin/shouldntexitinanysystem", "/bin/shouldntexitinanysystem", "ARG1", "ARG2", NULL);

	execvp("/bin/shouldntexitinanysystem", args);

#ifdef __linux__
	execvpe("/bin/shouldntexitinanysystem", args, env);

	fexecve(1, args, env);
#endif

	execl("/bin/false", "/bin/false", (char *) NULL);

	return 0;
}

