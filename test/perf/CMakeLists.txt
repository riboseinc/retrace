# SPDX-License-Identifier: BSD-2-Clause
#
# Performance micro-benchmarks (TODO.complete/34). Each benchmark
# runs an operation many times, measures with clock_gettime, and
# reports min / p50 / p95 / p99 / max nanoseconds.
#
# Labeled "perf" so `ctest -L perf` opts in. Not in the default
# suite -- timings vary by host; meant for manual runs and a
# future perf.yml workflow that compares against a baseline.

if(RETRACE_PLATFORM_DARWIN)
	set(_perf_arch_include
		${CMAKE_SOURCE_DIR}/src/backends/preload_macho/aarch64)
	set(_perf_backend_lib retrace_backend_preload_macho)
elseif(RETRACE_PLATFORM_LINUX)
	if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
		set(_perf_arch_include
			${CMAKE_SOURCE_DIR}/src/backends/preload_elf/aarch64)
		set(_perf_backend_lib retrace_backend_preload_elf)
	else()
		set(_perf_arch_include
			${CMAKE_SOURCE_DIR}/src/backends/preload_elf/x86_64)
		set(_perf_backend_lib retrace_backend_preload_elf)
	endif()
elseif(RETRACE_PLATFORM_FREEBSD OR RETRACE_PLATFORM_OPENBSD OR RETRACE_PLATFORM_NETBSD)
	set(_perf_arch_include
		${CMAKE_SOURCE_DIR}/src/backends/preload_bsd/x86_64)
	set(_perf_backend_lib retrace_backend_preload_bsd)
endif()

function(add_retrace_bench name source)
	add_executable(retrace_perf_${name} ${source})
	set_target_properties(retrace_perf_${name} PROPERTIES
		OUTPUT_NAME ${name}
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test/perf")

	target_link_libraries(retrace_perf_${name} PRIVATE
		retrace_core
		retrace_config_json
		retrace_backends
		${_perf_backend_lib}
		retrace_preload_common
		retrace_headers
		${CMAKE_DL_LIBS}
		Threads::Threads)

	target_include_directories(retrace_perf_${name} PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}
		${CMAKE_SOURCE_DIR}/src/core
		${CMAKE_SOURCE_DIR}/src/backends
		${CMAKE_SOURCE_DIR}/src/config/json
		${_perf_arch_include})

	if(RETRACE_PLATFORM_LINUX OR RETRACE_PLATFORM_FREEBSD)
		target_compile_definitions(retrace_perf_${name} PRIVATE
			_GNU_SOURCE _DEFAULT_SOURCE)
	elseif(RETRACE_PLATFORM_DARWIN)
		target_compile_definitions(retrace_perf_${name} PRIVATE
			_DARWIN_C_SOURCE)
	endif()

	add_test(NAME perf-${name}
		COMMAND retrace_perf_${name})
	set_tests_properties(perf-${name} PROPERTIES
		LABELS "perf"
		TIMEOUT 60)
endfunction()

add_retrace_bench(bench_script_resolve bench_script_resolve.c)
add_retrace_bench(bench_caller_match bench_caller_match.c)
add_retrace_bench(bench_action_log_params bench_action_log_params.c)
add_retrace_bench(bench_action_call_real bench_action_call_real.c)
add_retrace_bench(bench_log_emit bench_log_emit.c)
add_retrace_bench(bench_log_ring_contention bench_log_ring_contention.c)
add_retrace_bench(bench_filter_eval bench_filter_eval.c)
