/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * The "packaged app" for the escape-hunting demo (recipe 33,
 * TODO.windows/04 examples parity). It behaves like a
 * virtualized payload: everything it opens SHOULD live under
 * the VFS prefix (argv[1], absolute), and the VFS materialized
 * only the declared files (see inside.json). Reading
 * leaked.dat is the escape -- a host touch the VFS never saw.
 *
 * Uses open(2) rather than fopen(3): current macOS SDKs remap
 * fopen to fopen$DARWIN_EXTSN in optimized builds, which the
 * interposition table does not export (recorded in
 * TODO.windows/05); open's prototype also derefs the path
 * pointer, which is what the correlator consumes.
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#define rc_open(p) _open(p, _O_RDONLY)
#define rc_close(fd) _close(fd)
#else
#include <unistd.h>
#define rc_open(p) open(p, O_RDONLY)
#define rc_close(fd) close(fd)
#endif

static void touch(const char *prefix, const char *name)
{
	char path[1024];
	int fd;

	snprintf(path, sizeof(path), "%s/%s", prefix, name);
	fd = rc_open(path);
	if (fd < 0) {
		printf("miss  %s\n", path);
		return;
	}
	printf("read  %s\n", path);
	rc_close(fd);
}

int main(int argc, char **argv)
{
	const char *prefix;

	if (argc != 2) {
		fprintf(stderr, "usage: escape-demo <vfs-prefix-abs-path>\n");
		return 2;
	}
	prefix = argv[1];

	/* The declared tree (inside.json): served by the VFS. */
	touch(prefix, "entry.dat");
	touch(prefix, "settings.dat");

	/*
	 * The undeclared read: the payload reaches a host file the
	 * VFS never materialized -- the escape retrace-correlate
	 * reports.
	 */
	touch(prefix, "leaked.dat");

	return 0;
}
