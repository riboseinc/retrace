# SPDX-License-Identifier: BSD-2-Clause
#
# retrace tests.
#
# Currently mirrors the Autotools test/Makefile.am. the modernization plan/08 restructures
# this into unit/integration/property/golden/smoke directories.

set(_test_programs
	id env time fork exit malloc str pipe sock sock_srv file trace
	pledge scanf dlopen dir exec printf popen char mmap sbrk
	getaddrinfo log memcpy temp config perror setlocale strinject
	recvmsg writev readv)

foreach(_prog ${_test_programs})
	if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_prog}.c")
		add_executable(retrace_test_${_prog} ${_prog}.c)
		target_link_libraries(retrace_test_${_prog} PRIVATE retrace::v1)
		set_target_properties(retrace_test_${_prog} PROPERTIES
			OUTPUT_NAME ${_prog})
	endif()
endforeach()

# cmockatest is Linux-only (was: if CMOCKA && LINUX in test/Makefile.am).
if(Cmocka_FOUND AND RETRACE_PLATFORM_LINUX)
	add_executable(retrace_test_cmockatest cmockatest.c)
	target_compile_definitions(retrace_test_cmockatest PRIVATE USE_CMOCKA=1)
	target_link_libraries(retrace_test_cmockatest PRIVATE retrace::v1 Cmocka::Cmocka)
	set_target_properties(retrace_test_cmockatest PROPERTIES OUTPUT_NAME cmockatest)
	add_test(NAME cmockatest COMMAND retrace_test_cmockatest)
endif()

# Integration tests via runtests.sh (was: TESTS += $(dist_check_SCRIPTS)).
find_package(Perl)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/runtests.sh")
	add_test(NAME runtests
		COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/runtests.sh"
		WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
endif()

# the modernization plan/08: replace the above with structured CTest labels:
#   add_test(NAME integration-id   COMMAND ... LABELS "integration")
#   add_test(NAME unit-engine      COMMAND ... LABELS "unit")
#   add_test(NAME smoke-run        COMMAND ... LABELS "smoke")
