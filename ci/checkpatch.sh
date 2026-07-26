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
	--ignore PREFER_DEFINED_ATTRIBUTE_MACRO
	--ignore PREFER_FALLTHROUGH
	--ignore RETURN_PARENTHESES
	--ignore SPDX_LICENSE_TAG
	--ignore SPLIT_STRING
	--ignore SSCANF_TO_KSTRTO
	--ignore STATIC_CONST_CHAR_ARRAY
	--ignore STORAGE_CLASS
	--ignore STRCPY
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
# On push: HEAD is the commit to check.
# On pull_request: actions/checkout@v4 checks out refs/pull/<N>/merge,
#   which is an auto-generated merge commit with a generic message that
#   fails checkpatch's commit-message checks. Check each commit in the
#   PR branch range instead. Base SHA is injected via env (see
#   .github/workflows/checkpatch.yml) since the runner's default fetch
#   doesn't include origin/<base-ref>.
if [[ -n "${GITHUB_BASE_REF:-}" && -n "${RETRACE_BASE_SHA:-}" ]]; then
	while read -r c; do
		checkpatch "$c"
	done < <(git rev-list "${RETRACE_BASE_SHA}..HEAD" -- "${CHECKPATCH_EXCLUDE[@]}" .)
else
	checkpatch HEAD
fi
