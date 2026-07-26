#!/bin/bash
# Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
#
# Builds a source-only retrace-<version>.tar.gz.
#
# Excludes generated Autotools artifacts and vcpkg buildtrees so consumers
# get a clean source tree that re-runs CMake (or Autotools via autogen.sh).
#
# Usage: scripts/build-release-tarball.sh [OUTPUT_DIR]
# Output: $OUTPUT_DIR/retrace-<version>.tar.gz + .sha256

set -euo pipefail

OUTPUT_DIR="${1:-${RUNNER_TEMP:-/tmp}}"
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
REPO_ROOT="$( cd "${SCRIPT_DIR}/.." && pwd )"

cd "${REPO_ROOT}"

VERSION="$(awk -F\" '/^#define RETRACE_VERSION_STRING/ {print $2}' \
	"${REPO_ROOT}/include/retrace/version.h")"
TARBALL_NAME="retrace-${VERSION}"
STAGE_DIR="$(mktemp -d)"
STAGE_ROOT="${STAGE_DIR}/${TARBALL_NAME}"
mkdir -p "${STAGE_ROOT}"

# Files included in the tarball.
git archive --format=tar HEAD | tar -x -C "${STAGE_ROOT}"

# Strip out things we don't want in the release tarball.
# Per the global rule, never delete source files. This pruning is local to
# the staging dir, not the working tree.
rm -rf "${STAGE_ROOT}/.git"
rm -rf "${STAGE_ROOT}/autom4te.cache"
rm -rf "${STAGE_ROOT}/.libs"
rm -rf "${STAGE_ROOT}/build"

# Build the tarball.
OUT_TARBALL="${OUTPUT_DIR}/${TARBALL_NAME}.tar.gz"
mkdir -p "${OUTPUT_DIR}"
tar -czf "${OUT_TARBALL}" -C "${STAGE_DIR}" "${TARBALL_NAME}"

# SHA256 sidecar.
sha256sum "${OUT_TARBALL}" > "${OUT_TARBALL}.sha256"

# Cleanup.
rm -rf "${STAGE_DIR}"

echo "Created: ${OUT_TARBALL}"
echo "SHA256:  ${OUT_TARBALL}.sha256"
