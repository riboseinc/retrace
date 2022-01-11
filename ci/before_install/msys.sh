#!/usr/bin/env bash
set -ex

msys_install() {
	packages=(
		automake
		autoconf
		cmake
		doxygen
		git
		gcc
		libtool
		make
		mingw-w64-x86_64-cmocka
		mingw-w64-x86_64-graphviz # for doxygen's dot component
		openssl-devel
		ruby
	)
	pacman --noconfirm -S --needed "${packages[@]}"
	gem install mustache
}
