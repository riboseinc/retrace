#!/bin/bash
set -x
set -eu

. ci/install_functions.sh

for item in "$@"
do
	install_"${item}"
done
