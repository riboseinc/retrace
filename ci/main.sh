#!/bin/bash
set -eu

LD_LIBRARY_PATH="${CMOCKA_INSTALL}/lib"
LDFLAGS="-L${CMOCKA_INSTALL}/lib"
CFLAGS="-I${CMOCKA_INSTALL}/include"

export LD_LIBRARY_PATH CFLAGS LDFLAGS

CHECKPATCH=$CHECKPATCH_INSTALL/checkpatch.pl

CHECKPATCH_FLAGS+=" --no-tree"
CHECKPATCH_FLAGS+=" --ignore COMPLEX_MACRO"
CHECKPATCH_FLAGS+=" --ignore TRAILING_SEMICOLON"
CHECKPATCH_FLAGS+=" --ignore LONG_LINE"
CHECKPATCH_FLAGS+=" --ignore LONG_LINE_STRING"
CHECKPATCH_FLAGS+=" --ignore LONG_LINE_COMMENT"
CHECKPATCH_FLAGS+=" --ignore SYMBOLIC_PERMS"
CHECKPATCH_FLAGS+=" --ignore NEW_TYPEDEFS"
CHECKPATCH_FLAGS+=" --ignore SPLIT_STRING"
CHECKPATCH_FLAGS+=" --ignore USE_FUNC"
CHECKPATCH_FLAGS+=" --ignore COMMIT_LOG_LONG_LINE"
CHECKPATCH_FLAGS+=" --ignore FILE_PATH_CHANGES"
CHECKPATCH_FLAGS+=" --ignore MISSING_SIGN_OFF"
CHECKPATCH_FLAGS+=" --ignore RETURN_PARENTHESES"
CHECKPATCH_FLAGS+=" --ignore STATIC_CONST_CHAR_ARRAY"
CHECKPATCH_FLAGS+=" --ignore ARRAY_SIZE"
CHECKPATCH_FLAGS+=" --ignore NAKED_SSCANF"
CHECKPATCH_FLAGS+=" --ignore SSCANF_TO_KSTRTO"

# checkpatch.pl will ignore the following paths
CHECKPATCH_IGNORE+=" checkpatch.pl.patch"
CHECKPATCH_EXCLUDE=$(for p in $CHECKPATCH_IGNORE; do echo ":(exclude)$p" ; done)

function _checkpatch() {
		$CHECKPATCH $CHECKPATCH_FLAGS --no-tree -
}

function checkpatch() {
		git show --oneline --no-patch $1
		git format-patch -1 $1 --stdout -- $CHECKPATCH_EXCLUDE . | _checkpatch
}

sh autogen.sh && ./configure --disable-silent-rules --enable-tests --with-cmocka=${CMOCKA_INSTALL} && make clean && make
make check

# checkpatch
if [ "$TRAVIS_PULL_REQUEST" == "false" ]; then
	checkpatch HEAD
else
	for c in $(git rev-list HEAD^1..HEAD^2); do
		checkpatch $c
	done
fi
