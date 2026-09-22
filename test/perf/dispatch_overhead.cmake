# SPDX-License-Identifier: BSD-2-Clause
#
# The F2 datum (GH #866): tail share of a realistic dispatch.
#   none    = no preload (baseline)
#   nomatch = preload, config matches nothing  -> pure tail
#   capture = preload, log_params + call_real  -> full actions
# The runs go through /bin/sh with explicit env assignment:
# cmake's own child-spawn can scrub DYLD_* on macOS, which
# once silently produced no-interception numbers.
# Verdict printed, not gated (a datum, not a regression).

function(run_mode out_var preload config_path)
	set(cmd "${BENCH} mode 2>&1 >/dev/null")
	if(preload)
		set(cmd "DYLD_INSERT_LIBRARIES=${preload} LD_PRELOAD=${preload} ${cmd}")
	endif()
	if(config_path)
		set(cmd "RETRACE_JSON_CONFIG=${config_path} RETRACE_LOGGER_DEF_ENA=1 RETRACE_LOGGER_DEF_FN=${CMAKE_CURRENT_BINARY_DIR}/f2-run.log ${cmd}")
	endif()
	execute_process(COMMAND /bin/sh -c "${cmd}"
		OUTPUT_VARIABLE out ERROR_QUIET TIMEOUT 300)
	if(NOT out MATCHES "ns_int=([0-9]+)")
		message(FATAL_ERROR "no result: [${out}]")
	endif()
	set(${out_var} ${CMAKE_MATCH_1} PARENT_SCOPE)
endfunction()

set(HERE ${CMAKE_CURRENT_LIST_DIR})

run_mode(base "" "")
run_mode(tail ${LIB} ${HERE}/data/f2_nomatch.json)
run_mode(full ${LIB} ${HERE}/data/f2_capture.json)

message(STATUS "F2 dispatch overhead: baseline=${base}ns tail=${tail}ns full=${full}ns")
math(EXPR tail_ns "${tail} - ${base}")
math(EXPR full_ns "${full} - ${base}")
if(full_ns GREATER 0 AND tail_ns GREATER 0)
	math(EXPR share_pct "${tail_ns} * 100 / ${full_ns}")
	message(STATUS "F2 verdict datum: the tail is ${share_pct}% of the capture cost (${tail_ns}ns of ${full_ns}ns)")
	if(share_pct GREATER 50)
		message(STATUS "F2: the tail DOMINATES -- reopen card 22")
	else()
		message(STATUS "F2: the tail does not dominate -- ID-keyed dispatch stays not-worth-it")
	endif()
else()
	message(STATUS "F2 verdict datum: tail<=0 or full<=0 (noise); rerun on a quiet machine")
endif()
