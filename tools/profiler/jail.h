/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#ifndef RETRACE_PROFILER_JAIL_H_
#define RETRACE_PROFILER_JAIL_H_

#include "aggregate.h"

#include "parson.h"

/*
 * Jail emission (TODO.trace-profile/10): the retrace config that
 * jails a binary to what a profile observed. Pure model logic --
 * the CLI (profile.c) only serializes it.
 */

/*
 * Jail emission options (TODO.trace-profile/18). All optional
 * (zero/NULL = off).
 */
struct ProfJailOpts {
	int read_only;         /* deny_classes:["write"] everywhere */
	/*
	 * deception: undeclared READs are redirected to
	 * decoy_dir/<basename>
	 */
	const char *decoy_dir;
	long long pin_clock;   /* time() mocked to this epoch (>0) */
	int pin_clock_set;
};

/*
 * Every function observed in funcs_src gets a script whose first
 * action is sandbox with the allowlist as allow_paths
 * (deny-by-default), followed by call_real so allowed paths
 * execute. A path not on the list is denied before libc sees it
 * -- or, with opts->decoy_dir, redirected to a decoy.
 *
 * funcs_src scopes the scripts, paths_src supplies the
 * allowlist: with --inside these differ (declared set vs
 * observed functions). Scoped to observed functions rather than
 * a wildcard: a '*' jail also jails the dynamic loader's own
 * file opens and kills process startup. Re-profile to extend
 * coverage.
 */
JSON_Value *prof_jail_config(const struct Profile *funcs_src,
			     const struct Profile *paths_src,
			     const struct ProfJailOpts *opts);

#endif /* RETRACE_PROFILER_JAIL_H_ */
