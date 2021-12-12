#!/bin/sh
set -ex

export PATH=/usr/local/bin:$PATH
export MD5SUM=/bin/md5
export CMOCKA_INSTALL=$(pwd)/builds/cmocka-install
export CHECKPATCH_INSTALL=$(pwd)/builds/checkpatch-install
export AUTOCONF_VERSION=2.69
export AUTOMAKE_VERSION=1.15
