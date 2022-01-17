#!/usr/bin/env bash
set -ex

msys_install() {
	## XXX DEBUG: start
	echo '$ACLOCAL_PATH' === "$ACLOCAL_PATH"
	while read -r i; do echo ACLOCAL_PATH is $i . ; ls -la "$i" || : ; ls -la "$i/xsize.m4" || : ; done <<< "${ACLOCAL_PATH//:/$IFS}"
	## XXX DEBUG: end

	packages=(
		autoconf
		automake
		cmake
		doxygen
		gcc
		git
		libtool
		make
		mingw-w64-x86_64-cmocka
		mingw-w64-x86_64-libtool
		mingw-w64-x86_64-pkg-config
		mingw-w64-x86_64-toolchain
		mingw64/mingw-w64-x86_64-graphviz # for doxygen's dot component
		ninja
		openssl-devel
		pkg-config
	)
	# pacman --noconfirm -Syu  # NOTE: would close the current terminal, thereby failing the build
	pacman --noconfirm -S --needed "${packages[@]}"

	## XXX DEBUG: start
	echo '$ACLOCAL_PATH' === "$ACLOCAL_PATH"
	while read -r i; do echo ACLOCAL_PATH is $i . ; ls -la "$i" || : ; ls -la "$i/xsize.m4" || : ; done <<< "${ACLOCAL_PATH//:/$IFS}"
	## XXX DEBUG: end

	gem install mustache
}
