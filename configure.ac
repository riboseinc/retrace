AC_PREREQ([2.69])
AC_INIT([retrace], [2.0.1], [https://github.com/riboseinc/retrace/issues])
AM_INIT_AUTOMAKE([foreign subdir-objects -Wall])

AM_GNU_GETTEXT_VERSION([0.19.8])
AM_GNU_GETTEXT_REQUIRE_VERSION([0.19.8])
AM_GNU_GETTEXT([external])

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])

AC_CANONICAL_HOST

AC_PROG_CC([egcc gcc cl cc])
AM_PROG_CC_C_O

AC_USE_SYSTEM_EXTENSIONS

m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
LT_INIT
AC_PROG_INSTALL
AM_PROG_AS

LT_INIT
AC_ENABLE_SHARED
AC_ENABLE_STATIC(no)

AC_PREFIX_DEFAULT([/usr/local])

AC_SEARCH_LIBS(socket, [socket network])
AC_SEARCH_LIBS(dlopen, [dl])
AC_SEARCH_LIBS(inet_aton, [resolv])
AC_SEARCH_LIBS([pthread_create], [pthread])

# list all used system headers for checking
AC_CHECK_HEADERS_ONCE(m4_flatten([
    ctype.h
    dlfcn.h
    dirent.h
    lwp.h
    pthread.h
    pthread_np.h
    pwd.h
    runetype.h
    string.h
    stdio.h
    string.h
    stdarg.h
    stdlib.h
    syscall.h
    stdarg.h
    time.h
    unistd.h
    execinfo.h
    sys/stat.h
    sys/utsname.h
    sys/time.h
    sys/types.h
]))

# add more host definitions if needed.
islinux=no
isdarwin=no
isfreebsd=no
isopenbsd=no

case $host_os in
  linux* )
    islinux=yes
  ;;
  darwin* )
    isdarwin=yes
  ;;
  *freebsd* )
    isfreebsd=yes
  ;;
  openbsd* )
    isopenbsd=yes
  ;;
esac

AM_CONDITIONAL([LINUX],   [test "x$islinux" = "xyes"])
AM_CONDITIONAL([DARWIN],  [test "x$isdarwin" = "xyes"])
AM_CONDITIONAL([FREEBSD], [test "x$isfreebsd" = "xyes"])
AM_CONDITIONAL([OPENBSD], [test "x$isopenbsd" = "xyes"])

# Linux specific configuration
#if test "x$islinux" = "xyes"; then
#fi

# MacOS specific configuration
if test "x$isdarwin" = "xyes"; then
    CFLAGS="$CFLAGS -mmacosx-version-min=10.12 -D_DARWIN_C_SOURCE"
    LDFLAGS="$LDFLAGS -mmacosx-version-min=10.12"
fi

# FreeBSD specific configuration
#if test "x$isfreebsd" = "xyes"; then
#fi

# OpenBSD specific configuration
if test "x$isopenbsd" = "xyes"; then
    AC_DEFINE([STAILQ_INIT], [SIMPLEQ_INIT], [Define to SIMPLEQ_ for OpenBSD])
    AC_DEFINE([STAILQ_ENTRY], [SIMPLEQ_ENTRY], [Define to SIMPLEQ_ for OpenBSD])
    AC_DEFINE([STAILQ_HEAD], [SIMPLEQ_HEAD], [Define to SIMPLEQ_ for OpenBSD])
    AC_DEFINE([STAILQ_INSERT_TAIL], [SIMPLEQ_INSERT_TAIL], [Define to SIMPLEQ_ for OpenBSD])
    AC_DEFINE([STAILQ_FIRST], [SIMPLEQ_FIRST], [Define to SIMPLEQ_ for OpenBSD])
    AC_DEFINE([STAILQ_NEXT], [SIMPLEQ_NEXT], [Define to SIMPLEQ_ for OpenBSD])
    AC_DEFINE([STAILQ_FOREACH], [SIMPLEQ_FOREACH], [Define to SIMPLEQ_ for OpenBSD])
fi

# Test for __atomic support
AC_MSG_CHECKING([for gcc __atomic builtins])
AC_LINK_IFELSE(
 [AC_LANG_PROGRAM([int i;],
              [__atomic_load_n (&i, __ATOMIC_ACQUIRE);
           __atomic_store_n (&i, 1, __ATOMIC_RELEASE);])],
 [have_atomic_builtins=yes],
 [have_atomic_builtins=no])
if test "x$have_atomic_builtins" = "xyes"; then
  AC_DEFINE([HAVE_ATOMIC_BUILTINS], [1],
    [Define to 1 if gcc __atomic builtins are available])
fi
AC_MSG_RESULT([$have_atomic_builtins])

