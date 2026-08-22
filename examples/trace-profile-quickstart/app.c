/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * trace-profile-quickstart target (TODO.trace-profile/16): a
 * tiny portable program with a DECLARED and an UNDECLARED file
 * access plus an env read -- exactly the surface the
 * capture -> diff -> jail loop exercises. argv[1] "upgraded"
 * adds a new file access: the drift the diff must catch.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
	/* argv[1]: the directory holding the demo files (absolute,
	 * so the profile/jail see real paths); argv[2]: "upgraded"
	 * adds the drift access.
	 */
	const char *dir = argc > 1 ? argv[1] : ".";
	char path[512];
	FILE *f;
	char buf[64];
	const char *home = getenv("HOME");

	snprintf(path, sizeof(path), "%s/declared.dat", dir);
	f = fopen(path, "rb");                  /* declared */
	if (f != NULL) {
		if (fgets(buf, sizeof(buf), f) != NULL)
			printf("declared: %s", buf);
		fclose(f);
	}

	snprintf(path, sizeof(path), "%s/undeclared.dat", dir);
	f = fopen(path, "rb");                  /* NOT declared */
	if (f != NULL) {
		fclose(f);
		printf("undeclared: read!\n");
	}

	if (argc > 2 && strcmp(argv[2], "upgraded") == 0) {
		snprintf(path, sizeof(path), "%s/new-feature.dat", dir);
		f = fopen(path, "rb");          /* the drift */
		if (f != NULL)
			fclose(f);
		printf("upgraded: new-feature.dat\n");
	}

	printf("home is set: %s\n", home != NULL ? "yes" : "no");
	return 0;
}
