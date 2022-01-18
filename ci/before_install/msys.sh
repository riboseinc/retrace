#!/usr/bin/env bash
set -ex

msys_install() {
	packages=(
		autoconf
		automake
		cmake
		doxygen
		git
		gcc
		gettext-devel
                glib2-devel
		libtool
		make
		mingw-w64-x86_64-cmocka
		mingw-w64-x86_64-graphviz # for doxygen's dot component
		openssl-devel
		ruby
	)
	# pacman --noconfirm -Syu  # NOTE: would close the current terminal, thereby failing the build
	pacman --noconfirm -S --needed "${packages[@]}"

	gem install mustache
}
