#!/bin/bash

readonly __progname="$(basename $0)"

errx() {
	echo -e "${__progname}: $@" >&2
	exit 1
}

usage() {
	echo "usage: ${__progname} <executable>"
	exit 1
}

main() {
	[ -z "$1" ] && \
		usage

	if [[ "${RETRACE_CONFIG}" ]]; then
		readonly local config="${RETRACE_CONFIG}"

		[ ! -f "${config}" ] && \
			errx "cannot open '${config}'"
	fi

	if $(uname | grep -q ^Darwin); then
		readonly lib="./retrace.dylib"
		[ ! -f "${lib}" ] && \
			errx "cannot open '${lib}'"

		DYLD_FORCE_FLAT_NAMESPACE=1 DYLD_INSERT_LIBRARIES="${lib}" "$@"
	else
		readonly lib="./retrace.so"
		[ ! -f "${lib}" ] && \
			errx "cannot open '${lib}'"

		LD_PRELOAD="${lib}" "$@"
	fi
}

main "$@"

exit $?
