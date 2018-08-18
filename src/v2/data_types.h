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

#ifndef SRC_RETRACE_V2_DATA_TYPES_H_
#define SRC_RETRACE_V2_DATA_TYPES_H_

#include <stdlib.h>
#include "arch_spec_macros.h"

#define MAXLEN_DATATYPE_NAME 32
#define MAXLEN_STRUCT_MEMBER_NAME MAXLEN_DATATYPE_NAME
#define MAXCOUNT_STRUCT_MEMBERS 64

enum CDataModifiers {
	CDM_NOMOD = 0,

	CDM_POINTER = 0x1,

	CDM_VOLATILE = 0x2,

	/* const value */
	CDM_CONST = 0x4,

	CDM_ARRAY = 0x8,

	/* For structs */
	CMD_AGGREGATED = 0x10,

	CMD_UNION

#if 0
	/* Pointer points to char array
	 * TODO: Add support for arrays of other types (not expected)
	 */
	CDM_ARRAY = 0x8,

	/* Pointer points to C-string */
	CDT_SZ = 0x10
#endif
};

struct StructMember {
	/* name of member */
	char name[MAXLEN_STRUCT_MEMBER_NAME + 1];

	/* type name of member */
	char type[MAXLEN_DATATYPE_NAME + 1];

	/* modifiers of the member */
	enum CDataModifiers modifiers;

	/* in case modifiers & CDM_POINTER, this is the referenced type
	 * empty if void* or no dereferencing is needed
	 */
	char ref_type_name[MAXLEN_STRUCT_MEMBER_NAME + 1];

#if 0
	/* TODO support pointer dereferencing for struct members */
	/* in case modifiers & CDM_ARRAY, this the method for count calc */
	ArrayCountMethods_t count_meth;

	union {
		/* for ACM_DYN, must support to_size_t() */
		char array_cnt_member[STRUCT_MEMBER_NAME_MAX_LEN + 1];

		/* for ACM_STATIC */
		size_t array_cnt;
	} count_meth_data;
#endif

	/* if modifiers & CMD_ARRAY,
	 * this members holds the number of elements
	 */
	size_t array_cnt;
};

/* PrintfFmtBasicTypes and PrintfFmtMods
 * Superset the PA enum and FLAG macro from print.h
 * We have do do it since 0 value means PA_INT,
 * while we need it for datatypes that are not related
 * to standard types
 */
enum PrintfFmtBasicTypes
{
	PBT_UNK,
	PBT_INT,
	PBT_CHAR,
	PBT_STRING,
	PBT_POINTER,
	PBT_FLOAT,
	PBT_DOUBLE,
	PBT_CNT
};

enum PrintfFmtMods
{
	PFM_UNK,
	PFM_PTR,
	PFM_SHORT,
	PFM_LONG,
	PFM_LONG_LONG,
	/*PFM_LONG_DOUBLE,*/
	PFM_CNT
};

struct DataType {
	char name[MAXLEN_DATATYPE_NAME + 1];

	/* For C structs hosts the members,
	 * empty for non structs
	 */
	struct StructMember struct_members[MAXCOUNT_STRUCT_MEMBERS];

	/* for gnu format mapping, see printf.h */
	enum PrintfFmtBasicTypes pa_basic_type;
	enum PrintfFmtMods pa_flag;

	/**
	 * @brief serializes data object to C-string
	 *
	 * @param[in] data Pointer to data object buffer to be serialized
	 * @param[out] str Pointer of buffer to receive the
	 * C-string representation of data object
	 * @param[in] str_size Size of str buffer
	 *
	 * @return In case of error returns a negative value
	 */

	size_t (*to_sz)(const void *data,
			const struct DataType *data_type,
			char *str);

	size_t (*get_sz_size)(const void *data,
		const struct DataType *data_type);

	int (*to_size_t)(const void *data, size_t *dst_size_t);

	int (*get_size)(const void *data,
		const struct DataType *data_type,
		size_t *dst_size_t);

	int (*compare)(const void *a,
		const void *b,
		const struct DataType *data_type);
};

const struct DataType *retrace_datatype_get(const char *datatype_name);

/* as defined by parse_printf_format */
const struct DataType *retrace_datatype_printf_to_dt(int argtype);

int retrace_datatypes_init(void);

/* aligned 1 is used because i group all DataType in the same
 * section, creating an array
 */
#define retrace_datatype_define_prototypes(datatype_name) \
	retrace_as_define_var_in_sec(const struct DataType,\
		retrace_dt_##datatype_name[], \
			"__DATA", "__retrace_dt")__attribute__((aligned(1)))

#endif /* SRC_RETRACE_V2_DATA_TYPES_H_ */
