/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#ifndef RETRACE_PROFILER_PROF_FEED_H_
#define RETRACE_PROFILER_PROF_FEED_H_

#include "aggregate.h"

/* the load-anything input holder shared by the profiler modes */
struct ProfFeed {
	struct Profile prof;
	int skipped;
};

int load_any(const char *path, struct ProfFeed *feed);

#endif /* RETRACE_PROFILER_PROF_FEED_H_ */
