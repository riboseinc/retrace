#!/bin/bash
set -x
set -eu

SPWD="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

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

# checkpatch.pl
if [ ! -e "${CHECKPATCH_INSTALL}/checkpatch.pl" ]; then
	mkdir -p ${CHECKPATCH_INSTALL}
	cd ${CHECKPATCH_INSTALL}
	wget https://raw.githubusercontent.com/torvalds/linux/master/scripts/checkpatch.pl
	chmod a+x checkpatch.pl
	wget https://raw.githubusercontent.com/torvalds/linux/master/scripts/spelling.txt
	patch -p0 < $SPWD/checkpatch.pl.patch
	echo "invalid.struct.name" > const_structs.checkpatch
fi
