AC_PREREQ(2.60)
AC_INIT([retrace], [0.1], [https://github.com/riboseinc/retrace/issues])
AM_INIT_AUTOMAKE([foreign subdir-objects -Wall])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])

AC_CANONICAL_HOST

AC_PROG_CC
AM_PROG_CC_C_O

AC_USE_SYSTEM_EXTENSIONS

m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
AC_PROG_LIBTOOL
AC_PROG_INSTALL

LT_INIT
AC_ENABLE_SHARED
AC_ENABLE_STATIC(no)

AC_PREFIX_DEFAULT([/usr])

AC_SEARCH_LIBS(socket, [socket network])
AC_SEARCH_LIBS(dlopen, [dl])
AC_SEARCH_LIBS(inet_aton, [resolv])

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
    CFLAGS="$CFLAGS -mmacosx-version-min=10.12"
    LDFLAGS="$LDFLAGS -mmacosx-version-min=10.12"
fi

# FreeBSD specific configuration
#if test "x$isfreebsd" = "xyes"; then
#fi

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


openssl=false
AC_MSG_CHECKING(for OpenSSL)
AC_ARG_WITH([openssl], AS_HELP_STRING(--with-openssl=PATH, base of OpenSSL installation),
  [
   AC_MSG_RESULT([$with_openssl])
   case $with_openssl in
   no)
     AC_MSG_NOTICE([OpenSSL not found, OpenSSL support disabled])
     ;;
   yes)
     AC_CHECK_HEADERS([openssl/ssl.h], [openssl=true], [AC_MSG_NOTICE([OpenSSL not found, OpenSSL support disabled])])
     ;;
   *)
     # determine if library is installed
     SAVE_CFLAGS=$CFLAGS
     CFLAGS="-I$with_openssl/include $CFLAGS"
     LIBSSL_FLAGS="-I$with_openssl/include"
     AC_CHECK_HEADERS([openssl/ssl.h], [openssl=true], [AC_MSG_NOTICE([OpenSSL not found, OpenSSL support disabled])])
     AC_SUBST(LIBSSL_FLAGS)
     CFLAGS=$SAVE_CFLAGS
     ;;
   esac
  ],
  [
     AC_MSG_RESULT([--with-openssl not specified])
     AC_CHECK_HEADERS([openssl/ssl.h],[openssl=true], [AC_MSG_NOTICE([OpenSSL not found, OpenSSL support disabled])])
  ]
)

AM_CONDITIONAL(OPENSSL, [test "$openssl" = "xtrue"])
if test "x$openssl" = "xtrue" ; then
    AC_DEFINE(USE_OPENSSL, [1], [Define to use OpenSSL])
else
    AC_DEFINE(USE_OPENSSL, [0], [Define to use OpenSSL])
fi


# Check for PTRACE features
AC_MSG_CHECKING([for PTRACE_GETREGS])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <stdlib.h>
#include <stddef.h>
#include <sys/ptrace.h>
#include <sys/user.h>
]], [[
  void *p;
  long res = ptrace(PTRACE_GETREGS, 0, p, p);
]])],
[
    AC_MSG_RESULT([yes])
    AC_DEFINE([HAVE_PTRACE_GETREGS], 1, [Define to 1 if you have the PTRACE_GETREGS ptrace request.])
], [
AC_MSG_RESULT([no])
])

AC_MSG_CHECKING([for PTRACE_GETREGSET])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <stdlib.h>
#include <stddef.h>
#include <sys/ptrace.h>
#include <sys/user.h>
]], [[
  void *p;
  long res = ptrace(PTRACE_GETREGSET, 0, p, p);
]])],
[
    AC_MSG_RESULT([yes])
    AC_DEFINE([HAVE_PTRACE_GETREGSET], 1, [Define to 1 if you have the PTRACE_GETREGSET ptrace request.])
], [
AC_MSG_RESULT([no])
])

AC_MSG_CHECKING([for PTRACE_SETREGSET])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <stdlib.h>
#include <stddef.h>
#include <sys/ptrace.h>
#include <sys/user.h>
]], [[
  void *p;
  long res = ptrace(PTRACE_SETREGSET, 0, p, p);
]])],
[
    AC_MSG_RESULT([yes])
    AC_DEFINE([HAVE_PTRACE_SETREGSET], 1, [Define to 1 if you have the PTRACE_SETREGSET ptrace request.])
], [
AC_MSG_RESULT([no])
])

AC_MSG_CHECKING([for PTRACE_SEIZE])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <stdlib.h>
#include <stddef.h>
#include <sys/ptrace.h>
#include <sys/user.h>
]], [[
  void *p;
  long res = ptrace(PTRACE_SEIZE, 0, p, p);
]])],
[
    AC_MSG_RESULT([yes])
    AC_DEFINE([HAVE_PTRACE_SEIZE], 1, [Define to 1 if you have the PTRACE_SEIZE ptrace request.])
], [
AC_MSG_RESULT([no])
])

