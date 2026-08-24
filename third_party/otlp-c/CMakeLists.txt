cmake_minimum_required(VERSION 3.20)

project(otlp-c
    VERSION 0.6.15
    DESCRIPTION "Pure-C OTLP/HTTP client for OpenTelemetry"
    HOMEPAGE_URL "https://github.com/riboseinc/otlp-c"
    LANGUAGES C
)

set(OTLP_C_VERSION ${PROJECT_VERSION})
set(OTLP_C_VERSION_STRING "${PROJECT_VERSION}")

# otlp-c must be side-effect-free when embedded (FetchContent /
# add_subdirectory): install-layout pinning and CPack below only
# apply when otlp-c IS the project being configured. CMake 3.20
# compatibility — PROJECT_IS_TOP_LEVEL needs 3.21.
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(OTLP_C_IS_TOP_LEVEL ON)
endif()

# Standards. C11 (not C99) because we use <stdatomic.h> for
# lock-free MPSC queues and PRNG state. CLAUDE.md allows C11 for
# atomics; we write C99-style code everywhere else.
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
# MSVC's <stdatomic.h> requires C11 with extensions enabled (/std:c11
# without /Za). GCC/Clang are fine with extensions off.
if(MSVC)
    set(CMAKE_C_EXTENSIONS ON)
else()
    set(CMAKE_C_EXTENSIONS OFF)
endif()

# Position-independent code by default (so the static lib can be
# linked into a shared lib).
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Hidden visibility by default; exported symbols opt in.
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

