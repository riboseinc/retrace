#!/usr/bin/env bash
set -ex

freebsd_install() {
	sudo pkg install -y cmake
	sudo pkg install -y ruby
	sudo pkg install -y devel/ruby-gems
	sudo pkg install -y bash
	sudo pkg install -y git
	sudo pkg install -y wget
	sudo pkg install -y autoconf
	sudo pkg install -y automake
	sudo pkg install -y libtool
	sudo pkg install -y pkgconf
	sudo gem install mustache
}
