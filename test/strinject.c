#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <err.h>
#include <sys/socket.h>

int main(int argc, const char **argv)
{
	const char *in = argv[1];
	const char *out = argv[2];
	int fd[2];
	char buf[1024];
	FILE *fin, *fout;

	/* test read/write */
	printf("writing: %s\nreading: %s\n", in, out);
	if (pipe(fd))
		err(EXIT_FAILURE, "pipe");

	if (write(fd[1], in, strlen(in)) == -1)
		err(EXIT_FAILURE, "failed write");

	close(fd[1]);

	if (read(fd[0], buf, sizeof(buf)) == -1)
		err(EXIT_FAILURE, "failed read");

	close(fd[0]);

	if (strncmp(buf, out, strlen(out)))
		err(EXIT_FAILURE, "failed write/read");

	/* test fread/fwrite */
	if (pipe(fd))
		err(EXIT_FAILURE, "pipe");

	fin = fdopen(fd[0], "r");
	fout = fdopen(fd[1], "w");
	if (!fin || !fout)
		err(EXIT_FAILURE, "fdopen");

	if (fwrite(in, 1, strlen(in), fout) == -1)
		err(EXIT_FAILURE, "failed fwrite");

	fclose(fout);

	if (fread(buf, 1, sizeof(buf), fin) == -1)
		err(EXIT_FAILURE, "failed fread");

	fclose(fin);

	if (strncmp(buf, out, strlen(out)))
		err(EXIT_FAILURE, "failed fwrite/fread");

	/* test send/recv */
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, fd))
		err(EXIT_FAILURE, "pipe");

	if (send(fd[1], in, strlen(in), 0) == -1)
		err(EXIT_FAILURE, "failed send");

	close(fd[1]);

	if (recv(fd[0], buf, sizeof(buf), 0) == -1)
		err(EXIT_FAILURE, "failed recv");

	close(fd[0]);

	if (strncmp(buf, out, strlen(out)))
		err(EXIT_FAILURE, "failed send/recv");

	return EXIT_SUCCESS;
}
