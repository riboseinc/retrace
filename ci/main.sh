#!/bin/bash
set -eu

LD_LIBRARY_PATH="${CMOCKA_INSTALL}/lib"
LDFLAGS="-L${CMOCKA_INSTALL}/lib"
CFLAGS="-I${CMOCKA_INSTALL}/include"

export LD_LIBRARY_PATH CFLAGS LDFLAGS

make -j2
make test
