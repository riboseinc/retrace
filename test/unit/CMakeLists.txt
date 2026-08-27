# SPDX-License-Identifier: BSD-2-Clause
#
# Unit tests for retrace_core registries. Link directly against the
# retrace_core OBJECT library -- no LD_PRELOAD needed. Issue #481.

# otlp-c live-streaming link (TODO.trace-profile/31): the target
# only exists on POSIX builds (Windows stubs the module); empty
# elsewhere.
set(_otlp_lib)
if(TARGET otlp_c)
	list(APPEND _otlp_lib otlp_c)
endif()


# Windows (TODO.windows/04): the v2 core now builds here, and the
# portable unit set runs on the CI legs. Everything below links
# POSIX-only backend arch directories, so it stays gated.
if(RETRACE_PLATFORM_WINDOWS)
    # The logger-format scenarios previously segfaulted under
    # MSVC: real_impls never resolved atoi, so logger_init
    # called a NULL pointer (found via TODO.windows/05 boot
    # bisect, 2026-08-20). Fixed -- runs everywhere.
    add_executable(retrace_test_logger_fmt test_logger_fmt.c)
    set_target_properties(retrace_test_logger_fmt PROPERTIES
        OUTPUT_NAME test_logger_fmt
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test/unit")

    target_link_libraries(retrace_test_logger_fmt PRIVATE
        retrace_core
        retrace_supervisor_agent
        retrace_supervisor_proto
        ${_otlp_lib}
    retrace_config_json
        retrace_backends
        retrace_win_common
        retrace_headers)

    target_include_directories(retrace_test_logger_fmt PRIVATE
        ${CMAKE_SOURCE_DIR}/src/core
        ${CMAKE_SOURCE_DIR}/src/backends
        ${CMAKE_SOURCE_DIR}/src/config/json)

    add_test(NAME unit-test-logger-fmt-default
        COMMAND retrace_test_logger_fmt)
    add_test(NAME unit-test-logger-fmt-json
        COMMAND retrace_test_logger_fmt)
    set_tests_properties(unit-test-logger-fmt-json PROPERTIES
        ENVIRONMENT "RETRACE_FMT_UNDER_TEST=json")
    add_test(NAME unit-test-logger-fmt-jsonl
        COMMAND retrace_test_logger_fmt)
    set_tests_properties(unit-test-logger-fmt-jsonl PROPERTIES
        ENVIRONMENT "RETRACE_FMT_UNDER_TEST=jsonl")
    if(NOT MSVC)
        add_test(NAME unit-test-logger-fmt-ring
            COMMAND retrace_test_logger_fmt)
        set_tests_properties(unit-test-logger-fmt-ring PROPERTIES
            ENVIRONMENT "RETRACE_FMT_UNDER_TEST=ring")
    endif()

    add_executable(retrace_test_trace_load test_trace_load.c
        ${CMAKE_SOURCE_DIR}/tools/common/stream.c
        ${CMAKE_SOURCE_DIR}/tools/common/trace_load.c
        ${CMAKE_SOURCE_DIR}/src/config/json/parson.c)
    set_target_properties(retrace_test_trace_load PROPERTIES
        OUTPUT_NAME test_trace_load
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test/unit")

    target_include_directories(retrace_test_trace_load PRIVATE
        ${CMAKE_SOURCE_DIR}/tools/common
        ${CMAKE_SOURCE_DIR}/src/config/json
        ${CMAKE_SOURCE_DIR}/test/property/parson_stub)

    add_test(NAME unit-test-trace-load
        COMMAND retrace_test_trace_load)
    set_tests_properties(unit-test-logger-fmt-default
        unit-test-logger-fmt-json unit-test-logger-fmt-jsonl
        PROPERTIES LABELS "unit")
    set_tests_properties(unit-test-trace-load PROPERTIES LABELS "unit")

    # PE-section registry walk (TODO.windows/05): the .rtrA/.rtrF/
    # .rtrD sections resolve via the module's own PE headers.
    # Arch-neutral: runs on x64 and arm64 legs alike.
    add_executable(retrace_test_win_registry test_win_registry.c)
    set_target_properties(retrace_test_win_registry PROPERTIES
        OUTPUT_NAME test_win_registry
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test/unit")

    target_link_libraries(retrace_test_win_registry PRIVATE
        retrace_core
        retrace_supervisor_agent
        retrace_supervisor_proto
        ${_otlp_lib}
    retrace_config_json
        retrace_backends
        retrace_win_common
        retrace_headers)

    target_include_directories(retrace_test_win_registry PRIVATE
        ${CMAKE_SOURCE_DIR}/src/core
        ${CMAKE_SOURCE_DIR}/src/backends
        ${CMAKE_SOURCE_DIR}/src/config/json)

    add_test(NAME unit-test-win-registry
        COMMAND retrace_test_win_registry)
    set_tests_properties(unit-test-win-registry PROPERTIES LABELS "unit")

    # Wrapper runtime round trip (TODO.windows/05,
    # TODO.trace-profile/12): all arches. The arm64 decoder now
    # REFUSES non-relocatable prologues (v2.16.0 attempt blindly
    # copied ADRP instructions into the trampoline and
    # segfaulted); a refusal fails this test with the reason
    # instead of crashing.
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64|amd64|aarch64|arm64|ARM64")
        add_executable(retrace_test_win_wrapper test_win_wrapper.c)
        set_target_properties(retrace_test_win_wrapper PROPERTIES
            OUTPUT_NAME test_win_wrapper
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test/unit")

        target_link_libraries(retrace_test_win_wrapper PRIVATE
            retrace_core
            retrace_supervisor_agent
            retrace_supervisor_proto
            ${_otlp_lib}
            retrace_config_json
            retrace_backends
            retrace_win_common
            retrace_headers)

        target_include_directories(retrace_test_win_wrapper PRIVATE
            ${CMAKE_SOURCE_DIR}/src/core
            ${CMAKE_SOURCE_DIR}/src/backends
            ${CMAKE_SOURCE_DIR}/src/backends/win_common
            ${CMAKE_SOURCE_DIR}/src/config/json)

        # The wrapper round trip: proven on MinGW and MSVC
        # (TODO.trace-profile/07 closed: trampoline jump-back +
        # LLP64 pointer truncation). TIMEOUT bounds any future
        # regression loop to a fast failure, not a hung job.
        #
        # TODO.trace-profile/07+23 closed: the "env mystery"
        # was a measurement artifact. The logger never closes
        # its append handle (JSON framing), so the test's reader
        # must open with full sharing -- a FILE_SHARE_READ-only
        # open dies with ERROR_SHARING_VIOLATION and a healthy
        # 4 KB log READS as empty. No RETRACE_WIN_DIAG forcing.
        add_test(NAME unit-test-win-wrapper
            COMMAND retrace_test_win_wrapper)
        set_tests_properties(unit-test-win-wrapper
            PROPERTIES LABELS "integration" TIMEOUT 120)
    endif()
    return()
endif()

# The arch-specific include is needed because data_types.h includes
# arch_spec_macros.h, which differs per platform. Compute the path
# here since _retrace_arch_include from src/core/CMakeLists.txt is
# not in scope.
if(RETRACE_PLATFORM_DARWIN)
	if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
		set(_unit_arch_include
			${CMAKE_SOURCE_DIR}/src/backends/preload_macho/aarch64)
		set(_unit_backend_lib retrace_backend_preload_macho)
	else()
		set(_unit_arch_include
			${CMAKE_SOURCE_DIR}/src/backends/preload_macho/x86_64)
		set(_unit_backend_lib retrace_backend_preload_macho)
	endif()
elseif(RETRACE_PLATFORM_LINUX)
	if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
		set(_unit_arch_include
			${CMAKE_SOURCE_DIR}/src/backends/preload_elf/aarch64)
		set(_unit_backend_lib retrace_backend_preload_elf)
	else()
		set(_unit_arch_include
			${CMAKE_SOURCE_DIR}/src/backends/preload_elf/x86_64)
		set(_unit_backend_lib retrace_backend_preload_elf)
	endif()
elseif(RETRACE_PLATFORM_FREEBSD OR RETRACE_PLATFORM_OPENBSD OR RETRACE_PLATFORM_NETBSD)
	set(_unit_arch_include
		${CMAKE_SOURCE_DIR}/src/backends/preload_bsd/x86_64)
	set(_unit_backend_lib retrace_backend_preload_bsd)
endif()

# Function-inventory conformance (the architecture-review
# deepening): every backend's funcs_symbols.S must cover the
# shared funcs_symbols.def. The v2.36.0 linux-aarch64 incident
# (28 silent no-op functions incl. the socket family) becomes a
# red test instead of shipped drift. Python-driven; the platform
# gates (HAVE_ACCEPT4 / HAVE_ISOC99_SCANF) come from config.h.
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
	add_test(NAME unit-test-inventory-conformance
		COMMAND ${Python3_EXECUTABLE}
			${CMAKE_CURRENT_SOURCE_DIR}/test_inventory_conformance.py
			--cc ${CMAKE_C_COMPILER}
			--src ${CMAKE_SOURCE_DIR}
			--build ${CMAKE_BINARY_DIR})
	set_tests_properties(unit-test-inventory-conformance
		PROPERTIES LABELS "unit")
endif()

#
# One-line unit-test registration (the architecture-review
# deepening): POSIX unit tests here come in two shapes and
# both register through this one interface.
#   retrace_add_unit_test(<name>
#       [STANDALONE]           no engine includes/libs: the
#                               test compiles its own sources
#                               (parson/tool .c files) and
#                               needs the parson_stub headers
#                               to win over the real ones
#       [TEST_NAME t]          default: <name> with _ -> -
#       [EXTRA_SOURCES src...]
#       [EXTRA_INCLUDES dir...]
#       [EXTRA_LIBS lib...])
# Engine-linked tests get the standard include/link/definition
# set once, here. Extra test properties (WORKING_DIRECTORY,
# further add_test variants, conditional links) stay explicit
# at the call site.
#
function(retrace_add_unit_test name)
    cmake_parse_arguments(T "STANDALONE" "TEST_NAME"
        "EXTRA_SOURCES;EXTRA_INCLUDES;EXTRA_LIBS" ${ARGN})

    add_executable(retrace_test_${name} test_${name}.c
        ${T_EXTRA_SOURCES})
    set_target_properties(retrace_test_${name} PROPERTIES
        OUTPUT_NAME test_${name}
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test/unit")

    if(NOT T_STANDALONE)
        target_include_directories(retrace_test_${name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/core
            ${CMAKE_SOURCE_DIR}/src/backends
            ${CMAKE_SOURCE_DIR}/src/config/json
            ${_unit_arch_include}
            ${T_EXTRA_INCLUDES})
        target_link_libraries(retrace_test_${name} PRIVATE
            retrace_core
            retrace_supervisor_agent
            retrace_supervisor_proto
            ${_otlp_lib}
            retrace_config_json
            retrace_backends
            ${_unit_backend_lib}
            retrace_preload_common
            retrace_headers
            ${CMAKE_DL_LIBS}
            Threads::Threads)
    else()
        target_include_directories(retrace_test_${name} PRIVATE
            ${T_EXTRA_INCLUDES})
    endif()

    if(T_EXTRA_LIBS)
        target_link_libraries(retrace_test_${name} PRIVATE ${T_EXTRA_LIBS})
    endif()

    if(RETRACE_PLATFORM_LINUX OR RETRACE_PLATFORM_FREEBSD)
        target_compile_definitions(retrace_test_${name} PRIVATE _GNU_SOURCE _DEFAULT_SOURCE)
    elseif(RETRACE_PLATFORM_DARWIN)
        target_compile_definitions(retrace_test_${name} PRIVATE _DARWIN_C_SOURCE)
    endif()

    if(T_TEST_NAME)
        set(_tname ${T_TEST_NAME})
    else()
        string(REPLACE "_" "-" _tname ${name})
    endif()
    add_test(NAME unit-test-${_tname}
        COMMAND retrace_test_${name})
    set_tests_properties(unit-test-${_tname} PROPERTIES LABELS "unit")
endfunction()

retrace_add_unit_test(registries)

#
# parse_printf_format shim tests. The shim itself is a static inline
# in src/core/printf_compat.h; the test renames it via macro so it
# can be tested even on hosts that have a system parse_printf_format.
#
retrace_add_unit_test(printf_format
    STANDALONE
    EXTRA_INCLUDES ${CMAKE_SOURCE_DIR}/src/core)

#
# script_resolver tests (ADR-0013 extraction). Verifies the JSON
# intercept_script lookup semantics against a constructed config.
#
retrace_add_unit_test(script_resolver)

#
# sockaddr_inspect tests (TODO.complete/15). The helper is a pure C
# function -- no engine, no LD_PRELOAD. Minimal real_impls setup in
# the test main().
#
retrace_add_unit_test(sockaddr_inspect)

#
# addr_deny action unit tests (TODO.complete/15). The action is
# looked up by name via retrace_actions_get(); ThreadContext is
# constructed inline. No engine, no LD_PRELOAD.
#
retrace_add_unit_test(addr_deny)

#
# modify_return_value_int action unit tests (TODO.complete/14).
#
retrace_add_unit_test(modify_return_value_int
    EXTRA_INCLUDES ${CMAKE_SOURCE_DIR}/test/helpers)

#
# retraced ctl-plane command surface, unit-tested through the
# extracted module (buffer sink, no socket/daemon).
#
retrace_add_unit_test(retraced_ctl
    EXTRA_SOURCES ${CMAKE_SOURCE_DIR}/tools/retraced/retraced_ctl.c
        ${CMAKE_SOURCE_DIR}/tools/retraced/registry.c
        ${CMAKE_SOURCE_DIR}/tools/retraced/journal.c
    EXTRA_INCLUDES ${CMAKE_SOURCE_DIR}/tools/retraced
        ${CMAKE_SOURCE_DIR}/tools/retraced/../..
        ${CMAKE_SOURCE_DIR}/src/supervisor
        ${CMAKE_SOURCE_DIR}/src/config/json)

#
# evidence-pipeline formatter tests: the zero-alloc stack path
# and its decline contract (escaping/oversize -> heap path).
#
retrace_add_unit_test(event_format)

#
# freeze action unit tests (TODO.supervisor/09 quiet hold): the
# fabricated returns per type and the pure-timeout exemption.
#
retrace_add_unit_test(freeze)

#
# call_count_limit action unit tests (TODO.complete/14).
#
retrace_add_unit_test(call_count_limit
    EXTRA_INCLUDES ${CMAKE_SOURCE_DIR}/test/helpers)

#
# sandbox action unit tests (TODO.complete/14).
#
retrace_add_unit_test(sandbox)

#
# Policy acceptance gates (TODO.supervisor/05): the agent's
# validator against malformed / regressing / expired payloads,
# plus the applied swap (cache rebuilt from the new tree).
# Boots the core so the config cache is live.
#
retrace_add_unit_test(agent_policy)

#
# modify_in_param_int action unit tests (TODO.complete/14).
#
retrace_add_unit_test(modify_in_param_int)

#
# fuzzing_seed action unit tests (TODO.complete/14).
#
retrace_add_unit_test(fuzzing_seed
    EXTRA_INCLUDES ${CMAKE_SOURCE_DIR}/test/helpers)

#
# delay action unit tests (TODO.complete/14).
#
retrace_add_unit_test(delay)

#
# log_params action unit tests (TODO.complete/14).
#
retrace_add_unit_test(log_params)

#
# call_real action unit tests (TODO.complete/14).
#
retrace_add_unit_test(call_real)

#
# incomplete_io action unit tests (TODO.complete/14).
#
retrace_add_unit_test(incomplete_io)

#
# memory_fuzz action unit tests (TODO.complete/14).
#
retrace_add_unit_test(memory_fuzz)

#
# as_call_real_dispatch unit tests (TODO.trace-profile/28).
#
retrace_add_unit_test(as_call_real)

#
# fuzz_str action unit tests (TODO.trace-profile/25).
#
retrace_add_unit_test(fuzz_str
    EXTRA_INCLUDES ${CMAKE_SOURCE_DIR}/src/core/actions)
set_tests_properties(unit-test-fuzz-str PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/test/fixtures)

#
# modify_in_param_str action unit tests (TODO.complete/14).
#
retrace_add_unit_test(modify_in_param_str)

#
# reentrance_guard unit tests (TODO.complete/13).
#
retrace_add_unit_test(reentrance_guard)

#
# otlp_live unit tests (TODO.trace-profile/31).
# Verifies lifecycle, null-safety, and the no-op-when-uninitialized
# contract. End-to-end testing (with the fixture HTTP server) lives
# in the integration test.
#
retrace_add_unit_test(otlp_live)
retrace_add_unit_test(otlp_allocator)

# The control-protocol frame codec (TODO.supervisor/01): pinned
# byte-exact to the conformance suite's goldens (the SSOT is
# protocol.h's message table). STANDALONE -- no engine linkage.
retrace_add_unit_test(rpc_frame
    STANDALONE
    EXTRA_LIBS retrace_supervisor_proto)
set_tests_properties(unit-test-rpc-frame PROPERTIES
    ENVIRONMENT "RETRACE_RPC_GOLDEN_DIR=${CMAKE_SOURCE_DIR}/share/rpc-schema/golden")

#
# Lock-free log ring unit tests (TODO.complete/19 P0 PR A).
# Verifies SPSC ring semantics: push/drain, wrap-around, drop-on-full,
# text truncation, early-stop drain, NULL safety, multi-thread isolation.
#
retrace_add_unit_test(log_ring)

#
# Background log flusher unit tests (TODO.complete/19 PR B).
# Verifies the flusher thread lifecycle + entry delivery contract.
#
retrace_add_unit_test(log_flusher)

#
# Logger output-format tests (TODO.windows/07): the default JSON
# array document vs RETRACE_LOGGER_FMT=jsonl streaming output.
#
retrace_add_unit_test(logger_fmt
    TEST_NAME logger-fmt-default)

add_test(NAME unit-test-logger-fmt-json
    COMMAND retrace_test_logger_fmt)
set_tests_properties(unit-test-logger-fmt-json
    PROPERTIES LABELS "unit"
    ENVIRONMENT "RETRACE_FMT_UNDER_TEST=json")

add_test(NAME unit-test-logger-fmt-jsonl
    COMMAND retrace_test_logger_fmt)
set_tests_properties(unit-test-logger-fmt-jsonl
    PROPERTIES LABELS "unit"
    ENVIRONMENT "RETRACE_FMT_UNDER_TEST=jsonl")

#
# Per-thread call-sequence hash unit tests (TODO.complete/24).
# Verifies FNV-1a rolling hash: env gating, determinism, uniqueness,
# per-thread isolation, walk semantics, NULL safety, multi-thread.
#
retrace_add_unit_test(call_hash)

#
# modify_in_param_arr action unit tests (TODO.complete/14).
# Completes the 14-action sweep.
#
retrace_add_unit_test(modify_in_param_arr)

#
# JSON config loader unit tests (TODO.complete/14). conf.c lives
# in src/config/json/, so we add that to includes.
#
retrace_add_unit_test(conf)

#
# caller_match unit tests (TODO.complete/17).
#
retrace_add_unit_test(caller_match)

#
# caller_cache unit tests (TODO.complete/17 P1).
#
retrace_add_unit_test(caller_cache)

#
# filter action unit tests (TODO.complete/20).
#
retrace_add_unit_test(filter
    EXTRA_INCLUDES ${CMAKE_SOURCE_DIR}/test/helpers)

#
# decode_http action unit tests (TODO.complete/23).
#
retrace_add_unit_test(decode_http
    EXTRA_INCLUDES ${CMAKE_SOURCE_DIR}/test/helpers)

#
# config_cache unit tests (TODO.complete/18).
#
retrace_add_unit_test(config_cache)

#
# decode_dns action unit tests (TODO.complete/23). Uses shared
# test_utils.h for the first time in the test suite.
#
retrace_add_unit_test(decode_dns
    EXTRA_INCLUDES ${CMAKE_SOURCE_DIR}/test/helpers)

#
# Audit policy unit tests (TODO.complete/26). policy_rule_matches
# is the audit-converter's predicate engine -- exercises every
# predicate type (func_exact, func_prefix, path_contains,
# env_pattern suffix/prefix/exact), AND semantics, and NULL safety.
# Compiles policy.c directly from tools/audit-converter/.
#
set(_audit_policy_source
    ${CMAKE_SOURCE_DIR}/tools/audit-converter/policy.c)
set(_parson_for_tools ${CMAKE_SOURCE_DIR}/src/config/json/parson.c)

retrace_add_unit_test(policy
    STANDALONE
    EXTRA_SOURCES ${_audit_policy_source} ${_parson_for_tools}
    EXTRA_INCLUDES ${CMAKE_SOURCE_DIR}/tools/audit-converter ${CMAKE_SOURCE_DIR}/src/config/json ${CMAKE_SOURCE_DIR}/test/property/parson_stub)

#
# Trace-diff normalize unit tests (TODO.complete/27). Verifies
# call-count aggregation, duration sum, engine-noise filtering,
# ordering preservation, and NULL safety. Compiles normalize.c
# directly from tools/trace-diff/.
#
retrace_add_unit_test(normalize
    STANDALONE
    EXTRA_SOURCES ${CMAKE_SOURCE_DIR}/tools/trace-diff/normalize.c ${_parson_for_tools}
    EXTRA_INCLUDES ${CMAKE_SOURCE_DIR}/tools/trace-diff ${CMAKE_SOURCE_DIR}/src/config/json ${CMAKE_SOURCE_DIR}/test/property/parson_stub)

#
# Threshold math unit tests for retrace-diff (TODO.complete/27).
# diff_exceeds_threshold gates CI regression reports; wrong answers
# here mean either false alarms in clean PRs or missed regressions
# in dirty PRs. Pure function, no parson/normalize deps.
#
retrace_add_unit_test(threshold
    STANDALONE
    EXTRA_SOURCES ${CMAKE_SOURCE_DIR}/tools/trace-diff/threshold.c
    EXTRA_INCLUDES ${CMAKE_SOURCE_DIR}/tools/trace-diff)

#
# LCS alignment unit tests (TODO.complete/27 P1). diff_lcs_walk
# drives the call-order diff (cookbook 26): the edit count is what
# CI gates on and the alignment is what users read. Pure algorithm,
# no parson/normalize deps.
#
retrace_add_unit_test(lcs
    STANDALONE
    EXTRA_SOURCES ${CMAKE_SOURCE_DIR}/tools/trace-diff/lcs.c
    EXTRA_INCLUDES ${CMAKE_SOURCE_DIR}/tools/trace-diff)

#
# Parson allocation-budget regression tests (TODO.complete/33 P0).
# Guards against regressions where someone tightens the budget
# (breaking real configs) or fails to reset it between calls
# (breaking repeated parses). Compiles parson.c via the parson_stub
# real_impls.h (same pattern as the property tests).
#
retrace_add_unit_test(parson_budget
    STANDALONE
    EXTRA_SOURCES ${CMAKE_SOURCE_DIR}/src/config/json/parson.c
    EXTRA_INCLUDES ${CMAKE_SOURCE_DIR}/src/config/json ${CMAKE_SOURCE_DIR}/test/property/parson_stub)

#
# Native process attach tests (v2.4.0). Exercises the public
# retrace_attach_process / retrace_list_backends APIs. On Linux the
# attach leg forks a child and PTRACE_ATTACHes to it (permitted for
# a direct child under default yama); elsewhere it verifies the
# clean-failure contract. Needs the full engine + backends because
# the ptrace trace loop dispatches through retrace_engine_wrapper.
#
retrace_add_unit_test(attach)
if(RETRACE_PLATFORM_LINUX)
    target_link_libraries(retrace_test_attach PRIVATE retrace_backend_ptrace)
endif()

#
# Audit scan-engine unit tests (TODO.complete/26). audit_scan_trace
# drives the policy matcher across a trace and collects findings --
# false negatives here mean missed violations in compliance reports.
# Compiles scan.c + policy.c directly from tools/audit-converter/.
#
retrace_add_unit_test(scan
    STANDALONE
    EXTRA_SOURCES ${CMAKE_SOURCE_DIR}/tools/audit-converter/scan.c ${_audit_policy_source} ${_parson_for_tools}
    EXTRA_INCLUDES ${CMAKE_SOURCE_DIR}/tools/audit-converter ${CMAKE_SOURCE_DIR}/src/config/json ${CMAKE_SOURCE_DIR}/test/property/parson_stub)

#
# Audit formatter unit tests (TODO.complete/26). audit_format_sarif
# is what GitHub Code Scanning ingests -- a structural regression
# breaks alert ingestion silently. Pins the SARIF 2.1.0 skeleton
# and every field Code Scanning keys off, plus the default report.
# Compiles format.c + scan.c + policy.c from tools/audit-converter/.
#
retrace_add_unit_test(format
    STANDALONE
    EXTRA_SOURCES ${CMAKE_SOURCE_DIR}/tools/audit-converter/format.c ${CMAKE_SOURCE_DIR}/tools/audit-converter/scan.c ${_audit_policy_source} ${_parson_for_tools}
    EXTRA_INCLUDES ${CMAKE_SOURCE_DIR}/tools/audit-converter ${CMAKE_SOURCE_DIR}/src/config/json ${CMAKE_SOURCE_DIR}/test/property/parson_stub)

#
# Z-score stats unit tests (TODO.complete/27 P2). diff_stats_compute
# drives the --stats CI gate (cookbook 25) -- wrong mean/stddev/z
# means missed regressions or false alarms on every PR. Needs -lm
# for sqrt/fabs (part of libSystem on macOS).
#
retrace_add_unit_test(stats
    STANDALONE
    EXTRA_SOURCES ${CMAKE_SOURCE_DIR}/tools/trace-diff/stats.c
    EXTRA_INCLUDES ${CMAKE_SOURCE_DIR}/tools/trace-diff
    EXTRA_LIBS m)

#
# PDF writer unit tests (TODO.complete/26 P2). The PDF is a
# compliance deliverable -- malformed structure fails viewers and
# auditors' tooling. pdf_writer.c is decoupled from parson and
# the Findings model (takes string arrays), so this links alone.
#
retrace_add_unit_test(pdf
    STANDALONE
    EXTRA_SOURCES ${CMAKE_SOURCE_DIR}/tools/audit-converter/pdf_writer.c
    EXTRA_INCLUDES ${CMAKE_SOURCE_DIR}/tools/audit-converter)

#
# Public-API surface guard (ADR-0014). dlsyms every function
# declared in <retrace/retrace.h> from the running image so a
# declared-but-unlinked symbol fails the build. Links the full
# library (the symbols live across registry.c + public_api.c).
#
retrace_add_unit_test(public_api
    EXTRA_INCLUDES ${CMAKE_SOURCE_DIR}/include)
# dlsym(self) must see the exe's symbols: macOS exports by
# default, Linux needs --export-dynamic (ADR-0014 guard).
set_target_properties(retrace_test_public_api PROPERTIES
    ENABLE_EXPORTS ON)

#
# Correlate matcher unit tests (TODO.next-level/02). Path
# normalization (NT forms), path comparison, the sorted path set,
# and the escape set-difference decision. Compiles match.c
# directly from tools/correlate/.
#
retrace_add_unit_test(correlate_match
    STANDALONE
    EXTRA_SOURCES ${CMAKE_SOURCE_DIR}/tools/correlate/match.c ${_parson_for_tools}
    EXTRA_INCLUDES ${CMAKE_SOURCE_DIR}/tools/correlate ${CMAKE_SOURCE_DIR}/src/config/json ${CMAKE_SOURCE_DIR}/test/property/parson_stub)

#
# Tolerant log scanner unit tests (TODO.next-level/02). retrace
# emits one JSON array document; a crashed trace truncates the
# tail. The scanner must yield every complete entry from array,
# JSONL, and truncated shapes.
#
retrace_add_unit_test(correlate_stream
    STANDALONE
    EXTRA_SOURCES ${CMAKE_SOURCE_DIR}/tools/common/stream.c ${_parson_for_tools}
    EXTRA_INCLUDES ${CMAKE_SOURCE_DIR}/tools/common ${CMAKE_SOURCE_DIR}/src/config/json ${CMAKE_SOURCE_DIR}/test/property/parson_stub)

#
# Shared tolerant trace loader tests (TODO.windows/07): array
# document, JSONL, and truncated tails all yield the same
# entries for the offline tools (audit, diff, correlate).
#
retrace_add_unit_test(trace_load
    STANDALONE
    EXTRA_SOURCES ${CMAKE_SOURCE_DIR}/tools/common/stream.c ${CMAKE_SOURCE_DIR}/tools/common/trace_load.c ${_parson_for_tools}
    EXTRA_INCLUDES ${CMAKE_SOURCE_DIR}/tools/common ${CMAKE_SOURCE_DIR}/src/config/json ${CMAKE_SOURCE_DIR}/test/property/parson_stub)

#
# Procmon CSV scanner unit tests (TODO.next-level/06). Quoted
# fields, embedded commas/quotes/CRLF, BOM, truncation, and the
# dropped-unterminated-quote policy.
#
retrace_add_unit_test(procmon_csv
    STANDALONE
    EXTRA_SOURCES ${CMAKE_SOURCE_DIR}/tools/procmon2retrace/csv.c
    EXTRA_INCLUDES ${CMAKE_SOURCE_DIR}/tools/procmon2retrace)

#
# Procmon -> retrace entry converter unit tests
# (TODO.next-level/06): header mapping and the entry shape.
#
retrace_add_unit_test(procmon_convert
    STANDALONE
    EXTRA_SOURCES ${CMAKE_SOURCE_DIR}/tools/procmon2retrace/csv.c ${CMAKE_SOURCE_DIR}/tools/procmon2retrace/convert.c ${_parson_for_tools}
    EXTRA_INCLUDES ${CMAKE_SOURCE_DIR}/tools/procmon2retrace ${CMAKE_SOURCE_DIR}/src/config/json ${CMAKE_SOURCE_DIR}/test/property/parson_stub)

#
# Profiler aggregation + capability scan unit tests (TODO.windows/08).
# aggregate.c reduces both trace schemas; capability.c statically
# scans executable segments for raw-syscall gadgets.
#
retrace_add_unit_test(profile_aggregate
    STANDALONE
    EXTRA_SOURCES ${CMAKE_SOURCE_DIR}/tools/profiler/aggregate.c ${CMAKE_SOURCE_DIR}/tools/profiler/capability.c ${CMAKE_SOURCE_DIR}/tools/correlate/match.c ${_parson_for_tools}
    EXTRA_INCLUDES ${CMAKE_SOURCE_DIR}/tools/profiler ${CMAKE_SOURCE_DIR}/tools/correlate ${CMAKE_SOURCE_DIR}/src/config/json ${CMAKE_SOURCE_DIR}/test/property/parson_stub)


#
# Profile drift + validation unit tests (TODO.trace-profile/02,
# 06). diff.c merges two aggregated profiles; validate.c checks
# the profile contract.
#
retrace_add_unit_test(profile_diff
    STANDALONE
    EXTRA_SOURCES ${CMAKE_SOURCE_DIR}/tools/profiler/aggregate.c ${CMAKE_SOURCE_DIR}/tools/profiler/diff.c ${CMAKE_SOURCE_DIR}/tools/profiler/validate.c ${CMAKE_SOURCE_DIR}/tools/correlate/match.c ${_parson_for_tools}
    EXTRA_INCLUDES ${CMAKE_SOURCE_DIR}/tools/profiler ${CMAKE_SOURCE_DIR}/tools/correlate ${CMAKE_SOURCE_DIR}/src/config/json ${CMAKE_SOURCE_DIR}/test/property/parson_stub)

# Jail emission + the profile JSON inverse (TODO.trace-profile/10).
#
retrace_add_unit_test(profile_jail
    STANDALONE
    EXTRA_SOURCES ${CMAKE_SOURCE_DIR}/tools/profiler/aggregate.c ${CMAKE_SOURCE_DIR}/tools/profiler/jail.c ${CMAKE_SOURCE_DIR}/tools/profiler/harden.c ${CMAKE_SOURCE_DIR}/tools/correlate/match.c ${_parson_for_tools}
    EXTRA_INCLUDES ${CMAKE_SOURCE_DIR}/tools/profiler ${CMAKE_SOURCE_DIR}/tools/correlate ${CMAKE_SOURCE_DIR}/src/config/json ${CMAKE_SOURCE_DIR}/test/property/parson_stub)
# Kernel-truth converters (TODO.trace-profile/14): dtrace
# (macOS) + truss (FreeBSD) log -> trace JSON.
#
retrace_add_unit_test(kernel_converters
    STANDALONE
    EXTRA_SOURCES ${CMAKE_SOURCE_DIR}/tools/dtrace2retrace/convert.c ${CMAKE_SOURCE_DIR}/tools/truss2retrace/convert.c ${_parson_for_tools}
    EXTRA_INCLUDES ${CMAKE_SOURCE_DIR}/tools/dtrace2retrace ${CMAKE_SOURCE_DIR}/tools/truss2retrace ${CMAKE_SOURCE_DIR}/src/config/json ${CMAKE_SOURCE_DIR}/test/property/parson_stub)
# Fuzz-report clustering tests (TODO.trace-profile/20).
#
retrace_add_unit_test(fuzz_cluster
    STANDALONE
    EXTRA_SOURCES ${CMAKE_SOURCE_DIR}/tools/fuzz-report/cluster.c ${CMAKE_SOURCE_DIR}/tools/common/stream.c ${CMAKE_SOURCE_DIR}/tools/correlate/match.c ${_parson_for_tools}
    EXTRA_INCLUDES ${CMAKE_SOURCE_DIR}/tools/fuzz-report ${CMAKE_SOURCE_DIR}/tools/common ${CMAKE_SOURCE_DIR}/tools/correlate ${CMAKE_SOURCE_DIR}/src/config/json ${CMAKE_SOURCE_DIR}/test/property/parson_stub)

# Packaging converters smoke tests (TODO.trace-profile/19):
# snap2inside + flatpak2inside run on fixtures; output is
# compared with the expected JSON.
#
add_test(NAME unit-test-snap2inside
    COMMAND ${CMAKE_COMMAND}
    -DTOOL=$<TARGET_FILE:retrace_snap2inside>
    -DFIXTURE=${CMAKE_SOURCE_DIR}/test/fixtures/snapcraft.yaml
    -DEXPECTED=${CMAKE_SOURCE_DIR}/test/fixtures/snap2inside.expected.json
    -P ${CMAKE_SOURCE_DIR}/test/fixtures/run_converter.cmake)
set_tests_properties(unit-test-snap2inside PROPERTIES
    ENVIRONMENT "SNAP2INSIDE_HOME=/home/testuser")
set_tests_properties(unit-test-snap2inside PROPERTIES LABELS "unit")

add_test(NAME unit-test-flatpak2inside
    COMMAND ${CMAKE_COMMAND}
    -DTOOL=$<TARGET_FILE:retrace_flatpak2inside>
    -DFIXTURE=${CMAKE_SOURCE_DIR}/test/fixtures/flatpak-manifest.json
    -DEXPECTED=${CMAKE_SOURCE_DIR}/test/fixtures/flatpak2inside.expected.json
    -P ${CMAKE_SOURCE_DIR}/test/fixtures/run_converter.cmake)
set_tests_properties(unit-test-flatpak2inside PROPERTIES LABELS "unit")
# ktrace converter smoke test (TODO.trace-profile/21).
#
add_test(NAME unit-test-ktrace2retrace
    COMMAND ${CMAKE_COMMAND}
    -DTOOL=$<TARGET_FILE:retrace_ktrace2retrace>
    -DFIXTURE=${CMAKE_SOURCE_DIR}/test/fixtures/kdump.log
    -DEXPECTED=${CMAKE_SOURCE_DIR}/test/fixtures/ktrace2retrace.expected.json
    -P ${CMAKE_SOURCE_DIR}/test/fixtures/run_converter.cmake)
set_tests_properties(unit-test-ktrace2retrace PROPERTIES LABELS "unit")
# etw converter smoke test (TODO.trace-profile/24).
#
add_test(NAME unit-test-etw2retrace
    COMMAND ${CMAKE_COMMAND}
    -DTOOL=$<TARGET_FILE:retrace_etw2retrace>
    -DFIXTURE=${CMAKE_SOURCE_DIR}/test/fixtures/etw-events.jsonl
    -DEXPECTED=${CMAKE_SOURCE_DIR}/test/fixtures/etw2retrace.expected.json
    -P ${CMAKE_SOURCE_DIR}/test/fixtures/run_converter.cmake)
set_tests_properties(unit-test-etw2retrace PROPERTIES LABELS "unit")



