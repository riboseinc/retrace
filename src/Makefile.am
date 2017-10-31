
ACLOCAL_AMFLAGS = -I ../m4

lib_LTLIBRARIES= libretrace.la
libretrace_la_LDFLAGS= -no-undefined -avoid-version
libretrace_la_CFLAGS= $(AM_CFLAGS) $(OPENSSL_INCLUDES)
libretrace_la_LDFLAGS+= $(OPENSSL_LDFLAGS)

# include all headers here
noinst_HEADERS= \
	char.h \
	common.h \
	dir.h \
	dlopen.h \
	env.h \
	exec.h \
	exit.h \
	file.h \
	fork.h \
	httpredirect.h \
	id.h \
	log.h \
	malloc.h \
	netfuzz.h \
	rtr-netdb.h \
	perror.h \
	pipe.h \
	pledge.h \
	popen.h \
	printf.h \
	read.h \
	rtr-time.h \
	scanf.h \
	select.h \
	sock.h \
	ssl.h \
	str.h \
	strinject.h \
	temp.h \
	trace.h \
	write.h \
	ioctl.h

# include all C files here
libretrace_la_SOURCES= \
	char.c \
	common.c \
	dir.c \
	dlopen.c \
	env.c \
	exec.c \
	exit.c \
	file.c \
	fork.c \
	httpredirect.c \
	id.c \
	log.c \
	malloc.c \
	netdb.c \
	netfuzz.c \
	perror.c \
	pipe.c \
	pledge.c \
	popen.c \
	printf.c \
	read.c \
	scanf.c \
	select.c \
	sock.c \
	ssl.c \
	str.c \
	strinject.c \
	temp.c \
	time.c \
	trace.c \
	write.c \
	ioctl.c
