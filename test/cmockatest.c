/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <dlfcn.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>

#include <cmocka.h>

#include "common.h"
#include "char.h"
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
#include "printf.h"
#include "dir.h"

void *handle;

#define RTR_TEST_START(func)                              \
    static void test_rtr_##func(void **state)             \
    {                                                     \
        rtr_##func##_t rtr_##func = dlsym(handle, #func); \
        assert_non_null(rtr_##func);					  \
		assert_ptr_not_equal(rtr_##func, func);

#define RTR_TEST_END }

RTR_TEST_START(tolower)
	static char s[] = "abcDEF123";
	static char ls[] = "abcdef123";
	int i;
	char buf[] = "ABCDEF123";

	for (i = 0; i < sizeof(s) - 1; i++)
		buf[i] = rtr_tolower(s[i]);

	assert_string_equal(buf, ls);
RTR_TEST_END

RTR_TEST_START(toupper)
	static char s[] = "abcDEF123";
	static char us[] = "ABCDEF123";
	int i;
	char buf[] = "abcdef123";

	for (i = 0; i < sizeof(s) - 1; i++)
		buf[i] = rtr_toupper(s[i]);

	assert_string_equal(buf, us);
RTR_TEST_END

RTR_TEST_START(putc)
	int fd[2];
	FILE *f;
	int i;
	char buf[5];

	pipe(fd);
	f = fdopen(fd[1], "w");
	for (i = 0; i < 4; i++)
		rtr_putc('0'+i, f);
	fclose(f);
	read(fd[0], buf, 4);
	close(fd[0]);

	buf[4] = '\0';

	assert_string_equal(buf, "0123");
RTR_TEST_END

#ifndef __APPLE__
RTR_TEST_START(_IO_putc)
	int fd[2];
	FILE *f;
	int i;
	char buf[5];

	pipe(fd);
	f = fdopen(fd[1], "w");
	for (i = 0; i < 4; i++)
		rtr__IO_putc('0'+i, f);
	fclose(f);
	read(fd[0], buf, 4);
	close(fd[0]);

	buf[4] = '\0';

	assert_string_equal(buf, "0123");
RTR_TEST_END
#endif

RTR_TEST_START(getenv)
	putenv("TESTVAR=BIJOU");
	assert_string_equal(rtr_getenv("TESTVAR"), "BIJOU");
	assert_ptr_equal(rtr_getenv("NOTESTVAR"), NULL);
RTR_TEST_END

RTR_TEST_START(putenv)
	rtr_putenv("TESTVAR=BIJOU");
	assert_string_equal(getenv("TESTVAR"), "BIJOU");
RTR_TEST_END

RTR_TEST_START(unsetenv)
	putenv("TESTVAR=BIJOU");
	rtr_unsetenv("TESTVAR");
	rtr_unsetenv("NOTESTVAR");
	assert_ptr_equal(getenv("TESTVAR"), NULL);
RTR_TEST_END

RTR_TEST_START(execl)
RTR_TEST_END

RTR_TEST_START(execv)
RTR_TEST_END

RTR_TEST_START(execle)
RTR_TEST_END

RTR_TEST_START(execve)
RTR_TEST_END

RTR_TEST_START(execlp)
RTR_TEST_END

RTR_TEST_START(execvp)
RTR_TEST_END

RTR_TEST_START(execvpe)
RTR_TEST_END

/*
RTR_TEST_START(execveat)
RTR_TEST_END
*/

RTR_TEST_START(fexecve)
RTR_TEST_END

RTR_TEST_START(system)
RTR_TEST_END

RTR_TEST_START(exit)
RTR_TEST_END

RTR_TEST_START(fopen)
RTR_TEST_END

RTR_TEST_START(fclose)
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

RTR_TEST_START(strchr)
	char *p;
	static char s1[] = "0123456789";
	static char s2[] = "\t\r\n";

	p = rtr_strchr(s1, '4');
	assert_int_equal(p, s1+4);

	p = rtr_strchr(s1, 'a');
	assert_ptr_equal(p, NULL);

	p = rtr_strchr(s2, '\r');
	assert_int_equal(p, s2+1);

	p = rtr_strchr(s2, 'a');
	assert_ptr_equal(p, NULL);
RTR_TEST_END

RTR_TEST_START(strlen)
RTR_TEST_END

RTR_TEST_START(ctime)
RTR_TEST_END

RTR_TEST_START(ctime_r)
RTR_TEST_END

/* dir functions test */
RTR_TEST_START(opendir)
RTR_TEST_END

RTR_TEST_START(closedir)
RTR_TEST_END

RTR_TEST_START(fdopendir)
RTR_TEST_END

RTR_TEST_START(readdir_r)
RTR_TEST_END

RTR_TEST_START(telldir)
RTR_TEST_END

RTR_TEST_START(seekdir)
RTR_TEST_END

RTR_TEST_START(rewinddir)
RTR_TEST_END

RTR_TEST_START(dirfd)
RTR_TEST_END

#define READ_BUF_SIZE 256
RTR_TEST_START(read)
int     fd;
ssize_t ret;
char    buf[READ_BUF_SIZE];

fd = open("/dev/urandom", O_RDONLY);
assert_non_null(fd);

ret = rtr_read(fd, buf, sizeof(buf));
assert_int_equal(ret, sizeof(buf));
RTR_TEST_END

#define WRITE_BUF_SIZE 256
RTR_TEST_START(write)
int     fd;
ssize_t ret;
char    buf[WRITE_BUF_SIZE];

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
int   ret;
FILE *file;
char  buf[RTR_POPEN_BUF_SIZE];
char *p;

file = rtr_popen(RTR_POPEN_CMD, "r");
assert_non_null(file);

p = fgets(buf, sizeof(buf), file);
assert_non_null(p);

ret = pclose(file);
assert_int_equal(ret, 0);
RTR_TEST_END

RTR_TEST_START(pclose)
int   ret;
FILE *file;
char  buf[RTR_POPEN_BUF_SIZE];
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

static void
test_trace_printf(void **state)
{
    void (*trace_printf)(int, const char *, ...) = dlsym(handle, "trace_printf");
	FILE *oldstderr = stderr;
	char ebuf[256];
	char pbuf[256];

	stderr = fmemopen(ebuf, 256, "w");
	trace_printf(1, "%d %s %c", 42, "forty two", 42);
	fclose(stderr);
	stderr = oldstderr;
	sprintf(pbuf, "(%d) %d %s %c", getpid(), 42, "forty two", 42);
	assert_string_equal(ebuf, pbuf);

	stderr = fmemopen(ebuf, 256, "w");
	trace_printf(0, "%d %s %c", 42, "forty two", 42);
	fclose(stderr);
	stderr = oldstderr;
	sprintf(pbuf, "%d %s %c", 42, "forty two", 42);
	assert_string_equal(ebuf, pbuf);
}

RTR_TEST_START(printf)
char buf[256], buf1[256];
FILE *oldstdout = stdout;
stdout = fmemopen(buf, 256, "w");
int r = rtr_printf("%d %s %c", 42, "forty two", 42);
fclose(stdout);
stdout = fmemopen(buf1, 256, "w");
int r1 = printf("%d %s %c", 42, "forty two", 42);
fclose(stdout);
stdout = oldstdout;
assert_true(r > 0 && r == r1);
assert_string_equal(buf, buf1);
RTR_TEST_END

RTR_TEST_START(fprintf)
char buf[256], buf1[256];
FILE *f = fmemopen(buf, 256, "w");
FILE *f1 = fmemopen(buf1, 256, "w");
int r = rtr_fprintf(f, "%d %s %c", 42, "forty two", 42);
int r1 = fprintf(f1, "%d %s %c", 42, "forty two", 42);
fclose(f);
fclose(f1);
assert_true(r > 0 && r == r1);
assert_string_equal(buf, buf1);
RTR_TEST_END

RTR_TEST_START(dprintf)
char buf[256], buf1[256];
int fd[2], fd1[2];

pipe(fd);
int r = rtr_dprintf(fd[1], "%d %s %c", 42, "forty two", 42);
read(fd[0], buf, 256);
close(fd[0]);
close(fd[1]);

pipe(fd1);
int r1 = dprintf(fd1[1], "%d %s %c", 42, "forty two", 42);
read(fd1[0], buf1, 256);
close(fd1[0]);
close(fd1[1]);

assert_true(r > 0 && r == r1);
assert_string_equal(buf, buf1);
RTR_TEST_END

RTR_TEST_START(sprintf)
char buf[256], buf1[256];

int r = rtr_sprintf(buf, "%d %s %c", 42, "forty two", 42);
int r1 = sprintf(buf1, "%d %s %c", 42, "forty two", 42);

assert_true(r > 0 && r == r1);
assert_string_equal(buf, buf1);
RTR_TEST_END

RTR_TEST_START(snprintf)
char buf[256], buf1[256];

int r = rtr_snprintf(buf, 256, "%d %s %c", 42, "forty two", 42);
int r1 = snprintf(buf1, 256, "%d %s %c", 42, "forty two", 42);

assert_true(r > 0 && r == r1);
assert_string_equal(buf, buf1);
RTR_TEST_END

static int
to_vprintf(rtr_vprintf_t fn, const char * fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int result = fn(fmt, ap);
	va_end(ap);
	return result;
}

RTR_TEST_START(vprintf)
char buf[256], buf1[256];
FILE *oldstdout = stdout;
stdout = fmemopen(buf, 256, "w");
int r = to_vprintf(rtr_vprintf, "%d %s %c", 42, "forty two", 42);
fclose(stdout);
stdout = fmemopen(buf1, 256, "w");
int r1 = to_vprintf(vprintf, "%d %s %c", 42, "forty two", 42);
fclose(stdout);
stdout = oldstdout;
assert_true(r > 0 && r == r1);
assert_string_equal(buf, buf1);
RTR_TEST_END

static int
to_vfprintf(rtr_vfprintf_t fn, FILE *f, const char * fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int result = fn(f, fmt, ap);
	va_end(ap);
	return result;
}

RTR_TEST_START(vfprintf)
char buf[256], buf1[256];
FILE *f = fmemopen(buf, 256, "w");
FILE *f1 = fmemopen(buf1, 256, "w");
int r = to_vfprintf(rtr_vfprintf, f, "%d %s %c", 42, "forty two", 42);
int r1 = to_vfprintf(vfprintf, f1, "%d %s %c", 42, "forty two", 42);
fclose(f);
fclose(f1);
assert_true(r > 0 && r == r1);
assert_string_equal(buf, buf1);
RTR_TEST_END

static int
to_vdprintf(rtr_vdprintf_t fn, int fd, const char * fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int result = fn(fd, fmt, ap);
	va_end(ap);
	return result;
}

RTR_TEST_START(vdprintf)
char buf[256], buf1[256];
int fd[2], fd1[2];

pipe(fd);
int r = to_vdprintf(rtr_vdprintf, fd[1], "%d %s %c", 42, "forty two", 42);
read(fd[0], buf, 256);
close(fd[0]);
close(fd[1]);

pipe(fd1);
int r1 = to_vdprintf(vdprintf, fd1[1], "%d %s %c", 42, "forty two", 42);
read(fd1[0], buf1, 256);
close(fd1[0]);
close(fd1[1]);

assert_true(r > 0 && r == r1);
assert_string_equal(buf, buf1);
RTR_TEST_END

static int
to_vsprintf(rtr_vsprintf_t fn, char *str, const char * fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int result = fn(str, fmt, ap);
	va_end(ap);
	return result;
}

RTR_TEST_START(vsprintf)
char buf[256], buf1[256];

int r = to_vsprintf(rtr_vsprintf, buf, "%d %s %c", 42, "forty two", 42);
int r1 = to_vsprintf(vsprintf, buf1, "%d %s %c", 42, "forty two", 42);

assert_true(r > 0 && r == r1);
assert_string_equal(buf, buf1);
RTR_TEST_END

static int
to_vsnprintf(rtr_vsnprintf_t fn, char *str, size_t size, const char * fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int result = fn(str, size, fmt, ap);
	va_end(ap);
	return result;
}

RTR_TEST_START(vsnprintf)
char buf[256], buf1[256];

int r = to_vsnprintf(rtr_vsnprintf, buf, 256, "%d %s %c", 42, "forty two", 42);
int r1 = to_vsnprintf(vsnprintf, buf1, 256, "%d %s %c", 42, "forty two", 42);

assert_true(r > 0 && r == r1);
assert_string_equal(buf, buf1);
RTR_TEST_END

int
main(void)
{
    int                     ret;
    const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_rtr_perror),   cmocka_unit_test(test_rtr_tolower),
      cmocka_unit_test(test_rtr_toupper),  cmocka_unit_test(test_rtr_putc),
#ifndef __APPLE__
      cmocka_unit_test(test_rtr__IO_putc),
#endif
      cmocka_unit_test(test_rtr_getenv),   cmocka_unit_test(test_rtr_putenv),
      cmocka_unit_test(test_rtr_unsetenv), cmocka_unit_test(test_rtr_execl),
      cmocka_unit_test(test_rtr_execv),    cmocka_unit_test(test_rtr_execle),
      cmocka_unit_test(test_rtr_execve),   cmocka_unit_test(test_rtr_execlp),
      cmocka_unit_test(test_rtr_execvp),   cmocka_unit_test(test_rtr_execvpe),
      /* cmocka_unit_test(test_rtr_execveat), */ cmocka_unit_test(test_rtr_fexecve),
      cmocka_unit_test(test_rtr_system),   cmocka_unit_test(test_rtr_exit),
      cmocka_unit_test(test_rtr_fopen),    cmocka_unit_test(test_rtr_fclose),
      cmocka_unit_test(test_rtr_fseek),    cmocka_unit_test(test_rtr_fileno),
      cmocka_unit_test(test_rtr_chmod),    cmocka_unit_test(test_rtr_fchmod),
      cmocka_unit_test(test_rtr_stat),     cmocka_unit_test(test_rtr_dup),
      cmocka_unit_test(test_rtr_dup2),     cmocka_unit_test(test_rtr_close),
      cmocka_unit_test(test_rtr_getgid),   cmocka_unit_test(test_rtr_getegid),
      cmocka_unit_test(test_rtr_getuid),   cmocka_unit_test(test_rtr_geteuid),
      cmocka_unit_test(test_rtr_setuid),   cmocka_unit_test(test_rtr_seteuid),
      cmocka_unit_test(test_rtr_setgid),   cmocka_unit_test(test_rtr_getpid),
      cmocka_unit_test(test_rtr_getppid),  cmocka_unit_test(test_rtr_perror),
      cmocka_unit_test(test_rtr_atoi),     cmocka_unit_test(test_rtr_bind),
      cmocka_unit_test(test_rtr_connect),  cmocka_unit_test(test_rtr_strcpy),
      cmocka_unit_test(test_rtr_strncpy),  cmocka_unit_test(test_rtr_strcat),
      cmocka_unit_test(test_rtr_strncat),  cmocka_unit_test(test_rtr_strcmp),
      cmocka_unit_test(test_rtr_strncmp),  cmocka_unit_test(test_rtr_strstr),
      cmocka_unit_test(test_rtr_strchr),
      cmocka_unit_test(test_rtr_strlen),   cmocka_unit_test(test_rtr_ctime),
      cmocka_unit_test(test_rtr_ctime_r),  cmocka_unit_test(test_rtr_read),
      cmocka_unit_test(test_rtr_write),    cmocka_unit_test(test_rtr_malloc),
      cmocka_unit_test(test_rtr_free),     cmocka_unit_test(test_rtr_fork),
      cmocka_unit_test(test_rtr_popen),    cmocka_unit_test(test_rtr_pclose),
      cmocka_unit_test(test_rtr_pipe),     cmocka_unit_test(test_rtr_pipe2),
      cmocka_unit_test(test_rtr_printf),   cmocka_unit_test(test_rtr_fprintf),
      cmocka_unit_test(test_rtr_dprintf),  cmocka_unit_test(test_rtr_sprintf),
      cmocka_unit_test(test_rtr_snprintf), cmocka_unit_test(test_rtr_vprintf),
      cmocka_unit_test(test_rtr_vfprintf), cmocka_unit_test(test_rtr_vdprintf),
      cmocka_unit_test(test_rtr_vsprintf), cmocka_unit_test(test_rtr_vsnprintf),

      /* dir functions */
      cmocka_unit_test(test_rtr_opendir),  cmocka_unit_test(test_rtr_closedir),
      cmocka_unit_test(test_rtr_fdopendir),  cmocka_unit_test(test_rtr_readdir_r),
      cmocka_unit_test(test_rtr_telldir), cmocka_unit_test(test_rtr_seekdir),
      cmocka_unit_test(test_rtr_rewinddir), cmocka_unit_test(test_rtr_dirfd),
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
