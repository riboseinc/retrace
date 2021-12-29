#!/bin/bash
set -eu

CHECKPATCH=$CHECKPATCH_INSTALL/checkpatch.pl
CHECKPATCH_TYPEDEFS=$CHECKPATCH_INSTALL/typedefs.checkpatch

CHECKPATCH_FLAGS=(
	--ignore ARRAY_SIZE
	--ignore BRACES
	--ignore COMMIT_LOG_LONG_LINE
	--ignore COMPLEX_MACRO
	--ignore EMAIL_SUBJECT
	--ignore EMBEDDED_FILENAME
	--ignore EXECUTE_PERMISSIONS
	--ignore FILE_PATH_CHANGES
	--ignore LONG_LINE
	--ignore LONG_LINE_COMMENT
	--ignore LONG_LINE_STRING
	--ignore MISSING_SIGN_OFF
	--ignore MULTISTATEMENT_MACRO_USE_DO_WHILE
	--ignore NAKED_SSCANF
	--ignore NEW_TYPEDEFS
	--ignore PREFER_ALIGNED
	--ignore PREFER_FALLTHROUGH
	--ignore RETURN_PARENTHESES
	--ignore SPDX_LICENSE_TAG
	--ignore SPLIT_STRING
	--ignore SSCANF_TO_KSTRTO
	--ignore STATIC_CONST_CHAR_ARRAY
	--ignore STORAGE_CLASS
	--ignore SYMBOLIC_PERMS
	--ignore TRAILING_SEMICOLON
	--ignore USE_FUNC
	--no-tree
)

# checkpatch.pl will ignore the following paths
CHECKPATCH_IGNORE=(
	'*.json'
	'*.nix'
	'.gitattributes'
	'.github/*'
	'.gitignore'
	'ci/*'
	Makefile
	checkpatch.pl.patch
	src/v2/parson.c
	src/v2/parson.h
	test/Makefile
	test/http.redirect/hello.txt
)

CHECKPATCH_EXCLUDE=("${CHECKPATCH_IGNORE[@]/#/":(exclude)"}")

_checkpatch() {
	"$CHECKPATCH" "${CHECKPATCH_FLAGS[@]}" "--typedefsfile=$CHECKPATCH_TYPEDEFS" --no-tree -
}

checkpatch() {
	local commit="${1:?Missing commit}"
	git show --oneline --no-patch "${commit}"
	git format-patch -1 "${commit}" --stdout -- "${CHECKPATCH_EXCLUDE[@]}" . | _checkpatch
}

# checkpatch
#
# Check HEAD for pull requests.
#
# NOTE: $GITHUB_HEAD_REF is only set for pull requests.
# See: https://docs.github.com/en/actions/learn-github-actions/environment-variables
if [[ -z "${GITHUB_HEAD_REF:-}" ]]; then
	checkpatch HEAD

# XXX: HEAD^2 seems to only makes sense for merge commits
elif git show --format=short | head -n2 | grep -q ^Merge:
then
	while read -r c; do
		checkpatch "$c"
	done < <(git rev-list HEAD^1..HEAD^2)
else
	>&2 echo "Warning: this should not be run."
	exit 1
fi
