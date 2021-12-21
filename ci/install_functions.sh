#!/bin/bash
set -x
set -eu

SPWD="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

CORES="2" && [ -r /proc/cpuinfo ] && CORES=$(grep -c '^$' /proc/cpuinfo)

: "${CMOCKA_VERSION:=1.1.1}"
: "${LIBNEREON_VERSION:=v0.9.4}"

# cmocka
install_cmocka() {
	if [ ! -e "${CMOCKA_INSTALL}/lib/libcmocka.so" ] && [ ! -e "${CMOCKA_INSTALL}/lib/libcmocka.dylib" ]; then
		git clone https://git.cryptomilk.org/projects/cmocka.git ~/builds/cmocka
		cd ~/builds/cmocka
		git checkout tags/cmocka-"${CMOCKA_VERSION}"

		cd ~/builds/
		mkdir -p cmocka-build
		cd cmocka-build
		cmake \
			-DCMAKE_INSTALL_DIR="${CMOCKA_INSTALL}" \
			-DLIB_INSTALL_DIR="${CMOCKA_INSTALL}/lib" \
			-DINCLUDE_INSTALL_DIR="${CMOCKA_INSTALL}/include" \
			~/builds/cmocka
		make -j"${CORES}" all install
	fi
}

# checkpatch.pl
install_checkpatch() {
	if [ ! -e "${CHECKPATCH_INSTALL}/checkpatch.pl" ]; then
		mkdir -p "${CHECKPATCH_INSTALL}"
		cd "${CHECKPATCH_INSTALL}"
		wget https://raw.githubusercontent.com/torvalds/linux/master/scripts/checkpatch.pl
		chmod a+x checkpatch.pl
		wget https://raw.githubusercontent.com/torvalds/linux/master/scripts/spelling.txt
		patch -p0 < "$SPWD"/checkpatch.pl.patch
		echo "invalid.struct.name" > const_structs.checkpatch
		echo "JSON_Object" > typedefs.checkpatch
	fi
}

# install libnereon
install_libnereon() {
	git clone -b "${LIBNEREON_VERSION}" https://github.com/riboseinc/libnereon
	cd libnereon
	mkdir build
	cd build
	cmake ..
	ls -la
	ls -la ..
	make
	sudo make install
}
