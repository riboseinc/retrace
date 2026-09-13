# SPDX-License-Identifier: BSD-2-Clause
#
# retrace-campaign (TODO.impl/09): the farm arm -- a manifest
# to a matrix of runs, driven through the daemon's public
# surface (retrace-ctl as a subprocess). POSIX-only v1
# (popen + pthreads).

if(RETRACE_PLATFORM_WINDOWS)
	return()
endif()

add_executable(retrace_campaign
	campaign.c
	model.c
	${CMAKE_SOURCE_DIR}/src/config/json/parson.c)

set_target_properties(retrace_campaign PROPERTIES
	OUTPUT_NAME retrace-campaign)

target_include_directories(retrace_campaign PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_SOURCE_DIR}/src/config/json
	${CMAKE_SOURCE_DIR}/test/property/parson_stub)

find_package(Threads REQUIRED)
target_link_libraries(retrace_campaign PRIVATE Threads::Threads)

install(TARGETS retrace_campaign
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
