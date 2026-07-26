#!/bin/bash
set -eu

# CMake-only build driver. Was the Autotools entry point; v1 + Autotools
# were removed in Phase 9 (ADR-0011). CMake workflows (.github/workflows/,
# .cirrus.yml) invoke cmake/ninja directly -- this script is kept only as
# a documented entry point for local development.

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
	cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
		-DRETRACE_BUILD_TESTS=ON \
		${CMOCKA_INSTALL:+-DCMAKE_PREFIX_PATH="${CMOCKA_INSTALL}"}

	cmake --build build

	$SUDO cmake --install build
	ctest --test-dir build --output-on-failure
}

main() {
	test_retrace
}

main "$@"
