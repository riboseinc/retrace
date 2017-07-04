#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define FUZZING_TEST_COUNT			1000

static void test_mmap_file(const char *fpath)
{
	struct stat st;
	int fd;

	void *p;

	/* get file size */
	if (stat(fpath, &st) != 0) {
		fprintf(stderr, "Could not get stat of file '%s'\n", fpath);
		exit(-1);
	}

	if (st.st_size == 0) {
		fprintf(stderr, "The file size is 0\n");
		exit(-1);
	}

	/* open file */
	fd = open(fpath, O_RDONLY, 0);
	if (fd < 0) {
		fprintf(stderr, "Could not open file '%s' for reading mode.\n", fpath);
		exit(-1);
	}

	/* map file buffer */
	p = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (p == MAP_FAILED)
		fprintf(stderr, "Mapping has failed.\n");
	else {
		fprintf(stderr, "Mapping has been succeeded.\n");

		if (munmap(p, st.st_size) < 0)
			fprintf(stderr, "munmap() has failed.\n");
	}

	/* close file */
	close(fd);
}

static void test_mmap_anon(void)
{
	void *p;

	p = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	if (p != MAP_FAILED)
		munmap(p, sizeof(int));
}

int main(int argc, char *argv[])
{
	int i;

	/* check argument*/
	if (argc != 2) {
		fprintf(stderr, "Usage: mmap [filepath]\n");
		exit(-1);
	}

	test_mmap_file(argv[1]);

	for (i = 0; i < FUZZING_TEST_COUNT; i++)
		test_mmap_anon();

	return 0;
}
