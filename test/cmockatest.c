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

void *handle;

#define RTR_TEST_START(func) \
static void test_##func(void **state) { \
	rtr_##func##_t rtr_##func = dlsym(handle, #func); \
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

int main(void) 
{
	int ret;
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_perror),
		cmocka_unit_test(test_tolower),
		cmocka_unit_test(test_toupper),
		cmocka_unit_test(test_putc),
		cmocka_unit_test(test_getenv),
		cmocka_unit_test(test_putenv),
		cmocka_unit_test(test_unsetenv),
		cmocka_unit_test(test_execve),
		cmocka_unit_test(test_system),
		cmocka_unit_test(test_exit),
		cmocka_unit_test(test_fopen),
		cmocka_unit_test(test_opendir),
		cmocka_unit_test(test_fclose),
		cmocka_unit_test(test_closedir),
		cmocka_unit_test(test_fseek),
		cmocka_unit_test(test_fileno),
		cmocka_unit_test(test_chmod),
		cmocka_unit_test(test_fchmod),
		cmocka_unit_test(test_stat),
		cmocka_unit_test(test_dup),
		cmocka_unit_test(test_dup2),
		cmocka_unit_test(test_close),
		cmocka_unit_test(test_getgid),
		cmocka_unit_test(test_getegid),
		cmocka_unit_test(test_getuid),
		cmocka_unit_test(test_geteuid),
		cmocka_unit_test(test_setuid),
		cmocka_unit_test(test_seteuid),
		cmocka_unit_test(test_setgid),
		cmocka_unit_test(test_getpid),
		cmocka_unit_test(test_getppid),
		cmocka_unit_test(test_perror),
		cmocka_unit_test(test_atoi),
		cmocka_unit_test(test_bind),
		cmocka_unit_test(test_connect),
		cmocka_unit_test(test_strcpy),
		cmocka_unit_test(test_strncpy),
		cmocka_unit_test(test_strcat),
		cmocka_unit_test(test_strncat),
		cmocka_unit_test(test_strcmp),
		cmocka_unit_test(test_strncmp),
		cmocka_unit_test(test_strstr),
		cmocka_unit_test(test_strlen),
		cmocka_unit_test(test_ctime),
		cmocka_unit_test(test_ctime_r),
		cmocka_unit_test(test_read),
		cmocka_unit_test(test_write),
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
