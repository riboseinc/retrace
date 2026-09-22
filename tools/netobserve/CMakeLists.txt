# SPDX-License-Identifier: BSD-2-Clause
#
# retrace-netobserve: the stock-device network observer
# (TODO.impl/25, GH #865). POSIX sockets only -- the process-
# driving tools live on every platform; this one is network
# truth, so it builds wherever a TCP stack exists.

add_executable(retrace_netobserve retrace-netobserve.c)

set_target_properties(retrace_netobserve PROPERTIES
	OUTPUT_NAME retrace-netobserve)

install(TARGETS retrace_netobserve
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# the netobserve smoke test: proxy + curl through it + the
# JSON record asserted (Linux CI; self-skipping without curl)
if(RETRACE_BUILD_TESTS AND UNIX AND NOT APPLE)
	find_program(CURL curl)
	if(CURL)
		add_test(NAME integration-netobserve
			COMMAND ${CMAKE_COMMAND}
				-DNETOBSERVE=$<TARGET_FILE:retrace_netobserve>
				-DCURL=${CURL}
				-P ${CMAKE_CURRENT_SOURCE_DIR}/netobserve_smoke.cmake)
		set_tests_properties(integration-netobserve
			PROPERTIES LABELS "integration;v2")
	endif()
endif()
