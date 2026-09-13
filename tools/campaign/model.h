/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef RETRACE_TOOLS_CAMPAIGN_MODEL_H_
#define RETRACE_TOOLS_CAMPAIGN_MODEL_H_

/*
 * The campaign model (TODO.impl/09): the manifest parsed into
 * plain data, and the matrix expansion -- samples x policies x
 * repeats, in a deterministic sample-major order. Pure: no
 * sockets, no processes; the unit spec proves the expansion
 * and the validation; the runner (campaign.c) executes cells.
 */

#include <stddef.h>

#define CAMPAIGN_MAX_SAMPLES 64
#define CAMPAIGN_MAX_POLICIES 64
#define CAMPAIGN_MAX_ARGV 16
#define CAMPAIGN_NAME_MAX 64

struct campaign_sample {
	char name[CAMPAIGN_NAME_MAX];
	char *argv[CAMPAIGN_MAX_ARGV];
	size_t argc;
};

struct campaign_policy {
	char name[CAMPAIGN_NAME_MAX];
	char path[512];
};

struct campaign_cell {
	int sample;
	int policy;
	int repeat;
};

struct campaign {
	struct campaign_sample samples[CAMPAIGN_MAX_SAMPLES];
	size_t n_samples;
	struct campaign_policy policies[CAMPAIGN_MAX_POLICIES];
	size_t n_policies;
	int repeats;
	int concurrency;
	int timeout_sec;
	char preload[512];
	char ctl_sock[512];
	char journal_base[512];

	/* the expanded matrix, sample-major */
	struct campaign_cell cells[CAMPAIGN_MAX_SAMPLES *
		CAMPAIGN_MAX_POLICIES * 32];
	size_t n_cells;
};

/*
 * Parse + validate + expand. Returns 0 on success; on failure
 * returns -1 and fills `err` with the FIRST broken thing (the
 * manifest is rejected whole: a farm does not run half a plan).
 */
int campaign_load(const char *manifest_path, struct campaign *c,
	char *err, size_t err_cap);

/* Free argv storage owned by the samples. */
void campaign_free(struct campaign *c);

#endif /* RETRACE_TOOLS_CAMPAIGN_MODEL_H_ */
