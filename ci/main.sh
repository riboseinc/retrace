#!/bin/bash
set -eu

if [[ -n "${CMOCKA_INSTALL:-}" ]]
then
	LD_LIBRARY_PATH="${CMOCKA_INSTALL}/lib"
	LDFLAGS="-L${CMOCKA_INSTALL}/lib"
	CFLAGS="-I${CMOCKA_INSTALL}/include"

	export LD_LIBRARY_PATH CFLAGS LDFLAGS
fi

. ci/lib.sh

: "${SUDO:=$(get_sudo)}"

test_retrace() {
	{ [[ ! -r Makefile ]] || make clean ; } && \
	sh autogen.sh && \
	./configure "$@" && \
	make ${MAKE_FLAGS:+$MAKE_FLAGS}

	$SUDO make install
	make check
}

test_retracev1() {
	test_retrace \
		--disable-silent-rules \
		${CMOCKA_INSTALL:+--with-cmocka="${CMOCKA_INSTALL}"} \
		--enable-tests
}

test_retracev2() {
	MAKE_FLAGS=V=1 test_retrace \
		--enable-v2 \
		--enable-tests
}

test_retracev2wrapper() {
	MAKE_FLAGS=V=1 test_retrace \
		--enable-v2 \
		--enable-v2_wrapper \
		--enable-tests
}

main() {
	# Run these tests by default
	if [[ $# -lt 1 ]]
	then
		test_retracev1
		test_retracev2
		test_retracev2wrapper
	else
		for arg in "$@"
		do
			test_retrace"$arg"
		done
	fi
}

main "$@"
