# SPDX-License-Identifier: BSD-2-Clause
#
# retrace CLI launcher. Issue #485 / #358.
# Replaces the v1 shell-script launcher (ADR-0011).

add_executable(retrace_cli
	retrace_cli.c
	${CMAKE_SOURCE_DIR}/src/config/json/parson.c)
set_target_properties(retrace_cli PROPERTIES
	OUTPUT_NAME retrace)

# CLI's own dir is first on the path so parson.c finds our stub
# real_impls.h (maps to standard libc) instead of the full
# reentrancy-safe version from src/core/.
target_include_directories(retrace_cli PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_SOURCE_DIR}/src/config/json)

install(TARGETS retrace_cli
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
