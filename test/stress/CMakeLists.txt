# SPDX-License-Identifier: BSD-2-Clause
#
# Stress tests for retrace v2 (TODO.complete/35). Each scenario is
# a standalone C program designed to run under LD_PRELOAD /
# DYLD_INSERT_LIBRARIES so retrace intercepts every libc call.
#
# Stress tests are NOT run by the default `ctest` invocation; they
# take too long for normal CI. Run them manually via:
#
#   DYLD_INSERT_LIBRARIES=build/src/v2/libretrace.dylib \
#     build/test/stress/stress_threads
#
# Or wire into a nightly workflow (TODO 35 P0).

add_executable(retrace_stress_threads stress_threads.c)
set_target_properties(retrace_stress_threads PROPERTIES
    OUTPUT_NAME stress_threads
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test/stress")

target_link_libraries(retrace_stress_threads PRIVATE Threads::Threads)

add_executable(retrace_stress_many_funcs stress_many_funcs.c)
set_target_properties(retrace_stress_many_funcs PROPERTIES
    OUTPUT_NAME stress_many_funcs
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test/stress")

add_executable(retrace_stress_deep_stack stress_deep_stack.c)
set_target_properties(retrace_stress_deep_stack PROPERTIES
    OUTPUT_NAME stress_deep_stack
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test/stress")

add_executable(retrace_stress_large_args stress_large_args.c)
set_target_properties(retrace_stress_large_args PROPERTIES
    OUTPUT_NAME stress_large_args
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test/stress")

# Stress scenarios are opt-in via label so `ctest -L stress` runs
# only them. Default ctest invocation skips them.
add_test(NAME stress-threads
    COMMAND retrace_stress_threads)
set_tests_properties(stress-threads PROPERTIES
    LABELS "stress"
    TIMEOUT 120)

add_test(NAME stress-many-funcs
    COMMAND retrace_stress_many_funcs)
set_tests_properties(stress-many-funcs PROPERTIES
    LABELS "stress"
    TIMEOUT 60)

add_test(NAME stress-deep-stack
    COMMAND retrace_stress_deep_stack)
set_tests_properties(stress-deep-stack PROPERTIES
    LABELS "stress"
    TIMEOUT 60)

add_test(NAME stress-large-args
    COMMAND retrace_stress_large_args)
set_tests_properties(stress-large-args PROPERTIES
    LABELS "stress"
    TIMEOUT 60)
