# SPDX-License-Identifier: BSD-2-Clause
#
# retrace JSON config source. Vendored parson parser + conf.c that
# builds a retrace_script from JSON. See TODO.v2/04-engine-refactor.md
# and TODO.roadmap/04-config-sources.md.

add_library(retrace_config_json OBJECT
	json/parson.c
	json/conf.c)

target_include_directories(retrace_config_json
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/json>
		$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/core>)  # logger.h, real_impls.h

if(RETRACE_PLATFORM_LINUX OR RETRACE_PLATFORM_FREEBSD)
	target_compile_definitions(retrace_config_json PRIVATE _GNU_SOURCE _DEFAULT_SOURCE)
elseif(RETRACE_PLATFORM_DARWIN)
	target_compile_definitions(retrace_config_json PRIVATE _DARWIN_C_SOURCE)
endif()
