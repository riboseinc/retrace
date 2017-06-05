#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <dlfcn.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <cmocka.h>

#include "char.h"
#include "common.h"
#include "env.h"
#include "exec.h"
#include "exit.h"
#include "file.h"
#include "id.h"
#include "perror.h"
#include "sock.h"
#include "str.h"
#include "rtr-time.h"
#include "read.h"
#include "write.h"
#include "malloc.h"
#include "fork.h"
#include "popen.h"
#include "pipe.h"

void *handle;

#define RTR_TEST_START(func)					\
static void test_rtr_##func(void **state) {			\
	rtr_##func##_t rtr_##func = dlsym(handle, #func);	\
	assert_non_null(rtr_##func);

#define RTR_TEST_END }

RTR_TEST_START(tolower)
RTR_TEST_END

RTR_TEST_START(toupper)
RTR_TEST_END

RTR_TEST_START(putc)
RTR_TEST_END

RTR_TEST_START(getenv)
RTR_TEST_END

RTR_TEST_START(putenv)
RTR_TEST_END

RTR_TEST_START(unsetenv)
RTR_TEST_END

RTR_TEST_START(execve)
RTR_TEST_END

RTR_TEST_START(system)
RTR_TEST_END

RTR_TEST_START(exit)
RTR_TEST_END

RTR_TEST_START(fopen)
RTR_TEST_END

RTR_TEST_START(opendir)
RTR_TEST_END

RTR_TEST_START(fclose)
RTR_TEST_END

RTR_TEST_START(closedir)
RTR_TEST_END

RTR_TEST_START(fseek)
RTR_TEST_END

RTR_TEST_START(fileno)
RTR_TEST_END

RTR_TEST_START(chmod)
RTR_TEST_END

RTR_TEST_START(fchmod)
RTR_TEST_END

RTR_TEST_START(stat)
RTR_TEST_END

RTR_TEST_START(dup)
RTR_TEST_END

RTR_TEST_START(dup2)
RTR_TEST_END

RTR_TEST_START(close)
RTR_TEST_END

RTR_TEST_START(getgid)
RTR_TEST_END

RTR_TEST_START(getegid)
RTR_TEST_END

RTR_TEST_START(getuid)
RTR_TEST_END

RTR_TEST_START(geteuid)
RTR_TEST_END

RTR_TEST_START(setuid)
RTR_TEST_END

RTR_TEST_START(seteuid)
RTR_TEST_END

RTR_TEST_START(setgid)
RTR_TEST_END

RTR_TEST_START(getpid)
RTR_TEST_END

RTR_TEST_START(getppid)
RTR_TEST_END

RTR_TEST_START(perror)
RTR_TEST_END

RTR_TEST_START(atoi)
RTR_TEST_END

RTR_TEST_START(bind)
RTR_TEST_END

RTR_TEST_START(connect)
RTR_TEST_END

RTR_TEST_START(strcpy)
RTR_TEST_END

RTR_TEST_START(strncpy)
RTR_TEST_END

RTR_TEST_START(strcat)
RTR_TEST_END

RTR_TEST_START(strncat)
RTR_TEST_END

RTR_TEST_START(strcmp)
RTR_TEST_END

RTR_TEST_START(strncmp)
RTR_TEST_END

RTR_TEST_START(strstr)
RTR_TEST_END

RTR_TEST_START(strlen)
RTR_TEST_END

RTR_TEST_START(ctime)
RTR_TEST_END

RTR_TEST_START(ctime_r)
RTR_TEST_END

#define READ_BUF_SIZE 256
RTR_TEST_START(read)
	int fd;
	ssize_t ret;
	char buf[READ_BUF_SIZE];

	fd = open("/dev/urandom", O_RDONLY);
	assert_non_null(fd);

	ret = rtr_read(fd, buf, sizeof(buf));
	assert_int_equal(ret, sizeof(buf));
RTR_TEST_END

#define WRITE_BUF_SIZE 256
RTR_TEST_START(write)
	int fd;
	ssize_t ret;
	char buf[WRITE_BUF_SIZE];

	fd = open("/dev/null", O_WRONLY);
	assert_non_null(fd);

	ret = rtr_write(fd, buf, sizeof(buf));
	assert_int_equal(ret, sizeof(buf));
RTR_TEST_END

#define RTR_MALLOC_SIZE 256
RTR_TEST_START(free)
	void *p, *saved;

	p = malloc(RTR_MALLOC_SIZE);
	assert_non_null(p);

	saved = p;

	rtr_free(p);
	assert_ptr_equal(p, saved);
RTR_TEST_END

RTR_TEST_START(malloc)
	void *p;

	p = rtr_malloc(RTR_MALLOC_SIZE);
	assert_non_null(p);
RTR_TEST_END

RTR_TEST_START(fork)
	pid_t pid, parent;

	parent = getpid();

	pid = rtr_fork();
	assert_true(pid >= 0);

	if (pid == 0) {
		assert_true(getpid() != parent);
		exit(0);
	}

	assert_int_equal(getpid(), parent);
RTR_TEST_END

#define RTR_POPEN_CMD "which gcc 2>&1"
#define RTR_POPEN_BUF_SIZE 256
RTR_TEST_START(popen)
	int ret;
	FILE *file;
	char buf[RTR_POPEN_BUF_SIZE];
	char *p;

	file = rtr_popen(RTR_POPEN_CMD, "r");
	assert_non_null(file);

	p = fgets(buf, sizeof(buf), file);
	assert_non_null(p);

	ret = pclose(file);
	assert_int_equal(ret, 0);
RTR_TEST_END

RTR_TEST_START(pclose)
	int ret;
	FILE *file;
	char buf[RTR_POPEN_BUF_SIZE];
	char *p;

	file = popen(RTR_POPEN_CMD, "r");
	assert_non_null(file);

	p = fgets(buf, sizeof(buf), file);
	assert_non_null(p);

	ret = rtr_pclose(file);
	assert_int_equal(ret, 0);
RTR_TEST_END

static void
test_pipe_common(int ret, int pipefd[2])
{
	assert_int_equal(ret, 0);

	assert_true(pipefd[0] >= 0);
	assert_true(pipefd[1] >= 0);
	assert_true(pipefd[0] != pipefd[1]);

	close(pipefd[0]);
	close(pipefd[1]);
}

RTR_TEST_START(pipe)
	int ret;
	int pipefd[2];

	ret = rtr_pipe(pipefd);
	test_pipe_common(ret, pipefd);
RTR_TEST_END

RTR_TEST_START(pipe2)
	int ret;
	int pipefd[2];

	ret = rtr_pipe2(pipefd, O_NONBLOCK);
	test_pipe_common(ret, pipefd);
RTR_TEST_END

int main(void) 
{
	int ret;
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_rtr_perror),
		cmocka_unit_test(test_rtr_tolower),
		cmocka_unit_test(test_rtr_toupper),
		cmocka_unit_test(test_rtr_putc),
		cmocka_unit_test(test_rtr_getenv),
		cmocka_unit_test(test_rtr_putenv),
		cmocka_unit_test(test_rtr_unsetenv),
		cmocka_unit_test(test_rtr_execve),
		cmocka_unit_test(test_rtr_system),
		cmocka_unit_test(test_rtr_exit),
		cmocka_unit_test(test_rtr_fopen),
		cmocka_unit_test(test_rtr_opendir),
		cmocka_unit_test(test_rtr_fclose),
		cmocka_unit_test(test_rtr_closedir),
		cmocka_unit_test(test_rtr_fseek),
		cmocka_unit_test(test_rtr_fileno),
		cmocka_unit_test(test_rtr_chmod),
		cmocka_unit_test(test_rtr_fchmod),
		cmocka_unit_test(test_rtr_stat),
		cmocka_unit_test(test_rtr_dup),
		cmocka_unit_test(test_rtr_dup2),
		cmocka_unit_test(test_rtr_close),
		cmocka_unit_test(test_rtr_getgid),
		cmocka_unit_test(test_rtr_getegid),
		cmocka_unit_test(test_rtr_getuid),
		cmocka_unit_test(test_rtr_geteuid),
		cmocka_unit_test(test_rtr_setuid),
		cmocka_unit_test(test_rtr_seteuid),
		cmocka_unit_test(test_rtr_setgid),
		cmocka_unit_test(test_rtr_getpid),
		cmocka_unit_test(test_rtr_getppid),
		cmocka_unit_test(test_rtr_perror),
		cmocka_unit_test(test_rtr_atoi),
		cmocka_unit_test(test_rtr_bind),
		cmocka_unit_test(test_rtr_connect),
		cmocka_unit_test(test_rtr_strcpy),
		cmocka_unit_test(test_rtr_strncpy),
		cmocka_unit_test(test_rtr_strcat),
		cmocka_unit_test(test_rtr_strncat),
		cmocka_unit_test(test_rtr_strcmp),
		cmocka_unit_test(test_rtr_strncmp),
		cmocka_unit_test(test_rtr_strstr),
		cmocka_unit_test(test_rtr_strlen),
		cmocka_unit_test(test_rtr_ctime),
		cmocka_unit_test(test_rtr_ctime_r),
		cmocka_unit_test(test_rtr_read),
		cmocka_unit_test(test_rtr_write),
		cmocka_unit_test(test_rtr_malloc),
		cmocka_unit_test(test_rtr_free),
		cmocka_unit_test(test_rtr_fork),
		cmocka_unit_test(test_rtr_popen),
		cmocka_unit_test(test_rtr_pclose),
		cmocka_unit_test(test_rtr_pipe),
		cmocka_unit_test(test_rtr_pipe2),
	};

	handle = dlopen("../retrace.so", RTLD_LAZY);
	if (!handle) {
		fputs(dlerror(), stderr);
		return 1;
	}

	ret = cmocka_run_group_tests(tests, NULL, NULL);

	dlclose(handle);

	return ret;
}
