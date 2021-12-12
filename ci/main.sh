#!/bin/bash
set -eu

LD_LIBRARY_PATH="${CMOCKA_INSTALL}/lib"
LDFLAGS="-L${CMOCKA_INSTALL}/lib"
CFLAGS="-I${CMOCKA_INSTALL}/include"

export LD_LIBRARY_PATH CFLAGS LDFLAGS

install_retrace() {
	sh autogen.sh && \
		./configure \
			--disable-silent-rules \
			--with-cmocka="${CMOCKA_INSTALL}" \
			--enable-tests && \
		make clean && \
		make

	sudo make install
	make check
}

test_retrace() {
	make clean
	./configure \
		--enable-v2 \
		--enable-tests && \
		make clean
		make V=1

	sudo make install
	make check
}

install_retrace
test_retrace
