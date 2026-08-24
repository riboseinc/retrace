/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Jail-denial target for the otlp-c Wave C integration test
 * (TODO.trace-profile/32): opens the path given as argv[1]
 * (denied by the test's sandbox config) and a real system path
 * (allowed). The integration test asserts a /v1/logs POST
 * carrying the denial while /v1/traces still streams the spans.
 */

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	const char *denied = argc > 1 ? argv[1] : "/etc/hosts";
	const char *allowed = argc > 2 ? argv[2] : "/etc/protocols";
	int fd = open(denied, O_RDONLY);

	printf("denied-open rc=%d\n", fd);
	if (fd >= 0)
		close(fd);

	fd = open(allowed, O_RDONLY);
	printf("allowed-open rc=%d\n", fd);
	if (fd >= 0)
		close(fd);
	return 0;
}
