#!/usr/bin/env bash
set -ex

msys_install() {
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
	gem install mustache
}
