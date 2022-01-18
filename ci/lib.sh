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
