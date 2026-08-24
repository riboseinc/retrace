/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Symbol visibility macros. Public symbols are tagged OTLP_C_EXPORT;
 * internal symbols have no annotation and default to hidden (set via
 * CMAKE_C_VISIBILITY_PRESET in CMakeLists.txt).
 *
 * On Windows, OTLP_C_BUILDING is defined while compiling the library
 * itself; consumers of the (shared) library define nothing and get
 * the import macro. Static library users see no annotation.
 */
#ifndef OTLP_C_VISIBILITY_H
#define OTLP_C_VISIBILITY_H

/* When building a shared library on Windows, we need dllexport on
 * the library side and dllimport on the consumer side. The CMake
 * target_compile_definitions sets OTLP_C_EXPORT to the appropriate
 * macro for the platform; we just re-export it here. */
#ifndef OTLP_C_EXPORT
#  if defined(_WIN32) && defined(OTLP_C_SHARED_LIB)
#    ifdef OTLP_C_BUILDING
#      define OTLP_C_EXPORT __declspec(dllexport)
#    else
#      define OTLP_C_EXPORT __declspec(dllimport)
#    endif
#  elif defined(__GNUC__) || defined(__clang__)
#    define OTLP_C_EXPORT __attribute__((visibility("default")))
#  else
#    define OTLP_C_EXPORT
#  endif
#endif

#endif
