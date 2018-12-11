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

#include <stddef.h>

/* for printf.h */
#ifndef __GNUC__
#error GNU extensions are required!
#endif

#include <printf.h>

#include "data_types.h"
#include "logger.h"
#include "real_impls.h"

#define log_err(fmt, ...) \
	retrace_logger_log(DATA_TYPES, SEVERITY_ERROR, fmt, ##__VA_ARGS__)

#define log_info(fmt, ...) \
	retrace_logger_log(DATA_TYPES, SEVERITY_INFO, fmt, ##__VA_ARGS__)

#define log_warn(fmt, ...) \
	retrace_logger_log(DATA_TYPES, SEVERITY_WARN, fmt, ##__VA_ARGS__)

#define log_dbg(fmt, ...) \
	retrace_logger_log(DATA_TYPES, SEVERITY_DEBUG, fmt, ##__VA_ARGS__)

#define MAXCOUNT_DT_HASH_ENTRIES 16

struct HashEl {
	struct HashEl *next;
	const struct DataType *data_type;
};

static struct HashEl dts_hash[MAXCOUNT_DT_HASH_ENTRIES];

/* Map printf defs to known types */
#ifdef PA_LAST
/* On OSX PA_LAST does not exist */
static int gnu_fmt_to_pbt[PA_LAST] = {
#else
static int gnu_fmt_to_pbt[]
#endif
= {
	[PA_INT] = PBT_INT,
	[PA_CHAR] = PBT_CHAR,
	[PA_WCHAR] = PBT_UNK,
	[PA_STRING] = PBT_POINTER,
	[PA_WSTRING] = PBT_UNK,
	[PA_POINTER] = PBT_POINTER,
	[PA_FLOAT] = PBT_UNK,
	[PA_DOUBLE] = PBT_UNK
};

static const struct DataType *gnu_fmt_to_dt[PBT_CNT][PFM_CNT];

/* trivial string hash */
static inline unsigned long hash_string(const char *str)
{
	unsigned long hash;
	size_t i;

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

		log_dbg("Adding data type '%s', hash: %d", p->name, hash);

		if (!h->data_type) {

			log_dbg("New entry at hash: %d", hash);

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
				log_err("data type '%s' already exists", p->name);
				return -1;
			}

			h->next = (struct HashEl *)
				retrace_real_impls.malloc(sizeof(struct HashEl));

			h->next->data_type = p;
			h->next->next = 0;

			log_dbg("Added '%s' at hash: %d", p->name, hash);
		}

		/* add gnu printf mapping */
		if (p->pa_basic_type >= PBT_CNT) {
			log_err("Wrong pe_basic_type: %d of data type '%s'",
				p->pa_basic_type, p->name);
			return -1;
		}

		if (p->pa_flag >= PFM_CNT) {
			log_err("Wrong pa_flag: %d of data type '%s'",
				p->pa_flag, p->name);
			return -1;
		}

		if (p->pa_basic_type != PBT_UNK) {
			if (gnu_fmt_to_dt[p->pa_basic_type][p->pa_flag] != NULL) {
				log_err("data type '%s' for pe_basic_type: %d"
					" pa_flag: %d already exists",
					p->name,
					p->pa_basic_type,
					p->pa_flag);
				return -1;
			}
			gnu_fmt_to_dt[p->pa_basic_type][p->pa_flag] = p;
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

	log_dbg("Searching for datatype for '%s', hash: %d",
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

static inline int gnu_modflag_to_pfm(int pa_flag)
{
	if ((pa_flag == PA_FLAG_LONG_LONG) ||
		(pa_flag == PA_FLAG_LONG_DOUBLE))
		return PFM_LONG_LONG;
	else if (pa_flag == PA_FLAG_LONG)
		return PFM_LONG;
	else if (pa_flag == PA_FLAG_SHORT)
		return PFM_SHORT;
	else if (pa_flag == PA_FLAG_PTR)
		return PFM_PTR;
	else
		return PFM_UNK;
}

static size_t unk_to_sz(const void *data,
	const struct DataType *data_type,
	char *str)
{
	retrace_real_impls.strcpy(str, "?");
	return retrace_real_impls.strlen("?");
}

static size_t unk_get_sz_size(const void *data,
	const struct DataType *data_type)
{
	return retrace_real_impls.strlen("?");
}

static int unk_to_size_t(const void *data, size_t *dst_size_t)
{
	/* cannot convert */
	return -1;
};

static int unk_get_size(const void *data,
	const struct DataType *data_type,
	size_t *dst_size_t)
{
	*dst_size_t = sizeof("?");
	return 0;
}

static const struct DataType unk_dt = {
	.name = "unknown_type",
	.struct_members[0] = {.name = ""},
	.pa_basic_type = 0,
	.pa_flag = 0,
	.to_sz = unk_to_sz,
	.get_sz_size = unk_get_sz_size,
	.to_size_t = unk_to_size_t,
	.get_size = unk_get_size
};

const struct DataType *retrace_datatype_get_unk_dt(void)
{
	/* TODO consider register with all data types */
	return &unk_dt;
}

const struct DataType *retrace_datatype_printf_to_dt(int argtype)
{
	int basic_type;
	int flag_mod;
	const struct DataType *dt;

	basic_type = argtype & ~PA_FLAG_MASK;
	flag_mod = argtype & PA_FLAG_MASK;

	/* ensure basic_type does not overflow */
	if (((char *) &gnu_fmt_to_pbt[basic_type]) >
		((char *) gnu_fmt_to_pbt) + sizeof(gnu_fmt_to_pbt)) {
		log_err("invalid basic_type: %d", basic_type);
		return &unk_dt;
	}

	basic_type = gnu_fmt_to_pbt[basic_type];
	flag_mod = gnu_modflag_to_pfm(flag_mod);

	dt = gnu_fmt_to_dt[basic_type][flag_mod];
	if (!dt) {
		log_warn("Unsupported argtype, basic_type: %d, flag_mod: %d",
			basic_type, flag_mod);
		return &unk_dt;
	}

	return dt;
}