# Options
option(OTLP_C_BUILD_TESTS "Build unit + property + integration tests" OFF)
option(OTLP_C_BUILD_EXAMPLES "Build example programs" OFF)
option(OTLP_C_BUILD_BENCH "Build microbenchmarks" OFF)
option(OTLP_C_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(OTLP_C_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(OTLP_C_ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(BUILD_SHARED_LIBS "Build as shared library instead of static" OFF)

# Sanitizers
if(OTLP_C_ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()
if(OTLP_C_ENABLE_UBSAN)
    add_compile_options(-fsanitize=undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=undefined)
endif()
if(OTLP_C_ENABLE_TSAN)
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
    add_link_options(-fsanitize=thread)
endif()

# Warnings (gcc + clang)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    add_compile_options(
        -Wall -Wextra -Wpedantic
        -Wconversion -Wsign-conversion
        -Wundef -Wshadow -Wpointer-arith
        -Wformat=2 -Wno-format-nonliteral
        -Wwrite-strings -Wold-style-definition
        -Wmissing-prototypes
    )
endif()

# POSIX feature macros for clock_gettime, nanosleep, etc. on glibc/musl
# with -std=c11 (which disables the default _GNU_SOURCE). macOS
# declares these by default.
if(UNIX AND NOT APPLE)
    add_compile_definitions(_POSIX_C_SOURCE=200809L _DEFAULT_SOURCE)
endif()

# MSVC needs explicit /std:c11 for <stdatomic.h>
if(MSVC)
    add_compile_options(/std:c11)
    # Force C11 atomics support macro — the vcruntime header checks
    # _HAS_C11_ATOMICS which depends on the VS installation having
    # the "C11 atomics" component. On GitHub CI runners this isn't
    # always present. The compiler itself supports the operations.
    add_compile_definitions(_HAS_C11_ATOMICS=1)
endif()

# Threads
# Tests link Threads::Threads directly for pthread (test_helper_echo.c,
# test_property_mpsc.c). The library itself uses pthread_self() for
# PRNG seed entropy on POSIX only — that's in libc on modern systems,
# no -lpthread link needed for the library itself.
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Library target
add_library(otlp_c
    src/common.c
    src/internal_util.c
    src/slab.c
    src/context.c
    src/sampler.c
    src/span.c
    src/tracer.c
    src/w3c.c
    src/exporter.c
    src/exporter_otel.c
    src/http_client.c
    src/http_response_parser.c
    src/retry_policy.c
    src/otlp_messages.c
    src/otlp_metrics_encoder.c
    src/otlp_logs_encoder.c
    src/metric.c
    src/log.c
    src/protobuf_decode.c
    src/protobuf_encode.c
    src/mpsc_queue.c
    src/platform.c
)

# Platform-specific socket layer. POSIX vs Win32.
if(WIN32)
    target_sources(otlp_c PRIVATE src/platform_win.c)
    target_link_libraries(otlp_c PRIVATE Ws2_32)
else()
    target_sources(otlp_c PRIVATE src/platform_unix.c)
endif()

add_library(otlp-c::otlp_c ALIAS otlp_c)

target_include_directories(otlp_c
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(otlp_c PRIVATE OTLP_C_BUILDING=1)

# For shared library consumers on Windows, define OTLP_C_SHARED_LIB
# so the import path is taken in visibility.h.
if(BUILD_SHARED_LIBS)
    target_compile_definitions(otlp_c PUBLIC OTLP_C_SHARED_LIB=1)
endif()

# Tests
if(OTLP_C_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Examples
if(OTLP_C_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Benchmarks (opt-in)
if(OTLP_C_BUILD_BENCH)
    add_subdirectory(bench)
endif()

# Install
# Pin LIBDIR to "lib" before GNUInstallDirs so the cmake config files
# land at a single deterministic path on every platform
# (<prefix>/lib/cmake/otlp-c/). Without this, GNUInstallDirs on
# Windows MSVC chooses "lib/x64" or similar arch-suffixed paths,
# and find_package(otlp-c CONFIG) from a consumer CMakeLists can't
# locate the config without extra hints.
#
# Top-level only: an embedded otlp-c (FetchContent/add_subdirectory)
# must respect the parent project's install layout — the FORCE here
# used to clobber the parent's cache entry (verified: a consumer's
# lib/x86_64-linux-gnu silently became "lib").
if(OTLP_C_IS_TOP_LEVEL)
    set(CMAKE_INSTALL_LIBDIR "lib" CACHE STRING "" FORCE)
endif()
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS otlp_c
    EXPORT otlp-c-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/otlp-c
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

install(EXPORT otlp-c-targets
    FILE otlp-c-targets.cmake
    NAMESPACE otlp-c::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/otlp-c
)

configure_package_config_file(
    cmake/otlp-c-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/otlp-c-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/otlp-c
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/otlp-c-config-version.cmake
    VERSION ${OTLP_C_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/otlp-c-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/otlp-c-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/otlp-c
)

# ── Doxygen API reference ───────────────────────────────────────
# `cmake --build build --target docs` produces HTML in build/docs/html/.
find_package(Doxygen)
if(DOXYGEN_FOUND)
    # Version comes from PROJECT_VERSION — the Doxyfile is a
    # configured template so PROJECT_NUMBER can never go stale
    # (it said 0.2.0 for a hundred releases).
    configure_file(Doxyfile.in Doxyfile @ONLY)
    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating Doxygen API reference"
        VERBATIM)
endif()

# ── CPack (binary distribution) ─────────────────────────────────
# Top-level only: include(CPack) writes CPackConfig.cmake into the
# TOP-LEVEL build dir — when embedded, that's the consumer's build
# tree, not ours.
if(OTLP_C_IS_TOP_LEVEL)
set(CPACK_PACKAGE_NAME              "otlp-c")
set(CPACK_PACKAGE_VERSION           "${OTLP_C_VERSION_STRING}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Pure-C99 OTLP/HTTP client with zero non-libc dependencies")
set(CPACK_PACKAGE_VENDOR            "Ribose, Inc.")
set(CPACK_PACKAGE_CONTACT           "open.source@ribose.com")
set(CPACK_RESOURCE_FILE_LICENSE     "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README      "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_PACKAGE_HOMEPAGE_URL      "https://github.com/riboseinc/otlp-c")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "otlp-c")

# Per-platform archive formats.
if(WIN32)
    set(CPACK_GENERATOR "ZIP")
elseif(APPLE)
    set(CPACK_GENERATOR "TGZ;ZIP")
else()
    set(CPACK_GENERATOR "TGZ;TXZ")
endif()

# Source tarball (always .tar.gz for portability).
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_SOURCE_IGNORE_FILES
    "/\\\\.git/"
    "/build/"
    "/build-.*"
    "\\\\.swp$"
    "~$")

include(CPack)
endif()

# ── Code coverage instrumentation ───────────────────────────────
# `cmake -DOTLP_C_ENABLE_COVERAGE=ON` adds gcov / llvm-cov flags.
# Use with `LLVM_PROFILE_FILE=... ctest --test-dir build` then
# `llvm-cov report`. NOTE: the flags are target-scoped (PUBLIC) —
# the old directory-scope add_compile_options sat AFTER the
# otlp_c target definition, so nothing was ever instrumented and
# the option produced no data (found by measurement, v0.5.90).
option(OTLP_C_ENABLE_COVERAGE "Instrument for code coverage (gcov/llvm-cov)" OFF)
if(OTLP_C_ENABLE_COVERAGE)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(otlp_c PUBLIC -fprofile-instr-generate -fcoverage-mapping -O0 -g)
        target_link_options(otlp_c PUBLIC -fprofile-instr-generate)
        message(STATUS "Coverage instrumentation enabled (clang Profile)")
    else()
        message(WARNING "OTLP_C_ENABLE_COVERAGE: unsupported compiler; ignoring")
    endif()
endif()

# Status
message(STATUS "otlp-c ${OTLP_C_VERSION_STRING}")
message(STATUS "  Build tests:    ${OTLP_C_BUILD_TESTS}")
message(STATUS "  Build examples: ${OTLP_C_BUILD_EXAMPLES}")
message(STATUS "  Shared library: ${BUILD_SHARED_LIBS}")
message(STATUS "  ASan:           ${OTLP_C_ENABLE_ASAN}")
message(STATUS "  UBSan:          ${OTLP_C_ENABLE_UBSAN}")
