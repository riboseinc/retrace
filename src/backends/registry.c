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

/*
 * Backend registry. See include/retrace/backend.h for the public API and
 * TODO.v2/05-backend-abstraction.md for the design.
 *
 * Registration is via constructor functions in each backend's backend.c.
 * The registry collects them into a sorted array at startup; select()
 * iterates by rank.
 */

#include <retrace/backend.h>

#include <stddef.h>
#include <string.h>

#define RETRACE_BACKEND_MAX 16

static const retrace_backend_t *registered[RETRACE_BACKEND_MAX];
static size_t registered_count;

RETRACE_BACKEND_API int
retrace_backend_register(const retrace_backend_t *backend)
{
	size_t i;

	if (backend == NULL || backend->name == NULL || backend->spawn == NULL)
		return RETRACE_BACKEND_INTERNAL;

	for (i = 0; i < registered_count; i++) {
		if (registered[i] == backend)
			return RETRACE_BACKEND_OK;
		if (strcmp(registered[i]->name, backend->name) == 0)
			return RETRACE_BACKEND_INTERNAL;
	}

	if (registered_count >= RETRACE_BACKEND_MAX)
		return RETRACE_BACKEND_INTERNAL;

	registered[registered_count++] = backend;
	return RETRACE_BACKEND_OK;
}

RETRACE_BACKEND_API size_t
retrace_backend_list(const retrace_backend_t ***out)
{
	if (out != NULL)
		*out = registered;
	return registered_count;
}

RETRACE_BACKEND_API const retrace_backend_t *
retrace_backend_find(const char *name)
{
	size_t i;

	if (name == NULL)
		return NULL;

	for (i = 0; i < registered_count; i++) {
		if (strcmp(registered[i]->name, name) == 0)
			return registered[i];
	}

	return NULL;
}

RETRACE_BACKEND_API const retrace_backend_t *
retrace_backend_select(struct retrace_engine *eng,
                        const char *target_path,
                        const char *requested_name)
{
	const retrace_backend_t *best = NULL;
	size_t i;

	if (requested_name != NULL) {
		const retrace_backend_t *b = retrace_backend_find(requested_name);
		if (b == NULL)
			return NULL;
		if (b->probe != NULL && b->probe(eng, target_path) != 1)
			return NULL;
		return b;
	}

	for (i = 0; i < registered_count; i++) {
		const retrace_backend_t *b = registered[i];

		if (b->probe != NULL && b->probe(eng, target_path) != 1)
			continue;

		if (best == NULL || b->rank < best->rank)
			best = b;
	}

	return best;
}
