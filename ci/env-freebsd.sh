#!/bin/sh
set -ex

export PATH=/usr/local/bin:$PATH
export MD5SUM=/sbin/md5sum
export CMOCKA_INSTALL=$(pwd)/builds/cmocka-install
export CHECKPATCH_INSTALL=$(pwd)/builds/checkpatch-install