AC_MSG_CHECKING([for PTRACE_INTERRUPT])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <stdlib.h>
#include <stddef.h>
#include <sys/ptrace.h>
#include <sys/user.h>
]], [[
  void *p;
  long res = ptrace(PTRACE_INTERRUPT, 0, p, p);
]])],
[
    AC_MSG_RESULT([yes])
    AC_DEFINE([HAVE_PTRACE_INTERRUPT], 1, [Define to 1 if you have the PTRACE_INTERRUPT ptrace request.])
], [
AC_MSG_RESULT([no])
])

AC_MSG_CHECKING([for PTRACE_LISTEN])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <stdlib.h>
#include <stddef.h>
#include <sys/ptrace.h>
#include <sys/user.h>
]], [[
  void *p;
  long res = ptrace(PTRACE_LISTEN, 0, p, p);
]])],
[
    AC_MSG_RESULT([yes])
    AC_DEFINE([HAVE_PTRACE_LISTEN], 1, [Define to 1 if you have the PTRACE_LISTEN ptrace request.])
], [
AC_MSG_RESULT([no])
])

AC_MSG_CHECKING([for PTRACE_PEEKSIGINFO])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <stdlib.h>
#include <stddef.h>
#include <sys/ptrace.h>
#include <sys/user.h>
]], [[
  void *p;
  long res = ptrace(PTRACE_PEEKSIGINFO, 0, p, p);
]])],
[
    AC_MSG_RESULT([yes])
    AC_DEFINE([HAVE_PTRACE_PEEKSIGINFO], 1, [Define to 1 if you have the PTRACE_PEEKSIGINFO ptrace request.])
], [
AC_MSG_RESULT([no])
])

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
AX_CHECK_COMPILE_FLAG([-fno-builtin], [TESTS_CFLAGS="$TESTS_CFLAGS -fno-builtin"],,[])
else
AC_MSG_NOTICE([no])
fi
AC_SUBST([TESTS_CFLAGS])



cmocka=false
if test "x$enable_tests" = "xyes" ; then
    SAVE_LDFLAGS=$LDFLAGS
    SAVE_CFLAGS=$CFLAGS

    CMOCKA_LIBS="-lcmocka"
    AC_MSG_CHECKING(for cmocka)
    AC_ARG_WITH(cmocka,
      AS_HELP_STRING(--with-cmocka=PATH, base of cmocka installation),
      [AC_MSG_RESULT([$with_cmocka])
       case $with_cmocka in
       no)
         ;;
       yes)
        AC_CHECK_HEADERS([cmocka.h],, [AC_MSG_ERROR([cmocka.h header not found or not usable])],
        [#include <stdarg.h>
         #include <stddef.h>
         #include <setjmp.h>])
        AC_CHECK_LIB(cmocka, [main], [CMOCKA_LIBS="-lcmocka"],[
                AC_MSG_ERROR([Could not find cmocka installation])
        ])
        cmocka=true
         ;;
       *)

        LDFLAGS="-L$with_cmocka/lib $LDFLAGS"
        CFLAGS="-I$with_cmocka/include $CFLAGS"

        AC_CHECK_HEADERS(cmocka.h,
         CMOCKA_CFLAGS="-I$with_cmocka/include"
         CMOCKA_LDFLAGS="-L$with_cmocka/lib",
         AC_MSG_ERROR([cmocka.h header not found or not usable]),
         [#include <stdarg.h>
          #include <stddef.h>
          #include <setjmp.h>]
        )

        AC_CHECK_LIB(cmocka, [main], [CMOCKA_LIBS="-lcmocka"],[
         AC_MSG_ERROR([Could not find cmocka installation])
        ])

        cmocka=true

        #restore
        LDFLAGS=$SAVE_LDFLAGS
        CFLAGS=$SAVE_CFLAGS
        ;;
       esac
      ],
      [AC_MSG_RESULT([--with-cmocka not specified])
        AC_CHECK_HEADERS([cmocka.h],,
        [
         cmocka=false
         AC_MSG_NOTICE([cmocka.h header not found or not usable])
         ],
        [#include <stdarg.h>
         #include <stddef.h>
        #include <setjmp.h>])

        AC_CHECK_LIB(cmocka, [main], [CMOCKA_LIBS="-lcmocka"],
        [
         cmocka=false
         AC_MSG_NOTICE([Could not find cmocka installation])
        ])
      ])
    AC_SUBST(CMOCKA_CFLAGS)
    AC_SUBST(CMOCKA_LDFLAGS)
    AC_SUBST(CMOCKA_LIBS)
fi

AM_CONDITIONAL([CMOCKA], [test "x$cmocka" = "xtrue"])
if test "x$cmocka" = "xtrue" ; then
    AC_DEFINE(USE_CMOCKA, [1], [Define to use cmocka])
else
    AC_DEFINE(USE_CMOCKA, [0], [Define to use cmocka])
fi

AC_ARG_ENABLE(debug,
     AS_HELP_STRING(--enable-debug, turn on debugging (disabled by default)),
     [], [enable_debug=no]
)
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


AC_CONFIG_FILES([Makefile test/Makefile])
AC_OUTPUT
