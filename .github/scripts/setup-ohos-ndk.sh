#!/usr/bin/env bash
# Setup OHOS NDK for cross-compiling to aarch64-linux-ohos.
#
# Downloads OHOS SDK (~3.2 GB) and LLVM-19 (~670 MB) from the public
# dcp.openharmony.cn API (no Huawei portal login required), extracts all
# nested archives, and fixes the two-sysroots problem with a relative
# symlink so the official ohos.toolchain.cmake resolves headers correctly.
#
# Idempotent: skips download if toolchain + sysroot + clang already present.
# Safe to re-run, safe to cache (~4 GB total).
#
# Reference: https://gist.github.com/ronaldtse/dd9e08dd294f7f37fd40b0a9b84468aa
set -euo pipefail

PREFIX=""
while [ $# -gt 0 ]; do
    case "$1" in
        --prefix) PREFIX="$2"; shift 2 ;;
        *) echo "unknown arg: $1" >&2; exit 2 ;;
    esac
done
[ -z "$PREFIX" ] && { echo "Usage: $0 --prefix /opt/ohos" >&2; exit 2; }
mkdir -p "$PREFIX"
PREFIX="$(cd "$PREFIX" && pwd)"

query_component() {
    local payload
    payload=$(jq -nc --arg component "$1" '{
        projectName:"openharmony", branch:"master", pageNum:1, pageSize:10,
        deviceLevel:"", component:$component, type:1,
        startTime:"2025080100000000", endTime:"20990101235959",
        sortType:"", sortField:"", hardwareBoard:"",
        buildStatus:"success", buildFailReason:"", withDomain:1
    }')
    curl --retry 5 --retry-delay 5 --retry-all-errors -fsSL \
        'https://dcp.openharmony.cn/api/daily_build/build/list/component' \
        -H 'Accept: application/json, text/plain, */*' \
        -H 'Content-Type: application/json' \
        --data-raw "$payload"
}

TOOLCHAIN="$PREFIX/ohos-sdk/linux/native/build/cmake/ohos.toolchain.cmake"
if [ -f "$TOOLCHAIN" ] && [ -d "$PREFIX/llvm-19/sysroot" ] && [ -x "$PREFIX/llvm-19/llvm/bin/clang" ]; then
    echo "[setup-ohos-ndk] already populated, skipping"
    exit 0
fi

echo "[setup-ohos-ndk] querying daily_build API..."
SDK_URL=$(query_component "ohos-sdk-public" | jq -r '.data.list.dataList[0].obsPath')
LLVM_URL=$(query_component "LLVM-19"         | jq -r '.data.list.dataList[0].obsPath')

[ -n "$SDK_URL" ]  || { echo "FAIL: empty SDK URL" >&2; exit 1; }
[ -n "$LLVM_URL" ] || { echo "FAIL: empty LLVM URL" >&2; exit 1; }

mkdir -p "$PREFIX/downloads"
SDK_TARBALL="$PREFIX/downloads/ohos-sdk.tar.gz"
LLVM_TARBALL="$PREFIX/downloads/llvm-19.tar.gz"

[ -f "$SDK_TARBALL"  ] || curl --retry 5 --retry-all-errors -fL "$SDK_URL"  -o "$SDK_TARBALL"
[ -f "$LLVM_TARBALL" ] || curl --retry 5 --retry-all-errors -fL "$LLVM_URL" -o "$LLVM_TARBALL"

# ohos-sdk: outer .tar.gz -> unzip every nested .zip.
tar -xzf "$SDK_TARBALL" -C "$PREFIX"
cd "$PREFIX/ohos-sdk/linux"
for z in *.zip; do [ -e "$z" ] || continue; unzip -q -o "$z"; rm -f "$z"; done

# LLVM-19: outer .tar.gz -> recursively expand every nested .tar.gz.
mkdir -p "$PREFIX/llvm-19-extract"
tar -xzf "$LLVM_TARBALL" -C "$PREFIX/llvm-19-extract"
changed=1
while [ "$changed" = "1" ]; do
    changed=0
    while IFS= read -r -d '' tg; do
        tar -xzf "$tg" -C "$(dirname "$tg")"; rm -f "$tg"; changed=1
    done < <(find "$PREFIX/llvm-19-extract" -name '*.tar.gz' -print0)
done

# Locate clang + per-arch sysroot in the (now fully extracted) tree.
mkdir -p "$PREFIX/llvm-19/llvm" "$PREFIX/llvm-19/sysroot"
CLANG_SRC=$(find "$PREFIX/llvm-19-extract" -type f -name 'aarch64-*-ohos-clang' -path '*/bin/*' | head -1)
[ -z "$CLANG_SRC" ] && CLANG_SRC=$(find "$PREFIX/llvm-19-extract" -type f -name 'clang' -path '*/bin/*' | head -1)
SYSROOT_SRC=""
while IFS= read -r -d '' cand; do
    [ -d "$cand/usr/include/bits" ] && { SYSROOT_SRC="$cand"; break; }
done < <(find "$PREFIX/llvm-19-extract" -type d -name 'aarch64-linux-ohos' -print0)

[ -n "$CLANG_SRC" ]   || { echo "FAIL: clang not found" >&2; exit 1; }
[ -n "$SYSROOT_SRC" ] || { echo "FAIL: sysroot not found" >&2; exit 1; }

LLVM_ROOT=$(cd "$(dirname "$CLANG_SRC")/.." && pwd)
rmdir "$PREFIX/llvm-19/llvm";     mv "$LLVM_ROOT"    "$PREFIX/llvm-19/llvm"
rmdir "$PREFIX/llvm-19/sysroot";  mv "$SYSROOT_SRC"  "$PREFIX/llvm-19/sysroot"
rm -rf "$PREFIX/llvm-19-extract"

# Two-sysroots fix: replace SDK's multiarch sysroot with RELATIVE symlink.
# Must be relative (not absolute) so it resolves inside docker containers
# with different mount paths.
#
# Note: SYSROOT_SRC was the per-arch directory (e.g. .../aarch64-linux-ohos/)
# and was MOVED to llvm-19/sysroot/ -- its `usr/` is now directly under
# llvm-19/sysroot/, not under an aarch64-linux-ohos subdir.
cd "$PREFIX/ohos-sdk/linux/native"
rm -rf sysroot
ln -s "../../../llvm-19/sysroot" sysroot

echo "[setup-ohos-ndk] OK"
echo "  toolchain: $TOOLCHAIN"
echo "  sysroot:   $PREFIX/ohos-sdk/linux/native/sysroot"
echo "  clang:     $PREFIX/llvm-19/llvm/bin/clang"
echo "  signer:    $PREFIX/ohos-sdk/linux/toolchains/lib/binary-sign-tool"
