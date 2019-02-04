#!/bin/bash
set -eu

CHECKPATCH=$CHECKPATCH_INSTALL/checkpatch.pl
CHECKPATCH_TYPEDEFS=$CHECKPATCH_INSTALL/typedefs.checkpatch

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
CHECKPATCH_FLAGS+=" --ignore EXECUTE_PERMISSIONS"
CHECKPATCH_FLAGS+=" --ignore MULTISTATEMENT_MACRO_USE_DO_WHILE"
CHECKPATCH_FLAGS+=" --ignore STORAGE_CLASS"
CHECKPATCH_FLAGS+=" --ignore SPDX_LICENSE_TAG"
CHECKPATCH_FLAGS+=" --ignore PREFER_ALIGNED"
CHECKPATCH_FLAGS+=" --ignore EMAIL_SUBJECT"

# checkpatch.pl will ignore the following paths
CHECKPATCH_IGNORE+=" checkpatch.pl.patch Makefile test/Makefile test/http.redirect/hello.txt src/v2/parson.c src/v2/parson.h *.json"
CHECKPATCH_EXCLUDE=$(for p in $CHECKPATCH_IGNORE; do echo ":(exclude)$p" ; done)

function _checkpatch() {
		$CHECKPATCH $CHECKPATCH_FLAGS --typedefsfile=$CHECKPATCH_TYPEDEFS --no-tree -
}

function checkpatch() {
		git show --oneline --no-patch $1
		git format-patch -1 $1 --stdout -- $CHECKPATCH_EXCLUDE . | _checkpatch
}

# checkpatch
if [ "$TRAVIS_PULL_REQUEST" == "false" ]; then
	checkpatch HEAD
else
	for c in $(git rev-list HEAD^1..HEAD^2); do
		checkpatch $c
	done
fi
