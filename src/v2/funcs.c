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

#include "funcs.h"
#include "real_impls.h"
#include "logger.h"

#define log_err(fmt, ...) \
	retrace_logger_log(FUNCS, ERROR, fmt, ##__VA_ARGS__)

#define log_info(fmt, ...) \
	retrace_logger_log(FUNCS, INFO, fmt, ##__VA_ARGS__)

#define log_warn(fmt, ...) \
	retrace_logger_log(FUNCS, WARN, fmt, ##__VA_ARGS__)

#define log_dbg(fmt, ...) \
	retrace_logger_log(FUNCS, DEBUG, fmt, ##__VA_ARGS__)

#define MAXCOUNT_FUNCS_HASH_ENTRIES 128

struct HashEl;
struct HashEl {
	struct HashEl *next;
	const struct FuncPrototype *proto;
};

static struct HashEl funcs_hash[MAXCOUNT_FUNCS_HASH_ENTRIES];

/* trivial string hash */
static inline unsigned long hash_string(const char *str)
{
	unsigned long hash;
	int i;

	hash = 7;

	for (i = 0; i < retrace_real_impls.strlen(str); i++)
		hash = hash * 31 + str[i];

	return hash % MAXCOUNT_FUNCS_HASH_ENTRIES;
}

/* not threadsafe, must be called only once */
int retrace_funcs_init(void)
{
	const struct FuncPrototype *p;
	struct HashEl *h;
	unsigned long size;
	unsigned int i;
	unsigned long hash;

	retrace_as_get_section_info("__DATA", "__retrace_funcs", &p, &size);
	log_dbg("sizeof(struct FuncPrototype)=%d", sizeof(struct FuncPrototype));
	for (i = 0; i != size / sizeof(struct FuncPrototype); i++, p++) {

		hash = hash_string(p->name);
		h = &funcs_hash[hash];

		// log_dbg("Adding prototype for '%s', hash: %d", p->name, hash);

		if (!h->proto) {

			// log_dbg("New entry at hash: %d", hash);

			h->proto = p;
			h->next = 0;

		} else {

			while (retrace_real_impls.strcmp(
				h->proto->name, p->name) &&
				h->next) {

				// log_dbg("Passing '%s' at hash: %d", h->proto->name, hash);

				h = h->next;
			}

			if (!retrace_real_impls.strcmp(
					h->proto->name, p->name)) {
				//prototype already exists, error
				// log_dbg("Prototype for '%s' already exists", p->name);
				return -1;
			}

			h->next = (struct HashEl *)
				retrace_real_impls.malloc(sizeof(struct HashEl));

			h->next->proto = p;
			h->next->next = 0;

			// log_dbg("Added '%s' at hash: %d", p->name, hash);
		}
	}
	return 0;
}

const struct FuncPrototype *retrace_func_get(const char *func_name)
{
	int hash;
	struct HashEl *h;

	hash = hash_string(func_name);
	h = &funcs_hash[hash];

	// log_dbg("Seraching for prototype for '%s', hash: %d", func_name, hash);

	if (h->proto) {

		while (retrace_real_impls.strcmp(h->proto->name, func_name) &&
			h->next) {

			// log_dbg("Passing '%s' at hash: %d", h->proto->name, hash);

			h = h->next;
		}
	}

	if (h->proto && !retrace_real_impls.strcmp(h->proto->name, func_name)) {
		// log_dbg("Found '%s' at hash: %d", h->proto->name, hash);
		return h->proto;
	}

	// log_dbg("Not found '%s'", func_name);

	return (const struct FuncPrototype *) 0;
}
