# SPDX-License-Identifier: BSD-2-Clause
#
# retrace CLI launcher. Issue #485 / #358.
# Replaces the v1 shell-script launcher (ADR-0011).

add_executable(retrace_cli retrace_cli.c)
set_target_properties(retrace_cli PROPERTIES
	OUTPUT_NAME retrace)

install(TARGETS retrace_cli
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
