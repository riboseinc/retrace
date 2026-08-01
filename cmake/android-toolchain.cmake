# CMake toolchain for cross-compiling retrace for Android.
#
# Usage:
#   cmake -B build-android -G Ninja \
#     -DCMAKE_TOOLCHAIN_FILE=<this-file> \
#     -DANDROID_NDK=/path/to/ndk \
#     -DANDROID_ABI=arm64-v8a \
#     -DANDROID_PLATFORM=android-29
#
# Then build:
#   cmake --build build-android
#
# The resulting libretrace.so can be pushed to an Android device and
# loaded via LD_PRELOAD in a debug build (wrap.sh) or via Magisk on
# rooted devices.
#
# Android uses Bionic libc (not glibc/musl). Bionic supports
# LD_PRELOAD and exports standard symbol names. retrace's preload_elf
# backend should work, but some symbols may differ (e.g., Bionic
# doesn't export __isoc99_scanf variants).

set(CMAKE_SYSTEM_NAME Android)
set(CMAKE_SYSTEM_VERSION 1)

if(NOT DEFINED ANDROID_NDK)
    message(FATAL_ERROR "ANDROID_NDK must point to the Android NDK root (e.g., $ANDROID_NDK_HOME)")
endif()

if(NOT DEFINED ANDROID_ABI)
    set(ANDROID_ABI arm64-v8a)
endif()

if(NOT DEFINED ANDROID_PLATFORM)
    set(ANDROID_PLATFORM android-29)
endif()

set(ANDROID_TOOLCHAIN_ROOT "${ANDROID_NDK}/toolchains/llvm/prebuilt/linux-x86_64")

if(ANDROID_ABI STREQUAL "arm64-v8a")
    set(CMAKE_SYSTEM_PROCESSOR aarch64)
    set(ANDROID_TARGET aarch64-linux-android)
elseif(ANDROID_ABI STREQUAL "x86_64")
    set(CMAKE_SYSTEM_PROCESSOR x86_64)
    set(ANDROID_TARGET x86_64-linux-android)
elseif(ANDROID_ABI STREQUAL "armeabi-v7a")
    message(FATAL_ERROR "retrace requires arm64 or x86_64; armv7 not supported (no trampoline)")
else()
    message(FATAL_ERROR "Unsupported ANDROID_ABI: ${ANDROID_ABI}")
endif()

set(CMAKE_C_COMPILER "${ANDROID_TOOLCHAIN_ROOT}/bin/${ANDROID_TARGET}${ANDROID_PLATFORM_MAJOR}-clang")
set(CMAKE_CXX_COMPILER "${ANDROID_TOOLCHAIN_ROOT}/bin/${ANDROID_TARGET}${ANDROID_PLATFORM_MAJOR}-clang++")

set(CMAKE_FIND_ROOT_PATH "${ANDROID_TOOLCHAIN_ROOT}/sysroot")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
