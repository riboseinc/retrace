#!/usr/bin/env bash
set -ex

# https://gist.github.com/marcusandre/4b88c2428220ea255b83
get_os() {
	if [ -z "$OSTYPE" ]; then
		uname
	else
		echo "$OSTYPE"
	fi | tr '[:upper:]' '[:lower:]'
}

crossplat_install() {
	echo ""
}

main() {
	case $(get_os) in
		freebsd*)
			. ci/before_install/freebsd.sh
			freebsd_install ;;
		netbsd*)
			. ci/before_install/netbsd.sh
			netbsd_install ;;
		openbsd*)
			. ci/before_install/openbsd.sh
			openbsd_install ;;
		darwin*)
			. ci/before_install/darwin.sh
			macos_install ;;
		linux*)
			. ci/before_install/linux.sh
			linux_install ;;
		msys*)
			. ci/before_install/msys.sh
			msys_install ;;
		*) echo "unknown"; exit 1 ;;
	esac

	crossplat_install
}

main
