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

//struct DataType;
struct DataType {
	char name[MAXLEN_DATATYPE_NAME + 1];

	/* For C structs hosts the members,
	 * empty for non structs
	 */
	struct StructMember struct_members[MAXCOUNT_STRUCT_MEMBERS];

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
};

extern const struct DataType retrace_data_types[];

#endif /* SRC_RETRACE_V2_DATA_TYPES_H_ */