AC_CHECK_DECLS([O_TMPFILE], [], [], [#include <fcntl.h>])

# Check for PTRACE features
AC_CHECK_DECLS([PTRACE_GETREGS, PTRACE_GETREGSET, PTRACE_SETREGSET], [], [], [#include <sys/ptrace.h.h>])
AC_CHECK_DECLS([PTRACE_SEIZE], [], [], [#include <sys/ptrace.h.h>])
AC_CHECK_DECLS([PTRACE_INTERRUPT], [], [], [#include <sys/ptrace.h.h>])
AC_CHECK_DECLS([PTRACE_LISTEN], [], [], [#include <sys/ptrace.h.h>])
AC_CHECK_DECLS([PTRACE_PEEKSIGINFO], [], [], [#include <sys/ptrace.h.h>])

# Check fcntl features
AC_CHECK_DECLS([F_DUPFD_CLOEXEC], [], [], [#include <fcntl.h>])
AC_CHECK_DECLS([F_SETOWN, F_GETOWN], [], [], [#include <fcntl.h>])
AC_CHECK_TYPES([struct flock], [], [], [#include <fcntl.h>])
AC_CHECK_DECLS([F_SETLKW], [], [], [#include <fcntl.h>])
AC_CHECK_DECLS([F_GETPATH], , [], [#include <fcntl.h>])
AC_CHECK_TYPES([struct fstore], [], [], [#include <fcntl.h>])
AC_CHECK_DECLS([F_PREALLOCATE], [], [], [#include <fcntl.h>])
AC_CHECK_TYPES([struct fpunchhole], [], [], [#include <fcntl.h>])
AC_CHECK_DECLS([F_PUNCHHOLE], [], [], [#include <fcntl.h>])
AC_CHECK_DECLS([F_SETSIZE], [], [], [#include <fcntl.h>])
AC_CHECK_TYPES([struct radvisory], [], [], [#include <fcntl.h>])
AC_CHECK_DECLS([F_RDADVISE], [], [], [#include <fcntl.h>])
AC_CHECK_DECLS([F_RDAHEAD], [], [], [#include <fcntl.h>])
AC_CHECK_TYPES([struct fbootstraptransfer], [], [], [#include <fcntl.h>])
AC_CHECK_DECLS([F_READBOOTSTRAP, F_WRITEBOOTSTRAP], [], [], [#include <fcntl.h>])
AC_CHECK_DECLS([F_NOCACHE], [], [], [#include <fcntl.h>])
AC_CHECK_TYPES([struct log2phys], [], [], [#include <fcntl.h>])
AC_CHECK_DECLS([F_LOG2PHYS, F_LOG2PHYS_EXT], [], [], [#include <fcntl.h>])
AC_CHECK_DECLS([F_FULLFSYNC], [], [], [#include <fcntl.h>])
AC_CHECK_DECLS([F_SETNOSIGPIPE, F_GETNOSIGPIPE], [], [], [#include <fcntl.h>])
AC_CHECK_DECLS([F_OFD_SETLK, F_OFD_SETLKW, F_OFD_GETLK], [], [], [#include <fcntl.h>])
AC_CHECK_TYPES([struct f_owner_ex], [], [], [#include <fcntl.h>])
AC_CHECK_DECLS([F_GETOWN_EX, F_SETOWN_EX], [], [], [#include <fcntl.h>])
AC_CHECK_DECLS([F_GETSIG, F_SETSIG], [], [], [#include <fcntl.h>])
AC_CHECK_DECLS([F_SETLEASE, F_GETLEASE], [], [], [#include <fcntl.h>])
AC_CHECK_DECLS([F_NOTIFY], [], [], [#include <fcntl.h>])
AC_CHECK_DECLS([F_SETPIPE_SZ, F_GETPIPE_SZ], [], [], [#include <fcntl.h>])
AC_CHECK_DECLS([F_ADD_SEALS, F_GET_SEALS], [], [], [#include <fcntl.h>])

AC_CHECK_DECLS([SOCK_PACKET], [], [], [#include <sys/socket.h>])

AC_CHECK_DECLS([SO_BINDTODEVICE], [], [], [#include <sys/socket.h>])
AC_CHECK_DECLS([SO_BSDCOMPAT], [], [], [#include <sys/socket.h>])
AC_CHECK_DECLS([SO_DOMAIN], [], [], [#include <sys/socket.h>])
AC_CHECK_DECLS([SO_MARK], [], [], [#include <sys/socket.h>])
AC_CHECK_DECLS([SO_PASSCRED], [], [], [#include <sys/socket.h>])
AC_CHECK_DECLS([SO_PEERCRED], [], [], [#include <sys/socket.h>])
AC_CHECK_DECLS([SO_PRIORITY], [], [], [#include <sys/socket.h>])
AC_CHECK_DECLS([SO_PROTOCOL], [], [], [#include <sys/socket.h>])
AC_CHECK_DECLS([SO_RXQ_OVFL], [], [], [#include <sys/socket.h>])
AC_CHECK_DECLS([SO_SNDBUFFORCE], [], [], [#include <sys/socket.h>])
AC_CHECK_DECLS([SO_RCVBUFFORCE], [], [], [#include <sys/socket.h>])
AC_CHECK_DECLS([SO_BUSY_POLL], [], [], [#include <sys/socket.h>])

AX_CHECK_OPENSSL([], AC_MSG_ERROR([]))

AX_CHECK_CMOCKA([cmocka=true], )
AM_CONDITIONAL([CMOCKA], [test "x$cmocka" = "xtrue"])
if test "x$cmocka" = "xtrue" ; then
    AC_DEFINE(USE_CMOCKA, [1], [Define to use cmocka])
else
    AC_DEFINE(USE_CMOCKA, [0], [Define to use cmocka])
fi

# Check if the C compiler recognizes a variant of restrict keyword
AC_C_RESTRICT
# Check if _IO_putc exists
AC_CHECK_DECLS([_IO_putc], [], [], [#include <stdio.h>])
# Check second parameter of gettimeofday
AC_MSG_CHECKING(gettimeofday second parameter)
AC_LANG_WERROR
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <sys/time.h>]], 
                  [[struct timeval tv; int tz; gettimeofday(&tv, &tz)]])],
                   [with_struct_timezone=no],[with_struct_timezone=yes])

if test "$with_struct_timezone" = no; then
  AC_DEFINE(HAVE_GETTIMEOFDAY_WITHOUT_TIMEZONE, 1,
   [define to 1 if gettimeofday second argument is const void*; assume const struct timezone* otherwise])
  AC_MSG_RESULT((assumed) const struct timezone*) 
else  
  AC_MSG_RESULT(const void*) 
fi


TESTS_CFLAGS=""
AC_MSG_CHECKING(whether to run tests)
AC_ARG_ENABLE([tests],
   [AS_HELP_STRING([--enable-tests], [enable running tests on make check (default is NO)])],
   [enable_tests=${enableval}],
   [enable_tests=no])
AC_MSG_RESULT($enable_test)
AM_CONDITIONAL([ENABLE_TESTS], [test "x$enable_tests" = "xyes"])
if test "x$enable_tests" = "xyes" ; then
AC_MSG_NOTICE([yes])
TESTS_CFLAGS="$TESTS_CFLAGS -fno-builtin"
else
AC_MSG_NOTICE([no])
fi
AC_SUBST([TESTS_CFLAGS])

AC_ARG_ENABLE(debug,
     AS_HELP_STRING(--enable-debug, turn on debugging (disabled by default)),
     [], [enable_debug=no]
)

# Disable optimizations always for now as -O2 is crashing v2
CFLAGS="$CFLAGS -O0"
if test "x$enable_debug" = "xyes" ; then
    CFLAGS="$CFLAGS -g3 -O0 -DDEBUG"
fi

AC_ARG_ENABLE(profiling,
     AS_HELP_STRING(--enable-profiling, turn on profiling (disabled by default)),
     [], [enable_profiling=no]
)
if test "x$enable_profiling" = "xyes" ; then
    CFLAGS="$CFLAGS -pg"
fi

AC_MSG_CHECKING(whether to enable retrace v2)
AC_ARG_ENABLE(
  [v2],
  [AS_HELP_STRING([--enable-v2], [enable retrace version 2 @<:@default=no@:>@])],
  ,
  [enable_v2="no"]
)
AC_MSG_RESULT($enable_v2)
AM_CONDITIONAL([RTR_V2], [test "x$enable_v2" = "xyes"])
if test "x$enable_v2" = "xyes" ; then
    AC_DEFINE(RTR_V2, [1], [Define to use retrace v2])
else
    AC_DEFINE(RTR_V2, [0], [Define to use retrace v2])
fi

AC_MSG_RESULT($enable_v2_wrapper)
AM_CONDITIONAL([RTR_V2_WRAPPER], [test "x$enable_v2_wrapper" = "xyes"])
if test "x$enable_v2_wrapper" = "xyes" ; then
    AX_CHECK_NEREON([], AC_MSG_ERROR([]))
    AC_DEFINE(RTR_V2_WRAPPER, [1], [Define to use retrace v2 wrapper])
else
    AC_DEFINE(RTR_V2_WRAPPER, [0], [Define to use retrace v2 wrapper])
fi

AC_MSG_CHECKING(whether to enable rpc)
AC_ARG_ENABLE(rpc,
     AS_HELP_STRING(--enable-rpc, turn on rpc (disabled by default)),
     [], [enable_rpc=no]
)
AC_MSG_RESULT($enable_rpc)
AM_CONDITIONAL([RTR_RPC], [test "x$enable_rpc" = "xyes"])
if test "x$enable_rpc" = "xyes" ; then
    AC_DEFINE(RTR_RPC, [1], [Define to use rpc])
else
    AC_DEFINE(RTR_RPC, [0], [Define to use rpc])
fi

backtrace=false
AC_CHECK_HEADERS([execinfo.h], [backtrace=true], [AC_MSG_NOTICE([execinfo.h not found, backtrace disabled])])
if test "x$backtrace" = "xtrue" ; then
    AC_DEFINE(BACKTRACE, [1], [Define to enable backtrace])
else
    AC_DEFINE(BACKTRACE, [0], [Define to enable backtrace])
fi

AC_CONFIG_FILES([Makefile
                 src/Makefile
                 src/v1/Makefile
                 src/v2/Makefile
                 test/Makefile
                 rpc/Makefile
                 tools/Makefile
                 tools/stringinjector/Makefile
                 tools/spawn/Makefile
                 ])
AC_OUTPUT
