#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
#
# retrace install script — detects OS + arch, downloads the right
# pre-built binary from GitHub releases, installs it.
#
# Usage:
#   curl -sSL https://raw.githubusercontent.com/riboseinc/retrace/main/scripts/install.sh | sh
#
# Or with specific version:
#   sh scripts/install.sh v2.1.0
#
# Installs to /usr/local/lib/libretrace.{so,dylib} by default.
# Override with DESTDIR= to install elsewhere.

set -eu

VERSION="${1:-latest}"
PREFIX="${DESTDIR:-/usr/local}"

OS=$(uname -s)
ARCH=$(uname -m)

# Map to release asset name
case "$OS" in
    Linux)
        case "$ARCH" in
            x86_64|amd64) PLATFORM="linux-x86_64"; EXT="so"; ENV_VAR="LD_PRELOAD" ;;
            aarch64|arm64) PLATFORM="linux-aarch64"; EXT="so"; ENV_VAR="LD_PRELOAD" ;;
            *) echo "retrace: unsupported Linux arch '$ARCH'" >&2; exit 1 ;;
        esac
        ;;
    Darwin)
        case "$ARCH" in
            arm64|aarch64) PLATFORM="macos-arm64"; EXT="dylib"; ENV_VAR="DYLD_INSERT_LIBRARIES" ;;
            x86_64) PLATFORM="macos-x86_64"; EXT="dylib"; ENV_VAR="DYLD_INSERT_LIBRARIES" ;;
            *) echo "retrace: unsupported macOS arch '$ARCH'" >&2; exit 1 ;;
        esac
        ;;
    *) echo "retrace: unsupported OS '$OS'" >&2; exit 1 ;;
esac

ASSET="libretrace-${PLATFORM}.${EXT}"
LIBNAME="libretrace.${EXT}"

if [ "$VERSION" = "latest" ]; then
    URL="https://github.com/riboseinc/retrace/releases/latest/download/${ASSET}"
else
    URL="https://github.com/riboseinc/retrace/releases/download/${VERSION}/${ASSET}"
fi

DEST="${PREFIX}/lib/${LIBNAME}"

echo "retrace: downloading $ASSET ($VERSION)..."
mkdir -p "${PREFIX}/lib"

if command -v curl >/dev/null 2>&1; then
    curl -fsSL "$URL" -o "$DEST" || {
        echo "retrace: download failed. Check the version or your network." >&2
        exit 1
    }
elif command -v wget >/dev/null 2>&1; then
    wget -qO "$DEST" "$URL" || {
        echo "retrace: download failed. Check the version or your network." >&2
        exit 1
    }
else
    echo "retrace: curl or wget required." >&2
    exit 1
fi

chmod 755 "$DEST"

if [ "$OS" = "Linux" ]; then
    ldconfig 2>/dev/null || true
fi

echo ""
echo "retrace installed to $DEST"
echo ""
echo "Quick start:"
echo "  $ENV_VAR=$DEST /bin/ls        # trace all libc calls"
echo ""
echo "  # Or install the CLI too:"
echo "  git clone https://github.com/riboseinc/retrace.git"
echo "  cd retrace && cmake -B build -G Ninja && cmake --build build"
echo "  RETRACE_LIB=$DEST ./build/src/cli/retrace trace malloc -- /bin/ls"
