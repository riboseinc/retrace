#!/usr/bin/env bash
set -ex

macos_install() {
	packages=(
		autoconf
		automake
		cmake
		cmocka
		libtool
		make
		openssl@1.1
		pkg-config
		ruby
	)

	for p in "${packages[@]}"; do
		echo "brew '${p}'" >> Brewfile
	done

	brew update-reset
	# brew uninstall --ignore-dependencies openssl
	brew bundle
	mkdir -p "${CMOCKA_INSTALL}"
	brew unlink openssl
	brew link --force openssl@1.1
	gem install mustache
}
