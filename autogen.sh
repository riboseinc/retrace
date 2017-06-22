#!/bin/sh
if [ "$LIBTOOLIZE" = "" ] && [ "`uname`" = "Darwin" ]; then
  LIBTOOLIZE=glibtoolize
fi

ACLOCAL=${ACLOCAL:-aclocal}
AUTOCONF=${AUTOCONF:-autoconf}
AUTOMAKE=${AUTOMAKE:-automake}
AUTORECONF=${AUTORECONF:-autoreconf}
LIBTOOLIZE=${LIBTOOLIZE:-libtoolize}

"$LIBTOOLIZE" --copy
"$ACLOCAL" -I m4
"$AUTOCONF"
"$AUTORECONF"
"$AUTOMAKE" --add-missing --copy
