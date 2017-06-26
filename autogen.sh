#!/bin/sh
if [ "$LIBTOOLIZE" = "" ] && [ "`uname`" = "Darwin" ]; then
    LIBTOOLIZE=glibtoolize
fi

ACLOCAL=${ACLOCAL:-aclocal}
AUTOCONF=${AUTOCONF:-autoconf}
AUTOMAKE=${AUTOMAKE:-automake}
AUTORECONF=${AUTORECONF:-autoreconf}
LIBTOOLIZE=${LIBTOOLIZE:-libtoolize}

if [ -x "`which autoreconf 2>/dev/null`" ] ; then
    exec autoreconf -ivf
fi

"$ACLOCAL" -I m4
"$LIBTOOLIZE" --copy
"$AUTOCONF"
"$AUTOMAKE" --add-missing --force-missing --copy
