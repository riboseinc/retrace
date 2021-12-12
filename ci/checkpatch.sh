#!/bin/bash
set -eu

CHECKPATCH=$CHECKPATCH_INSTALL/checkpatch.pl
CHECKPATCH_TYPEDEFS=$CHECKPATCH_INSTALL/typedefs.checkpatch

CHECKPATCH_FLAGS=(
	--no-tree
	--ignore COMPLEX_MACRO
	--ignore TRAILING_SEMICOLON
	--ignore LONG_LINE
	--ignore LONG_LINE_STRING
	--ignore LONG_LINE_COMMENT
	--ignore SYMBOLIC_PERMS
	--ignore NEW_TYPEDEFS
	--ignore SPLIT_STRING
	--ignore USE_FUNC
	--ignore COMMIT_LOG_LONG_LINE
	--ignore FILE_PATH_CHANGES
	--ignore MISSING_SIGN_OFF
	--ignore RETURN_PARENTHESES
	--ignore STATIC_CONST_CHAR_ARRAY
	--ignore ARRAY_SIZE
	--ignore NAKED_SSCANF
	--ignore SSCANF_TO_KSTRTO
	--ignore EXECUTE_PERMISSIONS
	--ignore MULTISTATEMENT_MACRO_USE_DO_WHILE
	--ignore STORAGE_CLASS
	--ignore SPDX_LICENSE_TAG
	--ignore PREFER_ALIGNED
	--ignore EMAIL_SUBJECT
)

# checkpatch.pl will ignore the following paths
CHECKPATCH_IGNORE=(checkpatch.pl.patch Makefile test/Makefile test/http.redirect/hello.txt src/v2/parson.c src/v2/parson.h "*.json")
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
