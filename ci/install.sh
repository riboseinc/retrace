#!/bin/bash
set -x
set -eu

# cmocka
if [ ! -e "${CMOCKA_INSTALL}/lib/libcmocka.so" ]; then
	git clone git://git.cryptomilk.org/projects/cmocka.git ~/builds/cmocka
	cd ~/builds/cmocka
	git checkout tags/cmocka-1.1.1

	cd ~/builds/
	mkdir -p cmocka-build
	cd cmocka-build
	cmake -DCMAKE_INSTALL_PREFIX="${CMOCKA_INSTALL}" ~/builds/cmocka
	make all install
fi
