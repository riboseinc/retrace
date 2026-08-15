/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#ifndef RETRACE_AUDIT_FORMAT_H_
#define RETRACE_AUDIT_FORMAT_H_

#include "parson.h"
#include "scan.h"

/*
 * Audit output formatters (TODO.complete/26).
 *
 * Render a Findings set into a JSON document. Presentation only:
 * no rule evaluation, no trace walking (those live in policy.c
 * and scan.c). Each formatter returns a caller-owned JSON_Value.
 */

/*
 * Maps a policy severity to the SARIF level vocabulary:
 * critical/high -> "error", medium -> "warning", info -> "note".
 */
const char *audit_sarif_level(enum Severity s);

/*
 * Human-readable JSON: { policy, trace, findings[], summary }.
 * Each finding carries rule_id, severity, description,
 * entry_index, and a deep copy of the triggering log entry as
 * evidence. summary counts findings per severity.
 */
JSON_Value *audit_format_default(const struct Policy *policy,
				 const char *trace_path,
				 const struct Findings *findings);

/*
 * SARIF 2.1.0 (OASIS standard; native input to GitHub Code
 * Scanning, Azure DevOps, VS Code SARIF viewers). One run, one
 * tool driver. Each finding becomes a result with ruleId, level
 * (via audit_sarif_level), message.text, and a location whose
 * region.startLine is entry_index+1 (SARIF lines are 1-based;
 * entry_index is a 0-based array index used as a proxy).
 */
JSON_Value *audit_format_sarif(const struct Policy *policy,
			       const char *trace_path,
			       const struct Findings *findings);

#endif /* RETRACE_AUDIT_FORMAT_H_ */
