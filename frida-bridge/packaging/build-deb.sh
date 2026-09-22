#!/bin/sh
# Build the on-device .deb for retrace-frida (TODO.impl/25, #865).
#
# The payload is data-only: the bridge script installs to
# /usr/share/retrace/retrace-frida.js on the device. Works for
# any package manager that reads .deb (Cydia, Sileo, Zebra);
# TrollStore users can copy the file with any file manager
# instead (see docs/ios.md).
#
# Usage: frida-bridge/packaging/build-deb.sh [out-dir]
# Requires: dpkg-deb (or ar+tar as a fallback).

set -e
cd "$(dirname "$0")/.."
OUT="${1:-.}"
WORK=$(mktemp -d)
PKG=retrace-frida
VER=$(sed -n 's/.*retrace-frida \([0-9][0-9.]*\).*/\1/p' README.md | head -1)
[ -n "$VER" ] || VER=1.0

mkdir -p "$WORK/$PKG/DEBIAN" "$WORK/$PKG/usr/share/retrace"
cp retrace-frida.js "$WORK/$PKG/usr/share/retrace/"

cat > "$WORK/$PKG/DEBIAN/control" <<CTRL
Package: $PKG
Name: retrace-frida
Version: $VER
Architecture: iphoneos-arm
Description: retrace Frida bridge: capture libc calls as retrace JSON
 The bridge hooks libc via Frida and emits retrace-compatible
 traces. Use with: frida -l /usr/share/retrace/retrace-frida.js -n App
Maintainer: Ribose <open.source@ribose.com>
Section: Development
Depends: firmware (>= 12.0)
CTRL

mkdir -p "$OUT"
if command -v dpkg-deb >/dev/null 2>&1; then
	dpkg-deb --root-owner-group -b "$WORK/$PKG" \
		"$OUT/${PKG}_${VER}_iphoneos-arm.deb"
else
	# ar + tar fallback (macOS hosts without dpkg)
	CTRL_TAR="$WORK/control.tar.gz"
	DATA_TAR="$WORK/data.tar.gz"
	DEB="$OUT/${PKG}_${VER}_iphoneos-arm.deb"
	(cd "$WORK/$PKG/DEBIAN" && tar -czf "$CTRL_TAR" ./control)
	(cd "$WORK/$PKG" && tar -czf "$DATA_TAR" --exclude=./DEBIAN .)
	ar rcs "$DEB" "$CTRL_TAR" "$DATA_TAR" 2>/dev/null \
		|| ar -rc "$DEB" "$CTRL_TAR" "$DATA_TAR"
	echo "built (ar fallback): $DEB"
fi
rm -rf "$WORK"
echo "payload: /usr/share/retrace/retrace-frida.js"
