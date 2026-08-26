# SPDX-License-Identifier: BSD-2-Clause
#
# retrace-ctl -- the fleet CLI (TODO.supervisor/07). POSIX-only
# like retraced until plan 12 (named pipes).

add_executable(retrace_ctl main.c)
set_target_properties(retrace_ctl PROPERTIES
	OUTPUT_NAME retrace-ctl)

install(TARGETS retrace_ctl
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
