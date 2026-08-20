# SPDX-License-Identifier: BSD-2-Clause
#
# retrace-win-run: the Windows launcher (TODO.windows/05).
# Creates the target suspended, injects retrace.dll, resumes.
# The injection dance lives in win_common/inject.c (shared with
# the backend spawn -- one implementation).

add_executable(retrace_win_run
	retrace_win_run.c
	${CMAKE_SOURCE_DIR}/src/backends/win_common/inject.c)
set_target_properties(retrace_win_run PROPERTIES
	OUTPUT_NAME retrace-win-run
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools")

target_include_directories(retrace_win_run PRIVATE
	${CMAKE_SOURCE_DIR}/src/backends/win_common)

install(TARGETS retrace_win_run
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
