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

get_cores() {
	local cores=2
	if [[ -r /proc/cpuinfo ]]
	then
		cores=$(grep -c '^$' /proc/cpuinfo)
	fi
	echo "$cores"
}

get_sudo() {
	if command -v sudo 2>/dev/null >&2
	then
		command -v sudo
	fi
}
