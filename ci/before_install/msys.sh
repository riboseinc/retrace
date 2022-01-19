#!/usr/bin/env bash
set -ex

msys_install() {
	packages=(
		autoconf
		automake
		cmake
		doxygen
		gcc
		gettext-devel
		git
		glib2-devel
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
