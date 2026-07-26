# SPDX-License-Identifier: BSD-2-Clause
#
# retrace v2 shared library. After the v2 plan/04 (engine refactor) and
# the v2 plan/05 (backend abstraction), this is a thin shim:
#   - retrace_core             (object library): engine + registries
#   - retrace_config_json      (object library): JSON config source
#   - retrace_backends         (object library): backend registry
#   - retrace_backend_preload_* (object lib per backend, per arch):
#                                trampoline + spawn
#   - funcs_symbols.S          per-arch function table
#
# The v2 shared library is what consumers LD_PRELOAD/DYLD_INSERT.

# Per-arch function table. Phase 2 of the v2 plan splits this per arch
# (x86_64 vs aarch64) so each arch builds its own set.
set(_v2_trampoline_sources funcs_symbols.S)

add_library(retrace_v2 SHARED ${_v2_trampoline_sources})
add_library(retrace::v2 ALIAS retrace_v2)

# Pull in the modules.
target_link_libraries(retrace_v2 PRIVATE
	retrace_core
	retrace_config_json
	retrace_backends
	retrace_preload_common)

# Pull in the per-platform backend(s). On Linux this is preload_elf; on
# macOS preload_macho; on BSDs preload_bsd. Future Windows: preload_msvc
# + preload_mingw (the v2 plan/06).
if(RETRACE_PLATFORM_LINUX)
	target_link_libraries(retrace_v2 PRIVATE retrace_backend_preload_elf)
elseif(RETRACE_PLATFORM_DARWIN)
	target_link_libraries(retrace_v2 PRIVATE retrace_backend_preload_macho)
elseif(RETRACE_PLATFORM_FREEBSD OR RETRACE_PLATFORM_OPENBSD OR RETRACE_PLATFORM_NETBSD)
	target_link_libraries(retrace_v2 PRIVATE retrace_backend_preload_bsd)
endif()

# Find the per-backend arch_spec_top.S that funcs_symbols.S includes.
# Phase 2 of the v2 plan moves funcs_symbols.S into each backend's directory.
if(RETRACE_PLATFORM_LINUX)
	set(_v2_arch_dir ${CMAKE_SOURCE_DIR}/src/backends/preload_elf/x86_64)
elseif(RETRACE_PLATFORM_DARWIN)
	set(_v2_arch_dir ${CMAKE_SOURCE_DIR}/src/backends/preload_macho/x86_64)
elseif(RETRACE_PLATFORM_FREEBSD OR RETRACE_PLATFORM_OPENBSD OR RETRACE_PLATFORM_NETBSD)
	set(_v2_arch_dir ${CMAKE_SOURCE_DIR}/src/backends/preload_bsd/x86_64)
endif()

target_include_directories(retrace_v2
	PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
	PRIVATE ${CMAKE_SOURCE_DIR}/src/backends ${_v2_arch_dir})

set_target_properties(retrace_v2 PROPERTIES
	OUTPUT_NAME retrace_v2
	VERSION ${PROJECT_VERSION}
	SOVERSION ${PROJECT_VERSION_MAJOR}
	C_VISIBILITY_PRESET hidden
	VISIBILITY_INLINES_HIDDEN ON)

if(RETRACE_PLATFORM_LINUX OR RETRACE_PLATFORM_FREEBSD)
	target_compile_definitions(retrace_v2 PRIVATE _GNU_SOURCE _DEFAULT_SOURCE)
elseif(RETRACE_PLATFORM_DARWIN)
	target_compile_definitions(retrace_v2 PRIVATE _DARWIN_C_SOURCE)
endif()

target_link_libraries(retrace_v2
	PUBLIC Threads::Threads OpenSSL::SSL ${CMAKE_DL_LIBS})

install(TARGETS retrace_v2
	EXPORT retraceTargets
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
