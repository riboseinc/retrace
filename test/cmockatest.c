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

#include "common.h"
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
#include <sys/mman.h>

#include <cmocka.h>

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
#include "scanf.h"
#include "rtr-netdb.h"

void *handle;

#define RTR_TEST_START(func)					\
static void test_rtr_##func(void **state)			\
{								\
	rtr_##func##_t rtr_##func = dlsym(handle, #func);	\
	assert_non_null(rtr_##func);				\
	assert_ptr_not_equal(rtr_##func, func);

#define RTR_TEST_END }

RTR_TEST_START(tolower)
	static const char s[] = "abcDEF123";
	static const char ls[] = "abcdef123";
	int i;
	char buf[] = "ABCDEF123";

	for (i = 0; i < sizeof(s) - 1; i++)
		buf[i] = rtr_tolower(s[i]);

	assert_string_equal(buf, ls);
RTR_TEST_END

RTR_TEST_START(toupper)
	static const char s[] = "abcDEF123";
	static const char us[] = "ABCDEF123";
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
 * RTR_TEST_START(execveat)
 * RTR_TEST_END
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

RTR_TEST_START(socket)
RTR_TEST_END

RTR_TEST_START(bind)
RTR_TEST_END

RTR_TEST_START(connect)
RTR_TEST_END

RTR_TEST_START(accept)
RTR_TEST_END

RTR_TEST_START(setsockopt)
RTR_TEST_END

RTR_TEST_START(send)
RTR_TEST_END

RTR_TEST_START(sendto)
RTR_TEST_END

RTR_TEST_START(sendmsg)
RTR_TEST_END

RTR_TEST_START(recv)
RTR_TEST_END

RTR_TEST_START(recvfrom)
RTR_TEST_END

RTR_TEST_START(recvmsg)
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

RTR_TEST_START(strcasecmp)
RTR_TEST_END

RTR_TEST_START(strncmp)
RTR_TEST_END

RTR_TEST_START(strstr)
RTR_TEST_END

RTR_TEST_START(strchr)
	char *p;
	static const char s1[] = "0123456789";
	static const char s2[] = "\t\r\n";

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

/* netdb functions test */
RTR_TEST_START(gethostbyname)
RTR_TEST_END

RTR_TEST_START(gethostbyaddr)
RTR_TEST_END

RTR_TEST_START(sethostent)
RTR_TEST_END

RTR_TEST_START(endhostent)
RTR_TEST_END

RTR_TEST_START(gethostent)
RTR_TEST_END

RTR_TEST_START(gethostbyname2)
RTR_TEST_END

RTR_TEST_START(getaddrinfo)
	struct addrinfo hints, *result;
	int ret;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = 0;
	hints.ai_protocol = 0;

	ret = rtr_getaddrinfo("www.google.com", NULL, &hints, &result);
	if (result)
		freeaddrinfo(result);
RTR_TEST_END

RTR_TEST_START(freeaddrinfo)
	struct addrinfo hints, *result;
	int ret;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = 0;
	hints.ai_protocol = 0;

	ret = getaddrinfo("www.google.com", NULL, &hints, &result);
	if (result)
		rtr_freeaddrinfo(result);
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

RTR_TEST_START(readv)
	int fd;
	ssize_t ret;
	char buf[READ_BUF_SIZE];

	struct iovec iov[1];

	fd = open("/dev/urandom", O_RDONLY);
	assert_non_null(fd);

	iov[0].iov_base = buf;
	iov[0].iov_len = sizeof(buf);

	ret = rtr_readv(fd, iov, 1);
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

RTR_TEST_START(writev)
	int fd;
	ssize_t ret;
	char buf[WRITE_BUF_SIZE];

	struct iovec iov[1];

	fd = open("/dev/urandom", O_WRONLY);
	assert_non_null(fd);

	iov[0].iov_base = buf;
	iov[0].iov_len = sizeof(buf);

	ret = rtr_writev(fd, iov, 1);
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

RTR_TEST_START(realloc)
	void *p;

	p = malloc(RTR_MALLOC_SIZE);
	assert_non_null(p);

	p = rtr_realloc(p, RTR_MALLOC_SIZE + 64);
	assert_non_null(p);
RTR_TEST_END

RTR_TEST_START(calloc)
	void *p;

	p = rtr_calloc(1, RTR_MALLOC_SIZE);
	assert_non_null(p);
RTR_TEST_END

RTR_TEST_START(memcpy)
	void *p;
	unsigned char q[32];

	memset(q, '1', sizeof(q));

	p = malloc(32);
	assert_non_null(p);

	p = rtr_memcpy(p, q, sizeof(q));
	assert_non_null(p);

	free(p);
RTR_TEST_END

RTR_TEST_START(memmove)
	void *p;

	p = malloc(32);

	memset(p, '1', 32);

	p = rtr_memmove(p, p + 5, 10);
	assert_non_null(p);

	free(p);
RTR_TEST_END

RTR_TEST_START(bcopy)
	unsigned char p[32];

	memset(p, '1', sizeof(p));

	rtr_bcopy(p + 5, p, 10);
RTR_TEST_END

RTR_TEST_START(memccpy)
	unsigned char p[32];
	void *q;

	q = malloc(32);
	assert_non_null(q);

	memset(p, '0', 16);
	memset(p, '1', 16);

	q = rtr_memccpy(q, p, '1', sizeof(p));
	assert_non_null(q);
RTR_TEST_END

RTR_TEST_START(mmap)
	void *p;

	p = rtr_mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	assert_non_null(p);
RTR_TEST_END

RTR_TEST_START(munmap)
	void *p;

	p = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	assert_non_null(p);

	rtr_munmap(p, sizeof(int));
RTR_TEST_END

#ifndef __APPLE__

RTR_TEST_START(brk)
	void *p, *q;

	p = sbrk(0);
	q = sbrk(1024);

	assert_ptr_equal(p, q);
	assert_non_null(q);

	rtr_brk(q);
RTR_TEST_END

RTR_TEST_START(sbrk)
	void *p;

	p = rtr_sbrk(0);
	assert_non_null(p);
RTR_TEST_END

#endif

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

/*
 * TODO: unit test static functions
 *
 * trace_printf and trace_printf_str are static in common.c
 *
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

static void
test_trace_printf_str(void **state)
{
	void (*trace_printf_str)(const char *, int len);
	FILE *oldstderr = stderr;
	const char snip[] = "[SNIP]";
	char buf[256];
	char s[MAXLEN+1];
	char s1[MAXLEN+2];

	trace_printf_str = dlsym(handle, "trace_printf_str");

	memset(s, '1', MAXLEN+1);
	memset(s1, '1', MAXLEN+2);
	s[MAXLEN] = '\0';
	s1[MAXLEN+1] = '\0';

	// special characters are handled correctly
	stderr = fmemopen(buf, 256, "w");
	trace_printf_str("abc\r\n\tdef", -1);
	fclose(stderr);
	stderr = oldstderr;
	assert_string_equal(buf,
			"abc" VAR "\\r" RST VAR "\\n" RST VAR "\\t" RST "def");

	// MAXLEN string is unmodified
	stderr = fmemopen(buf, 256, "w");
	trace_printf_str(s, -1);
	fclose(stderr);
	stderr = oldstderr;
	assert_string_equal(buf, s);

	// MAXLEN+1 string is [SNIP]ped
	stderr = fmemopen(buf, 256, "w");
	trace_printf_str(s1, -1);
	fclose(stderr);
	stderr = oldstderr;

	assert_int_equal(strlen(buf), MAXLEN + sizeof(snip) - 1);
	assert_int_equal(strncmp(s, buf, MAXLEN), 0);
	assert_string_equal(buf+MAXLEN, snip);
}
 *
 */

static void
test_fdlist(void **state)
{
	struct descriptor_info *(*file_descriptor_get)(int fd);
	void (*file_descriptor_update)(int fd, unsigned int type,
	    const char *location);
	void (*file_descriptor_remove)(int fd);
	struct descriptor_info *p;
	int i;

	file_descriptor_get = dlsym(handle, "file_descriptor_get");
	file_descriptor_update = dlsym(handle, "file_descriptor_update");
	file_descriptor_remove = dlsym(handle, "file_descriptor_remove");

	assert_null(file_descriptor_get(42));

	file_descriptor_update(42, 6502, "forty two");
	p = file_descriptor_get(42);
	assert_non_null(p);
	assert_int_equal(p->fd, 42);
	assert_int_equal(p->type, 6502);
	assert_string_equal(p->location, "forty two");

	file_descriptor_update(42, 6503, "forty three");
	p = file_descriptor_get(42);
	assert_non_null(p);
	assert_int_equal(p->fd, 42);
	assert_int_equal(p->type, 6503);
	assert_string_equal(p->location, "forty three");

	file_descriptor_remove(42);
	assert_null(file_descriptor_get(42));

	for (i = 0; i < 1000; i++)
		file_descriptor_update(i, i+1, "test");

	for (i = 0; i < 1000; i++)
		file_descriptor_update(i, i+3, "test");

	for (i = 0; i < 1000; i++) {
		p = file_descriptor_get(i);
		assert_non_null(p);
		assert_int_equal(p->fd, i);
		assert_int_equal(p->type, i+3);
	}

	file_descriptor_remove(500);
	assert_null(file_descriptor_get(500));

	for (i = 999; i >= 0; i--) {
		file_descriptor_remove(i);
		assert_null(file_descriptor_get(i));
	}
	file_descriptor_remove(10001);
}

RTR_TEST_START(printf)
	int r, r1;
	char buf[256], buf1[256];
	FILE *oldstdout = stdout;

	stdout = fmemopen(buf, 256, "w");
	r = rtr_printf("%d %s %c", 42, "forty two", 42);
	fclose(stdout);

	stdout = fmemopen(buf1, 256, "w");
	r1 = printf("%d %s %c", 42, "forty two", 42);
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
	int r, r1;
	char buf[256], buf1[256];
	int fd[2], fd1[2];

	pipe(fd);
	r = rtr_dprintf(fd[1], "%d %s %c", 42, "forty two", 42);
	read(fd[0], buf, 256);
	close(fd[0]);
	close(fd[1]);

	pipe(fd1);
	r1 = dprintf(fd1[1], "%d %s %c", 42, "forty two", 42);
	read(fd1[0], buf1, 256);
	close(fd1[0]);
	close(fd1[1]);

	assert_true(r > 0 && r == r1);
	assert_string_equal(buf, buf1);
RTR_TEST_END

RTR_TEST_START(sprintf)
	int r, r1;
	char buf[256], buf1[256];

	r = rtr_sprintf(buf, "%d %s %c", 42, "forty two", 42);
	r1 = sprintf(buf1, "%d %s %c", 42, "forty two", 42);

	assert_true(r > 0 && r == r1);
	assert_string_equal(buf, buf1);
RTR_TEST_END

RTR_TEST_START(snprintf)
	int r, r1;
	char buf[256], buf1[256];

	r = rtr_snprintf(buf, 256, "%d %s %c", 42, "forty two", 42);
	r1 = snprintf(buf1, 256, "%d %s %c", 42, "forty two", 42);

	assert_true(r > 0 && r == r1);
	assert_string_equal(buf, buf1);
RTR_TEST_END

static int
to_vprintf(rtr_vprintf_t fn, const char *fmt, ...)
{
	int result;
	va_list ap;

	va_start(ap, fmt);
	result = fn(fmt, ap);
	va_end(ap);

	return result;
}

RTR_TEST_START(vprintf)
	int r, r1;
	char buf[256], buf1[256];
	FILE *oldstdout = stdout;

	stdout = fmemopen(buf, 256, "w");
	r = to_vprintf(rtr_vprintf, "%d %s %c", 42, "forty two", 42);
	fclose(stdout);

	stdout = fmemopen(buf1, 256, "w");
	r1 = to_vprintf(vprintf, "%d %s %c", 42, "forty two", 42);
	fclose(stdout);

	stdout = oldstdout;

	assert_true(r > 0 && r == r1);
	assert_string_equal(buf, buf1);
RTR_TEST_END

static int
to_vfprintf(rtr_vfprintf_t fn, FILE *f, const char *fmt, ...)
{
	int result;
	va_list ap;

	va_start(ap, fmt);
	result = fn(f, fmt, ap);
	va_end(ap);

	return result;
}

RTR_TEST_START(vfprintf)
	int r, r1;
	FILE *f, *f1;
	char buf[256], buf1[256];

	f = fmemopen(buf, 256, "w");
	f1 = fmemopen(buf1, 256, "w");

	r = to_vfprintf(rtr_vfprintf, f, "%d %s %c", 42, "forty two", 42);
	r1 = to_vfprintf(vfprintf, f1, "%d %s %c", 42, "forty two", 42);

	fclose(f);
	fclose(f1);

	assert_true(r > 0 && r == r1);
	assert_string_equal(buf, buf1);
RTR_TEST_END

static int
to_vdprintf(rtr_vdprintf_t fn, int fd, const char *fmt, ...)
{
	int result;
	va_list ap;

	va_start(ap, fmt);
	result = fn(fd, fmt, ap);
	va_end(ap);

	return result;
}

RTR_TEST_START(vdprintf)
	int r, r1;
	char buf[256], buf1[256];
	int fd[2], fd1[2];

	pipe(fd);
	r = to_vdprintf(rtr_vdprintf, fd[1], "%d %s %c", 42, "forty two", 42);
	read(fd[0], buf, 256);
	close(fd[0]);
	close(fd[1]);

	pipe(fd1);
	r1 = to_vdprintf(vdprintf, fd1[1], "%d %s %c", 42, "forty two", 42);
	read(fd1[0], buf1, 256);
	close(fd1[0]);
	close(fd1[1]);

	assert_true(r > 0 && r == r1);
	assert_string_equal(buf, buf1);
RTR_TEST_END

static int
to_vsprintf(rtr_vsprintf_t fn, char *str, const char *fmt, ...)
{
	int result;
	va_list ap;

	va_start(ap, fmt);
	result = fn(str, fmt, ap);
	va_end(ap);

	return result;
}

RTR_TEST_START(vsprintf)
	int r, r1;
	char buf[256], buf1[256];

	r = to_vsprintf(rtr_vsprintf, buf, "%d %s %c", 42, "forty two", 42);
	r1 = to_vsprintf(vsprintf, buf1, "%d %s %c", 42, "forty two", 42);

	assert_true(r > 0 && r == r1);
	assert_string_equal(buf, buf1);
RTR_TEST_END

static int
to_vsnprintf(rtr_vsnprintf_t fn, char *str, size_t size, const char *fmt, ...)
{
	int result;
	va_list ap;

	va_start(ap, fmt);
	result = fn(str, size, fmt, ap);
	va_end(ap);

	return result;
}

RTR_TEST_START(vsnprintf)
	int r, r1;
	char buf[256], buf1[256];

	r = to_vsnprintf(rtr_vsnprintf, buf, 256, "%d %s %c", 42, "forty two", 42);
	r1 = to_vsnprintf(vsnprintf, buf1, 256, "%d %s %c", 42, "forty two", 42);

	assert_true(r > 0 && r == r1);
	assert_string_equal(buf, buf1);
RTR_TEST_END

RTR_TEST_START(scanf)
	char buf1[100], buf2[100];
	int fd[2];
	int r1, r2, old_fd0;

	old_fd0 = dup(0);

	pipe(fd);
	dup2(fd[0], 0);

	write(fd[1], "string123 ", strlen("string123 "));
	r1 = scanf("%s", buf1);
	write(fd[1], "string123 ", strlen("string123 "));
	r2 = rtr_scanf("%s", buf2);

	dup2(old_fd0, 0);

	close(old_fd0);
	close(fd[0]);
	close(fd[1]);

	assert_true(r1 > 0 && r1 == r2);
	assert_string_equal(buf1, "string123");
	assert_string_equal(buf1, buf2);
RTR_TEST_END

RTR_TEST_START(fscanf)
	char str1[100], str2[100];
	FILE *fp1, *fp2;
	int r1, r2;

	fp1 = fopen("scanf_test1.txt", "w+");
	fputs("fscanftest", fp1);
	rewind(fp1);
	r1 = fscanf(fp1, "%s", str1);

	fp2 = fopen("scanf_test2.txt", "w+");
	fputs("fscanftest", fp2);
	rewind(fp2);
	r2 = rtr_fscanf(fp2, "%s", str2);

	assert_true(r1 > 0 && r1 == r2);
	assert_string_equal(str1, str2);
RTR_TEST_END

RTR_TEST_START(sscanf)
	char month1[20], month2[20], dtm[100];
	int r1, r2;

	strcpy(dtm, "October");
	r1 = sscanf(dtm, "%s", month1);
	r2 = rtr_sscanf(dtm, "%s", month2);

	assert_true(r1 > 0 && r1 == r2);
	assert_string_equal(month1, month2);
RTR_TEST_END

int
to_vscanf(rtr_vscanf_t fn, const char *fmt, ...)
{
	va_list ap;
	int result;

	va_start(ap, fmt);
	result = fn(fmt, ap);
	va_end(ap);
	return result;
}

RTR_TEST_START(vscanf)
	char buf1[100], buf2[100];
	int fd[2];
	int r1, r2, old_fd0;

	old_fd0 = dup(0);

	pipe(fd);
	dup2(fd[0], 0);

	write(fd[1], "string123 ", strlen("string123 "));
	r1 = to_vscanf(vscanf, "%s", buf1);
	write(fd[1], "string123 ", strlen("string123 "));
	r2 = to_vscanf(rtr_vscanf, "%s", buf2);

	dup2(old_fd0, 0);

	close(old_fd0);
	close(fd[0]);
	close(fd[1]);

	assert_true(r1 > 0 && r1 == r2);
	assert_string_equal(buf1, "string123");
	assert_string_equal(buf1, buf2);
RTR_TEST_END

int
to_vfscanf(rtr_vfscanf_t fn, FILE *stream, const char *fmt, ...)
{
	va_list ap;
	int result;

	va_start(ap, fmt);
	result = fn(stream, fmt, ap);
	va_end(ap);
	return result;
}

RTR_TEST_START(vfscanf)
	char str1[100], str2[100];
	FILE *fp1, *fp2;
	int r1, r2;

	fp1 = fopen("scanf_test1.txt", "w+");
	fputs("fscanftest", fp1);
	rewind(fp1);
	r1 = to_vfscanf(vfscanf, fp1, "%s", str1);

	fp2 = fopen("scanf_test2.txt", "w+");
	fputs("fscanftest", fp2);
	rewind(fp2);
	r2 = to_vfscanf(rtr_vfscanf, fp2, "%s", str2);

	assert_true(r1 > 0 && r1 == r2);
	assert_string_equal(str1, str2);
RTR_TEST_END

int
to_vsscanf(rtr_vsscanf_t fn, const char *str, const char *fmt, ...)
{
	va_list ap;
	int result;

	va_start(ap, fmt);
	result = fn(str, fmt, ap);
	va_end(ap);
	return result;
}

RTR_TEST_START(vsscanf)
	char month1[20], month2[20], dtm[100];
	int r1, r2;

	strcpy(dtm, "October");
	r1 = to_vsscanf(vsscanf, dtm, "%s", month1);
	r2 = to_vsscanf(rtr_vsscanf, dtm, "%s", month2);

	assert_true(r1 > 0 && r1 == r2);
	assert_string_equal(month1, month2);
RTR_TEST_END

int
main(void)
{
	int ret;
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
		cmocka_unit_test(test_rtr_strcpy),
		cmocka_unit_test(test_rtr_strncpy),  cmocka_unit_test(test_rtr_strcat),
		cmocka_unit_test(test_rtr_strncat),  cmocka_unit_test(test_rtr_strcmp),
		cmocka_unit_test(test_rtr_strncmp),  cmocka_unit_test(test_rtr_strstr),
		cmocka_unit_test(test_rtr_strchr),   cmocka_unit_test(test_rtr_strcasecmp),
		cmocka_unit_test(test_rtr_strlen),   cmocka_unit_test(test_rtr_ctime),
		cmocka_unit_test(test_rtr_ctime_r),

		/* read/write functions */
		cmocka_unit_test(test_rtr_read),     cmocka_unit_test(test_rtr_readv),
		cmocka_unit_test(test_rtr_write),    cmocka_unit_test(test_rtr_writev),

		/* mem functions */
		cmocka_unit_test(test_rtr_malloc),   cmocka_unit_test(test_rtr_free),
		cmocka_unit_test(test_rtr_realloc),  cmocka_unit_test(test_rtr_calloc),
		cmocka_unit_test(test_rtr_memcpy),   cmocka_unit_test(test_rtr_memmove),
		cmocka_unit_test(test_rtr_bcopy),    cmocka_unit_test(test_rtr_memccpy),
		cmocka_unit_test(test_rtr_mmap),     cmocka_unit_test(test_rtr_munmap),

#ifndef __APPLE__
		cmocka_unit_test(test_rtr_brk),      cmocka_unit_test(test_rtr_sbrk),
#endif

		cmocka_unit_test(test_rtr_fork),
		cmocka_unit_test(test_rtr_popen),    cmocka_unit_test(test_rtr_pclose),
		cmocka_unit_test(test_rtr_pipe),     cmocka_unit_test(test_rtr_pipe2),
		/*cmocka_unit_test(test_trace_printf), cmocka_unit_test(test_trace_printf_str),*/
		cmocka_unit_test(test_fdlist),
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

		/* socket functions */
		cmocka_unit_test(test_rtr_socket),   cmocka_unit_test(test_rtr_connect),
		cmocka_unit_test(test_rtr_bind),     cmocka_unit_test(test_rtr_accept),

		cmocka_unit_test(test_rtr_setsockopt),
		cmocka_unit_test(test_rtr_send),     cmocka_unit_test(test_rtr_sendto),
		cmocka_unit_test(test_rtr_sendmsg),  cmocka_unit_test(test_rtr_recv),
		cmocka_unit_test(test_rtr_recvfrom),  cmocka_unit_test(test_rtr_recvmsg),

		/* scanf funcitons */
		cmocka_unit_test(test_rtr_scanf),    cmocka_unit_test(test_rtr_fscanf),
		cmocka_unit_test(test_rtr_sscanf),   cmocka_unit_test(test_rtr_vscanf),
		cmocka_unit_test(test_rtr_vfscanf),  cmocka_unit_test(test_rtr_vsscanf),

		/* netdb functions */
		cmocka_unit_test(test_rtr_gethostbyname), cmocka_unit_test(test_rtr_gethostbyaddr),
		cmocka_unit_test(test_rtr_sethostent),    cmocka_unit_test(test_rtr_endhostent),
		cmocka_unit_test(test_rtr_gethostent),    cmocka_unit_test(test_rtr_gethostbyname2),
		cmocka_unit_test(test_rtr_getaddrinfo),      cmocka_unit_test(test_rtr_freeaddrinfo)
	};

	handle = dlopen("../src/v1/.libs/libretrace.so", RTLD_LAZY);
	if (!handle) {
		fputs(dlerror(), stderr);
		return 1;
	}

	ret = cmocka_run_group_tests(tests, NULL, NULL);

	dlclose(handle);

	return ret;
}
