/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#ifndef RETRACE_TOOLS_CTL_VERBS_H_
#define RETRACE_TOOLS_CTL_VERBS_H_

/*
 * The control-plane verb list -- the SSOT every surface derives
 * from (the X-macro idiom the protocol table set): the daemon's
 * dispatch table and scope gate expand this list, and the CLI
 * prints its usage from the same rows. A verb is one line here
 * plus one handler function; nothing else enumerates names.
 *
 * Fields: (wire name, CLI name, scope suffix, args, help).
 * The scope suffix concatenates with RETRACED_SCOPE_ (tls_gate.h).
 * CLI-only commands (sign-policy) that never round-trip the
 * daemon stay off this list.
 */
#define RETRACED_CTL_VERBS(X)					      \
	X(status, status, STATUS, "",				      \
	  "daemon info, agent count")				      \
	X(ps, ps, PS, "",					      \
	  "registry table (JSON)")				      \
	X(sessions, sessions, PS, "",				      \
	  "the session tree (nested JSON)")			      \
	X(events, events, STATUS, "[--last N]",		      \
	  "journal tail + chain verdict")		      \
	X(drift, drift, PS, "",				      \
	  "kernel observations per session (two-layer)")			      \
	X(policy_push, policy-push, POLICY, "FILE",		      \
	  "push a policy to all agents")			      \
	X(freeze, freeze, POLICY, "",				      \
	  "hold every agent (wildcard freeze)")		      \
	X(thaw, thaw, POLICY, "",				      \
	  "restore the pre-freeze policy")			      \
	X(kill, kill, KILL, "PID",				      \
	  "SIGTERM one target")				      \
	X(spawn, spawn, SPAWN, "--preload LIB -- ARGV...",	      \
	  "launch a workload that joins this daemon")

#endif /* RETRACE_TOOLS_CTL_VERBS_H_ */
