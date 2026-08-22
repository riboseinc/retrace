# SPDX-License-Identifier: BSD-2-Clause
#
# retrace-fuzz-report: the fuzzing workbench driver
# (TODO.trace-profile/20). Corpus of seeds -> classified,
# clustered failures + per-cluster reproducer configs.
# POSIX only in v1.

set(_parson_source ${CMAKE_SOURCE_DIR}/src/config/json/parson.c)

if(NOT WIN32)
	add_executable(retrace_fuzz_report fuzz-report.c cluster.c
		${CMAKE_SOURCE_DIR}/tools/profiler/aggregate.c
		${CMAKE_SOURCE_DIR}/tools/profiler/diff.c
		${CMAKE_SOURCE_DIR}/tools/common/stream.c
		${CMAKE_SOURCE_DIR}/tools/correlate/match.c
		${_parson_source})
	set_target_properties(retrace_fuzz_report PROPERTIES
		OUTPUT_NAME retrace-fuzz-report
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools")

	target_include_directories(retrace_fuzz_report PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}
		${CMAKE_SOURCE_DIR}/tools/profiler
		${CMAKE_SOURCE_DIR}/tools/common
		${CMAKE_SOURCE_DIR}/tools/correlate
		${CMAKE_SOURCE_DIR}/src/config/json
		${CMAKE_SOURCE_DIR}/test/property/parson_stub)

	install(TARGETS retrace_fuzz_report
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
