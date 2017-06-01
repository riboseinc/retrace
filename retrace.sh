#!/bin/bash

readonly __progname="$(basename $0)"

if [ -z "$1" ]; then
	echo "usage: ${__progname} <executable>"
	exit 1
fi

readonly so="./retrace.so"

if [ ! -f "${so}" ]; then
	echo "${__progname}: cannot open '${so}'"
	exit 1
fi

LD_PRELOAD="${so}" "$@"
