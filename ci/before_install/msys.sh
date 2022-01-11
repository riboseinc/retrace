#!/usr/bin/env bash
set -ex

msys_install() {
	packages=(
		automake
		autoconf
		make 
		pkg-config 
		libtool
		mingw-w64-x86_64-cmocka
		mingw64/mingw-w64-x86_64-ninja
		mingw64/mingw-w64-x86_64-cmake
		mingw64/mingw-w64-x86_64-graphviz # for doxygen's dot component
		openssl-devel
		doxygen
	)
	pacman --noconfirm -S --needed "${packages[@]}"
	gem install mustache
}
