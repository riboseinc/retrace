ACLOCAL_AMFLAGS = -I m4

SUBDIRS= test

lib_LTLIBRARIES= libretrace.la
libretrace_la_LDFLAGS= -no-undefined -avoid-version
libretrace_la_CFLAGS= $(AM_CFLAGS) $(LIBSSL_FLAGS)

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
	id.h \
	malloc.h \
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
	str.h \
	trace.h \
	write.h

if OPENSSL
	noinst_HEADERS+= ssl.h
endif

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
	id.c \
	malloc.c \
	perror.c \
	pipe.c \
	pledge.c \
	popen.c \
	printf.c \
	read.c \
	scanf.c \
	select.c \
	sock.c \
	str.c \
	time.c \
	trace.c \
	write.c

if OPENSSL
	libretrace_la_SOURCES+= ssl.c
endif


retrace: retrace.in
	@sed -e "s|\@libdir\@|$(libdir)|" $< > $@
	chmod +x $@

dist_bin_SCRIPTS= retrace
EXTRA_DIST= retrace.in autogen.sh
CLEANFILES = $(dist_bin_SCRIPTS)
