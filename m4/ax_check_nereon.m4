# SYNOPSIS
#
#   AX_CHECK_NEREON([action-if-found[, action-if-not-found]])
#
# DESCRIPTION
#
#   Look for nereon in a number of default spots, or in a user-selected
#   spot (via --with-nereon).  Sets
#
#     NEREON_INCLUDES to the include directives required
#     NEREON_LIBS to the -l directives required
#     NEREON_LDFLAGS to the -L or -R flags required
#     NEREON_TO_CC to the nereon_cc_bin path required
#
#   and calls ACTION-IF-FOUND or ACTION-IF-NOT-FOUND appropriately
#
#   This macro sets NEREON_INCLUDES such that source files should include
#   nereon.h like so:
#
#     #include <nereon.h>

AU_ALIAS([CHECK_NEREON], [AX_CHECK_NEREON])
AC_DEFUN([AX_CHECK_NEREON], [
    found=false
    AC_ARG_WITH([nereon],
        [AS_HELP_STRING([--with-nereon=DIR],
            [root of the nereon directory])],
        [
            case "$withval" in
            "" | y | ye | yes | n | no)
            AC_MSG_ERROR([Invalid --with-nereon value])
              ;;
            *) nereondirs="$withval"
              ;;
            esac
        ], [
            nereondirs="/usr/local/nereon /usr/lib/nereon /usr/nereon /usr/pkg /usr/local /usr"
        ]
        )

    NEREON_INCLUDES=
    for nereondir in $nereondirs; do
        AC_MSG_CHECKING([for nereon in $nereondir])
        if test -f "$nereondir/include/nereon.h"; then
            NEREON_INCLUDES="-I$nereondir/include/"
            NEREON_LDFLAGS="-L$nereondir/lib"
            NEREON_LIBS="-lnereon"
            NEREON_LIBDIR="$nereondir/lib"
            NEREON_TO_CC="$nereondir/bin/nos2cc"
            found=true
            AC_MSG_RESULT([yes])
            break
        else
            AC_MSG_RESULT([no])
        fi
    done

    # try the preprocessor and linker with our new flags,
    # being careful not to pollute the global LIBS, LDFLAGS, and CPPFLAGS

    AC_MSG_CHECKING([whether compiling and linking against nereon works])
    echo "Trying link with NEREON_LDFLAGS=$NEREON_LDFLAGS;" \
        "NEREON_LIBS=$NEREON_LIBS; NEREON_INCLUDES=$NEREON_INCLUDES" >&AS_MESSAGE_LOG_FD

    save_LIBS="$LIBS"
    save_LDFLAGS="$LDFLAGS"
    save_CPPFLAGS="$CPPFLAGS"
    LDFLAGS="$LDFLAGS $NEREON_LDFLAGS"
    LIBS="$NEREON_LIBS $LIBS"
    CPPFLAGS="$NEREON_INCLUDES $CPPFLAGS"
    AC_LINK_IFELSE(
        [AC_LANG_PROGRAM([#include <nereon.h>], [nereon_get_errmsg()])],
        [
            AC_MSG_RESULT([yes])
            $1
        ], [
            AC_MSG_RESULT([no])
            $2
        ])
    CPPFLAGS="$save_CPPFLAGS"
    LDFLAGS="$save_LDFLAGS"
    LIBS="$save_LIBS"

    AC_SUBST([NEREON_INCLUDES])
    AC_SUBST([NEREON_LIBS])
    AC_SUBST([NEREON_LDFLAGS])
    AC_SUBST([NEREON_LIBDIR])
    AC_SUBST([NEREON_TO_CC])
])
