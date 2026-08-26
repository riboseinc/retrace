# SPDX-License-Identifier: BSD-2-Clause
#
# Runs examples/supervisor-quickstart's platform runner with the
# build tree (integration-supervisor-quickstart).

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	set(RUNNER ${DEMO_DIR}/run-macos.sh)
else()
	set(RUNNER ${DEMO_DIR}/run-linux.sh)
endif()

execute_process(
	COMMAND /bin/sh ${RUNNER} ${BUILD_DIR}
	RESULT_VARIABLE rc
	OUTPUT_VARIABLE out
	ERROR_VARIABLE out
	TIMEOUT 100)
if(NOT rc EQUAL 0)
	message(FATAL_ERROR
		"supervisor-quickstart failed (${rc}):\n${out}")
endif()
