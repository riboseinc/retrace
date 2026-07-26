# SPDX-License-Identifier: BSD-2-Clause
#
# Sanitizer + coverage flag helpers. See TODO.roadmap/08-testing-quality.md.
#
# Usage from the top-level CMakeLists.txt:
#
#   include(Sanitizers)
#
# These read the options RETRACE_ENABLE_{COVERAGE,ASAN,UBSAN,TSAN}.

if(RETRACE_ENABLE_COVERAGE)
	if(NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
		message(WARNING "Code coverage requires GCC or Clang; disabling.")
		set(RETRACE_ENABLE_COVERAGE OFF)
	else()
		# --coverage at both compile and link time. Compile emits .gcno
		# notes; link pulls in libgcov for __gcov_init / __gcov_fork etc.
		add_compile_options(--coverage)
		add_link_options(--coverage)
		add_compile_definitions(RETRACE_COVERAGE)
	endif()
endif()

if(RETRACE_ENABLE_ASAN)
	if(NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
		message(WARNING "AddressSanitizer requires GCC or Clang; disabling.")
		set(RETRACE_ENABLE_ASAN OFF)
	else()
		add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
		add_link_options(-fsanitize=address)
	endif()
endif()

if(RETRACE_ENABLE_UBSAN)
	if(NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
		message(WARNING "UBSan requires GCC or Clang; disabling.")
		set(RETRACE_ENABLE_UBSAN OFF)
	else()
		add_compile_options(-fsanitize=undefined -fno-omit-frame-pointer)
		add_link_options(-fsanitize=undefined)
	endif()
endif()

if(RETRACE_ENABLE_TSAN)
	if(NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
		message(WARNING "ThreadSanitizer requires GCC or Clang; disabling.")
		set(RETRACE_ENABLE_TSAN OFF)
	elseif(RETRACE_ENABLE_ASAN)
		message(FATAL_ERROR "ASAN and TSAN are mutually exclusive.")
	else()
		add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
		add_link_options(-fsanitize=thread)
	endif()
endif()

# Coverage report target (lcov + genhtml).
if(RETRACE_ENABLE_COVERAGE)
	find_program(LCOV_PATH lcov)
	find_program(GENHTML_PATH genhtml)
	if(LCOV_PATH AND GENHTML_PATH)
		add_custom_target(coverage
			COMMAND ${LCOV_PATH} --directory ${CMAKE_BINARY_DIR} --capture
			        --output-file ${CMAKE_BINARY_DIR}/coverage.info
			COMMAND ${GENHTML_PATH} ${CMAKE_BINARY_DIR}/coverage.info
			        --output-directory ${CMAKE_BINARY_DIR}/coverage-html
			WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
			COMMENT "Generating coverage report at ${CMAKE_BINARY_DIR}/coverage-html/index.html")
	endif()
endif()
