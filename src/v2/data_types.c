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

#include "data_types.h"
#include "logger.h"
#include "real_impls.h"

#define log_err(fmt, ...) \
	retrace_logger_log(DATA_TYPES, ERROR, fmt, ##__VA_ARGS__)

#define log_info(fmt, ...) \
	retrace_logger_log(DATA_TYPES, INFO, fmt, ##__VA_ARGS__)

#define log_warn(fmt, ...) \
	retrace_logger_log(DATA_TYPES, WARN, fmt, ##__VA_ARGS__)

#define log_dbg(fmt, ...) \
	retrace_logger_log(DATA_TYPES, DEBUG, fmt, ##__VA_ARGS__)

#define MAXCOUNT_DT_HASH_ENTRIES 16

struct HashEl {
	struct HashEl *next;
	const struct DataType *data_type;
};

static struct HashEl dts_hash[MAXCOUNT_DT_HASH_ENTRIES];

/* trivial string hash */
static inline unsigned long hash_string(const char *str)
{
	unsigned long hash;
	int i;

	hash = 7;

	for (i = 0; i < retrace_real_impls.strlen(str); i++)
		hash = hash * 31 + str[i];

	return hash % MAXCOUNT_DT_HASH_ENTRIES;
}

/* not threadsafe, must be called only once */
int retrace_datatypes_init(void)
{
	const struct DataType *p;
	struct HashEl *h;
	unsigned long size;
	unsigned int i;
	unsigned long hash;

	retrace_as_get_section_info("__DATA", "__retrace_dt", &p, &size);

	for (i = 0; i != size / sizeof(struct DataType); i++, p++) {

		hash = hash_string(p->name);
		h = &dts_hash[hash];

		// log_dbg("Adding data type '%s', hash: %d", p->name, hash);

		if (!h->data_type) {

			// log_dbg("New entry at hash: %d", hash);

			h->data_type = p;
			h->next = 0;

		} else {

			while (retrace_real_impls.strcmp(
				h->data_type->name, p->name) &&
				h->next) {

				log_dbg("Passing '%s' at hash: %d", h->data_type->name, hash);

				h = h->next;
			}

			if (!retrace_real_impls.strcmp(
					h->data_type->name, p->name)) {
				//prototype already exists, error
				log_dbg("Prototype for '%s' already exists", p->name);
				return -1;
			}

			h->next = (struct HashEl *)
				retrace_real_impls.malloc(sizeof(struct HashEl));

			h->next->data_type = p;
			h->next->next = 0;

			// log_dbg("Added '%s' at hash: %d", p->name, hash);
		}
	}
	return 0;
}

const struct DataType *retrace_datatype_get(const char *datatype_name)
{
	int hash;
	struct HashEl *h;

	hash = hash_string(datatype_name);
	h = &dts_hash[hash];

	log_dbg("Seraching for datatype for '%s', hash: %d",
		datatype_name, hash);

	if (h->data_type) {

		while (retrace_real_impls.strcmp(h->data_type->name,
			datatype_name) && h->next) {

			log_dbg("Passing '%s' at hash: %d", h->data_type->name, hash);

			h = h->next;
		}
	}

	if (h->data_type &&
		!retrace_real_impls.strcmp(h->data_type->name, datatype_name)) {

		log_dbg("Found '%s' at hash: %d", h->data_type->name, hash);
		return h->data_type;
	}

	log_dbg("Not found '%s'", datatype_name);

	return 0;
}
