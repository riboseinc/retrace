# SPDX-License-Identifier: BSD-2-Clause

# Windows (TODO.windows/04): the per-feature LD_PRELOAD targets and
# the sh-based integration runner are POSIX-only. Windows runs its
# portable unit set only.
if(RETRACE_PLATFORM_WINDOWS)
	add_subdirectory(unit)

	# Static-CRT smoke target (TODO.trace-profile/27): /MT links
	# the CRT INTO the exe -- no ucrtbase to hook; file opens are
	# only visible at the ntdll boundary. Built (not ctest-run):
	# the CI step drives it through retrace-win-run with
	# RETRACE_WIN_NTDLL=1 and asserts the capture.
	if(MSVC AND (CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64|ARM64|arm64"))
		add_executable(retrace_static_smoke smoke/static_smoke.c)
		set_target_properties(retrace_static_smoke PROPERTIES
			OUTPUT_NAME retrace-static-smoke
			MSVC_RUNTIME_LIBRARY "MultiThreaded"
			RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test/smoke")
		target_link_libraries(retrace_static_smoke PRIVATE retrace_headers)
	endif()

	# The supervised loop on Windows (TODO.supervisor/12 P0):
	# retraced + retrace-win-run + the in-process agent, HELLO
	# with the nonce (full peer), policy push + ACK.
	find_package(Python3 COMPONENTS Interpreter)
	if(Python3_Interpreter_FOUND AND TARGET retrace_win_run)
		add_test(NAME integration-supervised-win
			COMMAND ${Python3_EXECUTABLE}
				${CMAKE_SOURCE_DIR}/test/integration/test_supervised_win.py
				$<TARGET_FILE:retrace_retraced>
				$<TARGET_FILE:retrace_win_run>)
		set_tests_properties(integration-supervised-win PROPERTIES
			LABELS "integration"
			TIMEOUT 180)
	endif()

	# retraced as a Windows SERVICE (TODO.supervisor/12 P1):
	# the real SCM lifecycle (sc create/start/stop/delete) on the
	# elevated runners.
	find_package(Python3 COMPONENTS Interpreter)
	if(Python3_Interpreter_FOUND AND TARGET retrace_retraced)
		add_test(NAME integration-retraced-service
			COMMAND ${Python3_EXECUTABLE}
				${CMAKE_SOURCE_DIR}/test/integration/test_retraced_service.py
				$<TARGET_FILE:retrace_retraced>)
		set_tests_properties(integration-retraced-service PROPERTIES
			LABELS "integration"
			TIMEOUT 180)
	endif()

	# pyretrace over the pipe (the runtime lane's Windows leg).
	find_package(Python3 COMPONENTS Interpreter)
	if(Python3_Interpreter_FOUND AND TARGET retrace_retraced)
		add_test(NAME integration-pyretrace-win
			COMMAND ${Python3_EXECUTABLE}
				${CMAKE_SOURCE_DIR}/test/integration/test_pyretrace.py
				$<TARGET_FILE:retrace_retraced>
				${CMAKE_SOURCE_DIR}/bindings/python)
		set_tests_properties(integration-pyretrace-win PROPERTIES
			LABELS "integration"
			TIMEOUT 120)
	endif()

	# The kernel-observation agents speak the pipe natively on
	# Windows (TODO.beyond-libc/03): synthetic ETW lane E2E.
	find_package(Python3 COMPONENTS Interpreter)
	if(Python3_Interpreter_FOUND AND TARGET retrace_retraced)
		add_test(NAME integration-etw-agent-win
			COMMAND ${Python3_EXECUTABLE}
				${CMAKE_SOURCE_DIR}/test/integration/test_etw_agent.py
				$<TARGET_FILE:retrace_retraced>
				${CMAKE_SOURCE_DIR}/etw-bridge/retrace-etw-agent)
		set_tests_properties(integration-etw-agent-win PROPERTIES
			LABELS "integration"
			TIMEOUT 120)
	endif()

	# retrace-ctl over the ctl pipe (the fleet CLI on Windows).
	find_package(Python3 COMPONENTS Interpreter)
	if(Python3_Interpreter_FOUND AND TARGET retrace_ctl)
		add_test(NAME integration-ctl-win
			COMMAND ${Python3_EXECUTABLE}
				${CMAKE_SOURCE_DIR}/test/integration/test_ctl_win.py
				$<TARGET_FILE:retrace_retraced>
				$<TARGET_FILE:retrace_ctl>)
		set_tests_properties(integration-ctl-win PROPERTIES
			LABELS "integration"
			TIMEOUT 120)
	endif()

	# The named-pipe daemon (TODO.supervisor/12 P0): the same
	# protocol over \\.\pipe\; a python stub walks the full
	# HELLO/WINDOW/EVENT/BYE exchange as a spectator.
	find_package(Python3 COMPONENTS Interpreter)
	if(Python3_Interpreter_FOUND AND TARGET retrace_retraced)
		add_test(NAME integration-retraced-pipe
			COMMAND ${Python3_EXECUTABLE}
				${CMAKE_SOURCE_DIR}/test/integration/test_retraced_pipe.py
				$<TARGET_FILE:retrace_retraced>)
		set_tests_properties(integration-retraced-pipe PROPERTIES
			LABELS "integration"
			TIMEOUT 120)
	endif()

	# AppContainer kernel enforcement (TODO.beyond-libc/01 P1):
	# the Windows backend E2E. Self-skips on non-Windows hosts,
	# so POSIX CI legs can carry it too if ever moved.
	find_package(Python3 COMPONENTS Interpreter)
	if(Python3_Interpreter_FOUND AND TARGET retrace_profile
	   AND TARGET retrace_enforce)
		add_test(NAME integration-enforce-win
			COMMAND ${Python3_EXECUTABLE}
				${CMAKE_SOURCE_DIR}/test/integration/test_enforce_win.py
				$<TARGET_FILE:retrace_profile>
				$<TARGET_FILE:retrace_enforce>)
		set_tests_properties(integration-enforce-win PROPERTIES
			LABELS "integration"
			TIMEOUT 120)
	endif()
	return()
endif()

#
# retrace tests. Structured into three layers (see modernization plan/08):
#   - integration/: per-feature test binaries, run under v2.
#                   Executed by the CMake-configured runtests.sh.
#   - unit/:        cmocka-based tests for v2 internals (engine, registry).
#   - smoke/:       minimal "v2 loads and runs" test per platform.
#
# v1 was removed in Phase 9 (ADR-0011); these tests now exercise v2 only.

set(_test_programs
	id env time fork exit malloc str pipe sock sock_srv file trace
	pledge scanf dlopen dir exec printf popen char mmap sbrk
	getaddrinfo log memcpy temp perror setlocale
	recvmsg writev readv)

# Most test programs are plain C that exercise libc -- they get retrace
# via LD_PRELOAD/DYLD_INSERT_LIBRARIES at runtime, not via link-time dep.
foreach(_prog ${_test_programs})
	if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_prog}.c")
		add_executable(retrace_test_${_prog} ${_prog}.c)
		set_target_properties(retrace_test_${_prog} PROPERTIES
			OUTPUT_NAME ${_prog}
			RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test")
	endif()
endforeach()

# pipe.c and config.c include v1's common.h directly via #if HAVE_CONFIG_H.
# v1 was removed in Phase 9. They need to be ported to use the v2 public
# API before they can run again. For now, skip them.
if(0)
	if(TARGET retrace_test_pipe)
		target_link_libraries(retrace_test_pipe PRIVATE retrace::v2)
	endif()
	if(TARGET retrace_test_config)
		target_link_libraries(retrace_test_config PRIVATE retrace::v2)
	endif()
endif()

# Drop v1-only test programs that can't be built without retrace_v1.
# These need rewrites against v2's public API.
foreach(_v1_dep_test config pipe)
	if(TARGET retrace_test_${_v1_dep_test})
		set_target_properties(retrace_test_${_v1_dep_test} PROPERTIES
			EXCLUDE_FROM_ALL TRUE
			EXCLUDE_FROM_DEFAULT_BUILD TRUE)
	endif()
endforeach()

# ----------------------------------------------------------------
# Integration runner. Generated by configure_file + file(GENERATE):
#   1. configure_file(@ONLY) substitutes @RETRACE_CONF_EXAMPLE@ at
#      configure time.
#   2. file(GENERATE) then evaluates $<TARGET_FILE:...> at build time,
#      giving the script absolute paths to the built v2 library and
#      test programs.
# ----------------------------------------------------------------
set(RETRACE_CONF_EXAMPLE "${CMAKE_SOURCE_DIR}/examples/getenv-fuzzing/retrace.conf.example")
set(TEST_DIR "${CMAKE_BINARY_DIR}/test")

# 1. configure_file(@ONLY) substitutes @TEST_DIR@ and @RETRACE_CONF_EXAMPLE@
#    at configure time.
# 2. file(GENERATE) then evaluates $<TARGET_FILE:retrace_v2> at build time
#    (generator expressions aren't allowed in configure_file).
configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/runtests.sh.in"
	"${CMAKE_CURRENT_BINARY_DIR}/runtests.sh.in.configured"
	@ONLY)

file(GENERATE
	OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/runtests.sh"
	INPUT  "${CMAKE_CURRENT_BINARY_DIR}/runtests.sh.in.configured")

# ----------------------------------------------------------------
# CTest registration with labels.
# ----------------------------------------------------------------

add_test(NAME integration-runtests
	COMMAND sh "${CMAKE_CURRENT_BINARY_DIR}/runtests.sh"
	WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
set_tests_properties(integration-runtests PROPERTIES
	LABELS "integration;v2")

# otlp-c live streaming integration test (TODO.trace-profile/31).
# Spawns the fixture HTTP server, runs a target under v2 with
# RETRACE_OTLP_ENDPOINT, and asserts that spans arrive at the
# server. Skipped if Python3 is not available.
add_executable(retrace_test_otlp_live_target otlp_live_target.c)
set_target_properties(retrace_test_otlp_live_target PROPERTIES
	OUTPUT_NAME otlp_live_target
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test")

# Wave C (TODO.trace-profile/32): jail-denial target -- opens a
# denied path (per the sandbox config the test generates) plus an
# allowed one; the test asserts the /v1/logs denial event.
add_executable(retrace_test_jail_otlp_target jail_otlp_target.c)
set_target_properties(retrace_test_jail_otlp_target PROPERTIES
	OUTPUT_NAME jail_otlp_target
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test")

find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
	add_test(NAME integration-otlp-live
		COMMAND ${Python3_EXECUTABLE}
			${CMAKE_SOURCE_DIR}/test/integration/test_otlp_live.py
			$<TARGET_FILE:retrace_v2>
			$<TARGET_FILE:retrace_test_otlp_live_target>
			${CMAKE_SOURCE_DIR}/test/fixtures/fixture_otlp_server.py)
	set_tests_properties(integration-otlp-live PROPERTIES
		LABELS "integration;v2"
		TIMEOUT 120)
	add_test(NAME integration-otlp-jail
		COMMAND ${Python3_EXECUTABLE}
			${CMAKE_SOURCE_DIR}/test/integration/test_otlp_jail.py
			$<TARGET_FILE:retrace_v2>
			$<TARGET_FILE:retrace_test_jail_otlp_target>
			${CMAKE_SOURCE_DIR}/test/fixtures/fixture_otlp_server.py)
	set_tests_properties(integration-otlp-jail PROPERTIES
		LABELS "integration;v2"
		TIMEOUT 120)

	# retraced daemon smoke (TODO.supervisor/02): a stub agent
	# drives HELLO/HEARTBEAT/EVENT/POLICY_ACK/PING/BYE; asserts
	# the journal chain, reboot replay, and tamper refusal.
	add_test(NAME integration-retraced-daemon
		COMMAND ${Python3_EXECUTABLE}
			${CMAKE_SOURCE_DIR}/test/integration/test_retraced_daemon.py
			$<TARGET_FILE:retrace_retraced>)
	set_tests_properties(integration-retraced-daemon PROPERTIES
		LABELS "integration"
		TIMEOUT 60)

	# The in-process agent against the live daemon
	# (TODO.supervisor/03): denial delivery + schema +
	# attribution, then daemon-absent fail-open liveness.
	add_test(NAME integration-supervisor-agent
		COMMAND ${Python3_EXECUTABLE}
			${CMAKE_SOURCE_DIR}/test/integration/test_supervisor_agent.py
			$<TARGET_FILE:retrace_retraced>
			$<TARGET_FILE:retrace_v2>
			$<TARGET_FILE:retrace_test_jail_otlp_target>)
	set_tests_properties(integration-supervisor-agent PROPERTIES
		LABELS "integration"
		TIMEOUT 60)

	# Policy distribution (TODO.supervisor/05): live tightening
	# (deny), freeze, and epoch-replay refusal across daemon
	# restarts against one running target.
	add_executable(retrace_test_policy_target policy_target.c)
	set_target_properties(retrace_test_policy_target PROPERTIES
		OUTPUT_NAME policy_target
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test")
	# Sessions/tree stitching (TODO.supervisor/04): one session
	# across fork + exec-through-hole + env-scrubbed children.
	add_executable(retrace_test_session_target session_target.c)
	set_target_properties(retrace_test_session_target PROPERTIES
		OUTPUT_NAME session_target
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test")
	# retrace-ctl, the fleet CLI (TODO.supervisor/07): the
	# whole lifecycle through one binary.
	add_test(NAME integration-supervisor-ctl
		COMMAND ${Python3_EXECUTABLE}
			${CMAKE_SOURCE_DIR}/test/integration/test_supervisor_ctl.py
			$<TARGET_FILE:retrace_retraced>
			$<TARGET_FILE:retrace_ctl>
			$<TARGET_FILE:retrace_v2>
			$<TARGET_FILE:retrace_test_policy_target>)
	set_tests_properties(integration-supervisor-ctl PROPERTIES
		LABELS "integration"
		TIMEOUT 120)

	# The kernel-observation agent (TODO.beyond-libc/03):
	# eBPF lane as a spectator peer; synthetic source proves
	# the pipe on CI (no BPF privileges).
	if(NOT WIN32)
		add_test(NAME integration-ebpf-agent
			COMMAND ${Python3_EXECUTABLE}
				${CMAKE_SOURCE_DIR}/test/integration/test_ebpf_agent.py
				$<TARGET_FILE:retrace_retraced>
				${CMAKE_SOURCE_DIR}/ebpf-bridge/retrace-ebpf-agent)
		set_tests_properties(integration-ebpf-agent
			PROPERTIES LABELS "integration"
			TIMEOUT 120)

		# The ETW kernel-observation agent (TODO.beyond-libc/03 P1):
		# the Windows lane as a spectator peer; --synthetic proves
		# the pipe on POSIX CI (the --loader source carries the
		# real Windows chain; pipes ride supervisor/12).
		if(NOT WIN32)
			add_test(NAME integration-etw-agent
				COMMAND ${Python3_EXECUTABLE}
					${CMAKE_SOURCE_DIR}/test/integration/test_etw_agent.py
					$<TARGET_FILE:retrace_retraced>
					${CMAKE_SOURCE_DIR}/etw-bridge/retrace-etw-agent)
			set_tests_properties(integration-etw-agent
				PROPERTIES LABELS "integration"
				TIMEOUT 120)
		endif()

		# Runtime agents (TODO.beyond-libc/04): pyretrace
		# (Python) and jretrace (JVM, P1) as full peers on the
		# same session. jretrace self-skips where no JDK is
		# present; both assert source=runtime events land in
		# the shared journal.
		add_test(NAME integration-pyretrace
			COMMAND ${Python3_EXECUTABLE}
				${CMAKE_SOURCE_DIR}/test/integration/test_pyretrace.py
				$<TARGET_FILE:retrace_retraced>
				${CMAKE_SOURCE_DIR}/bindings/python)
		set_tests_properties(integration-pyretrace
			PROPERTIES LABELS "integration"
			TIMEOUT 120)
		add_test(NAME integration-jretrace
			COMMAND ${Python3_EXECUTABLE}
				${CMAKE_SOURCE_DIR}/test/integration/test_jretrace.py
				$<TARGET_FILE:retrace_retraced>
				${CMAKE_SOURCE_DIR}/bindings/jvm/src/main/java)
		set_tests_properties(integration-jretrace
			PROPERTIES LABELS "integration"
			TIMEOUT 120)

		# TLS fleet transport (TODO.supervisor/08 P1 /
		# TODO.beyond-libc/05): mTLS + claim scopes. Needs
		# OpenSSL both at build (daemon linked) and at test
		# time (openssl CLI mints the throwaway PKI).
		if(OpenSSL_FOUND)
			add_test(NAME integration-tls-fleet
				COMMAND ${Python3_EXECUTABLE}
					${CMAKE_SOURCE_DIR}/test/integration/test_tls_fleet.py
					$<TARGET_FILE:retrace_retraced>)
			set_tests_properties(integration-tls-fleet
				PROPERTIES LABELS "integration"
				TIMEOUT 120)
		endif()
	endif()

	# Kernel enforcement compile (TODO.beyond-libc/01):
	# declared-set -> Landlock ruleset + seccomp floor, installed
	# by retrace-enforce. Linux-only; self-skips elsewhere or
	# where the kernel lacks the planes.
	if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
		add_test(NAME integration-enforce
			COMMAND ${Python3_EXECUTABLE}
				${CMAKE_SOURCE_DIR}/test/integration/test_enforce.py
				$<TARGET_FILE:retrace_profile>
				$<TARGET_FILE:retrace_enforce>)
		set_tests_properties(integration-enforce
			PROPERTIES LABELS "integration"
			TIMEOUT 120)
	endif()

	# Journal durability (the writer deepening): durable
	# control-plane records, recorded gaps on unclean shutdown,
	# clean-close markers, chain across boots.
	# Socket activation + privilege drop (TODO.supervisor/08
	# P2 / TODO.beyond-libc/05 P2): inherited listener served and
	# not unlinked; the drop section runs where the runner is
	# root (the Alpine container leg) and skips elsewhere.
	add_test(NAME integration-retraced-fd
		COMMAND ${Python3_EXECUTABLE}
			${CMAKE_SOURCE_DIR}/test/integration/test_retraced_fd.py
			$<TARGET_FILE:retrace_retraced>)
	set_tests_properties(integration-retraced-fd PROPERTIES
		LABELS "integration"
		TIMEOUT 120)

	add_test(NAME integration-journal-durability
		COMMAND ${Python3_EXECUTABLE}
			${CMAKE_SOURCE_DIR}/test/integration/test_journal_durability.py
			$<TARGET_FILE:retrace_retraced>
			$<TARGET_FILE:retrace_v2>
			$<TARGET_FILE:retrace_test_policy_target>)
	set_tests_properties(integration-journal-durability
		PROPERTIES LABELS "integration"
		TIMEOUT 120)

	# The farm/IR playbook, end to end (TODO.supervisor/09):
	# register, tighten mid-run, freeze, journal the story.
	if(NOT WIN32)
		add_test(NAME integration-supervisor-quickstart
			COMMAND ${CMAKE_COMMAND}
				-DDEMO_DIR=${CMAKE_SOURCE_DIR}/examples/supervisor-quickstart
				-DBUILD_DIR=${CMAKE_BINARY_DIR}
				-DCMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}
				-P ${CMAKE_SOURCE_DIR}/test/RunQuickstart.cmake)
		set_tests_properties(integration-supervisor-quickstart
			PROPERTIES LABELS "integration"
			TIMEOUT 120)
	endif()

	# Transport auth (TODO.supervisor/08 P0): socket mode
	# gates, nonce roles (spectator vs full), peer-uid refusal.
	add_test(NAME integration-supervisor-auth
		COMMAND ${Python3_EXECUTABLE}
			${CMAKE_SOURCE_DIR}/test/integration/test_supervisor_auth.py
			$<TARGET_FILE:retrace_retraced>
			$<TARGET_FILE:retrace_v2>
			$<TARGET_FILE:retrace_test_policy_target>)
	set_tests_properties(integration-supervisor-auth PROPERTIES
		LABELS "integration"
		TIMEOUT 120)

	add_test(NAME integration-supervisor-session
		COMMAND ${Python3_EXECUTABLE}
			${CMAKE_SOURCE_DIR}/test/integration/test_supervisor_session.py
			$<TARGET_FILE:retrace_retraced>
			$<TARGET_FILE:retrace_v2>
			$<TARGET_FILE:retrace_test_session_target>)
	set_tests_properties(integration-supervisor-session PROPERTIES
		LABELS "integration"
		TIMEOUT 90)

	add_test(NAME integration-supervisor-policy
		COMMAND ${Python3_EXECUTABLE}
			${CMAKE_SOURCE_DIR}/test/integration/test_supervisor_policy.py
			$<TARGET_FILE:retrace_retraced>
			$<TARGET_FILE:retrace_v2>
			$<TARGET_FILE:retrace_test_policy_target>)
	set_tests_properties(integration-supervisor-policy PROPERTIES
		LABELS "integration"
		TIMEOUT 120)
endif()

# Unit tests for retrace_core internals. Links against the OBJECT
# library so no LD_PRELOAD needed. Issue #481.
add_subdirectory(unit)

# Conformance suites: wire protocols whose artifacts must derive
# from a single source (TODO.supervisor/01+11).
add_subdirectory(conformance)

# Property-based tests for the third-party surfaces (parson, etc.).
# Pure C, no engine state. See TODO.complete/16-property-based-tests.md.
add_subdirectory(property)

# Stress tests (TODO.complete/35). Multi-threaded / high-event-rate
# scenarios that run under LD_PRELOAD. Labeled "stress" so default
# ctest skips them; run via `ctest -L stress`.
add_subdirectory(stress)

# Fuzz harnesses (TODO.complete/33). libFuzzer targets, gated by
# RETRACE_BUILD_FUZZERS=ON. The CMakeLists.txt in fuzz/ no-ops
# otherwise.
add_subdirectory(fuzz)

# Performance benchmarks (TODO.complete/34). Each runs an op many
# times and reports min/p50/p95/p99/max nanoseconds. Labeled
# "perf" so `ctest -L perf` opts in.
add_subdirectory(perf)
