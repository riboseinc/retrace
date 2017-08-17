#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


int main(void)
{
	char buf[1024];
	int fd;
	char *ret;
	FILE *ret_file;

	strcpy(buf, "/tmp/retrace-XXXXXX");
	mktemp(buf);

	strcpy(buf, "/tmp/retrace-XXXXXX");
	mkdtemp(buf);

	strcpy(buf, "/tmp/retrace-XXXXXX");
	fd = mkstemp(buf);
	if (fd > 0)
		close(fd);

	strcpy(buf, "/tmp/retrace-XXXXXX-42");
	fd = mkstemps(buf, 3);
	if (fd > 0)
		close(fd);

#ifndef __APPLE__
	strcpy(buf, "/tmp/retrace-XXXXXX");
	fd = mkostemp(buf, O_APPEND);
	if (fd > 0)
		close(fd);

	strcpy(buf, "/tmp/retrace-XXXXXX-42");
	fd = mkostemps(buf, 3, O_APPEND);
	if (fd > 0)
		close(fd);

	ret = tempnam("/tmp/", "retrace");
#endif


	ret_file = tmpfile();
	if (ret_file)
		fclose(ret_file);

#ifndef __APPLE__
	tmpnam(NULL);
#endif

	return 0;
}
