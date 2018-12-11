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

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

#include "data_types.h"
#include "real_impls.h"
#include "parson.h"
#include "basic.h"
#include "logger.h"

static int get_member(const struct DataType *struct_type,
		const void *struct_data,
		const char *member_name,
		const struct DataType **member_type,
		const void **member_data)
{
	/* TODO: improve speed */
	size_t m_size;
	const struct StructMember *m_struct;
	const struct DataType *m_type;
	const void *m_data;

	/* iterate calculating offset */
	m_data = struct_data;
	m_struct = struct_type->struct_members;
	//m_type = get_member_type(m_struct->type);
	m_type = retrace_datatype_get(m_struct->type);
	if (m_type == NULL)
		return -1;

	while (retrace_real_impls.strlen(m_struct->name)
		&& retrace_real_impls.strcmp(m_struct->name, member_name)) {

		if (m_type->get_size(m_data, m_type, &m_size) < 0)
			return -2;

		/* FIXME! Handle alignment */
		m_struct++;
		m_data += m_size;
		//m_type = get_member_type(m_struct->type);
		m_type = retrace_datatype_get(m_struct->type);
		if (m_type == NULL)
			return -1;
	}

	*member_type = m_type;
	*member_data = m_data;

	return 0;
}

size_t retrace_struct_to_sz(const void *data,
	const struct DataType *data_type,
	char *str)
{
	/*
	 * serialization to JSON format.
	 * Shall be the same for struct data type
	 * under presumption that both serializations define a complex data,
	 * this allows important unification between the serialization formats
	 *
	 * example
	 *
	 * func(int_param,
	 * ptr_to_smth_param,
	 * ptr_to_arr_param,
	 * arr_count_param,
	 * ptr_to_struct_param)
	 *
	 * will be serialized to (with dereferencing one level):
	 *
	 * { "int_param" : "int_to_sz()", \
	 * "ptr_to_smth_param" : "ptr_to_sz()", \
	 * "*ptr_to_smth_param" : typeof(*)_to_sz(), \
	 * .ptr_to_arr_param = ptr_to_sz(), \
	 * .*ptr_to_arr_param = { typeof(*)_to_sz(), ..., typeof(*)_to_sz() }, \
	 * .arr_count_param = size_t_to_sz() }
	 */

	int ret;
	char *serialized_string;
	JSON_Value *root_value;
	JSON_Object *root_object;
	JSON_Value *arr_val;
	size_t sz_size;
	size_t arr_count;
	size_t arr_member_size;
	char *sz;

	/* iterate over all members */
	const struct StructMember *member;
	const struct DataType *member_type;
	const void *member_data;
	const void *arr_member_data;

	root_value = json_value_init_object();
	root_object = json_value_get_object(root_value);
	member = data_type->struct_members;
	while (retrace_real_impls.strlen(member->name)) {

		ret = get_member(data_type,
				data,
				member->name,
				&member_type,
				&member_data);

		if (ret) {
			log_err("Could not get member info for %s, error %d",
					member->name, ret);
			break;
		}

		if (member->modifiers & CDM_ARRAY) {
			arr_count = member->array_cnt;

			arr_val = json_value_init_array();
			arr_member_data = member_data;
			while (arr_count) {

				sz_size = member_type->get_sz_size(
						arr_member_data,
						arr_member_data);

				sz = (char *) retrace_real_impls.malloc(
						sz_size + 1);

				member_type->to_sz(arr_member_data,
						member_type, sz);

				json_array_append_string(json_array(arr_val),
						sz);

				retrace_real_impls.free(sz);

				/* advance to the next element */
				ret = member_type->get_size(arr_member_data,
					member_type,
					&arr_member_size);

				if (ret) {
					log_err(
						"get_size() failed for type %s, error %d",
						member_type->name,
						ret);
					break;
				}

				arr_member_data += arr_member_size;
				arr_count--;
			}

			json_object_set_value(root_object,
					member->name, arr_val);
		} else {
			sz_size = member_type->get_sz_size(member_data,
					data_type);

			sz = (char *) retrace_real_impls.malloc(
					sz_size + 1);

			member_type->to_sz(member_data,
					member_type, sz);

			json_object_set_string(root_object,
					member->name, sz);

			retrace_real_impls.free(sz);
		}

		/* process next member */
		member++;
	}

	serialized_string = json_serialize_to_string_pretty(root_value);

	/* finally */
	sz_size = retrace_real_impls.strlen(serialized_string);
	if (str != NULL)
		retrace_real_impls.sprintf(str, "%s", serialized_string);

	json_free_serialized_string(serialized_string);
	json_value_free(root_value);

	return sz_size;
}

size_t retrace_struct_get_sz_size(const void *data,
	const struct DataType *data_type)
{
	return retrace_struct_to_sz(data, data_type, NULL);
}

int retrace_struct_to_size_t(const void *data, size_t *dst_size_t)
{
	/* cannot convert struct to size_t */
	return -1;
};

int retrace_struct_get_size(const void *data,
	const struct DataType *data_type,
	size_t *dst_size_t)
{
	int ret;
	size_t member_size;

	/* iterate over all members */
	const struct StructMember *member;
	const struct DataType *member_type;
	const void *member_data;

	ret = 0;
	*dst_size_t = 0;
	member = data_type->struct_members;
	while (retrace_real_impls.strlen(member->name)) {

		ret = get_member(data_type,
				data,
				member->name,
				&member_type,
				&member_data);

		if (ret) {
			log_err("Could not get member info for %s, error %d",
				member->name, ret);
			break;
		}

		ret = member_type->get_size(member_data,
				member_type,
				&member_size);

		if (ret) {
			log_err("get_size() failed for type %s, error %d",
				member_type->name,
				ret);
			break;
		}

		if (member->modifiers & CDM_ARRAY)
			*dst_size_t += member->array_cnt * member_size;
		else
			*dst_size_t += member_size;

		/* process next member */
		member++;
	}

	return ret;
}

size_t retrace_int_to_sz(const void *data,
	const struct DataType *data_type,
	char *str)
{
	return retrace_real_impls.sprintf(str, "%d", *((const int *) data));
}

size_t retrace_int_get_sz_size(const void *data,
	const struct DataType *data_type)
{
	return retrace_real_impls.snprintf(NULL,
			0,
			"%d",
			*((const int *) data));
}

int retrace_int_to_size_t(const void *data, size_t *dst_size_t)
{
	/* warning, possible data loss */
	*dst_size_t = *((size_t *) data);
	return 0;
};

int retrace_int_get_size(const void *data,
	const struct DataType *data_type,
	size_t *dst_size_t)
{
	*dst_size_t = sizeof(int);
	return 0;
}

size_t retrace_uint_to_sz(const void *data,
	const struct DataType *data_type,
	char *str)
{
	return retrace_real_impls.sprintf(str, "%u",
		*((const unsigned int *) data));
}

size_t retrace_uint_get_sz_size(const void *data,
	const struct DataType *data_type)
{
	return retrace_real_impls.snprintf(NULL,
		0,
		"%u",
		*((const unsigned int *) data));
}

int retrace_uint_to_size_t(const void *data, size_t *dst_size_t)
{
	/* warning, possible data loss */
	*dst_size_t = (size_t) *((unsigned int *) data);
	return 0;
};

int retrace_uint_get_size(const void *data,
	const struct DataType *data_type,
	size_t *dst_size_t)
{
	*dst_size_t = sizeof(unsigned int);
	return 0;
}

size_t retrace_int16_to_sz(const void *data,
	const struct DataType *data_type,
	char *str)
{
	return retrace_real_impls.sprintf(str, "%" PRId16,
		*((const int16_t *) data));
}

size_t retrace_int16_get_sz_size(const void *data,
	const struct DataType *data_type)
{
	return retrace_real_impls.snprintf(NULL,
		0,
		"%" PRId16,
		*((const int16_t *) data));
}

int retrace_int16_to_size_t(const void *data, size_t *dst_size_t)
{
	/* warning, possible data loss */
	*dst_size_t = (size_t) *((int16_t *) data);
	return 0;
};

int retrace_int16_get_size(const void *data,
	const struct DataType *data_type,
	size_t *dst_size_t)
{
	*dst_size_t = sizeof(int16_t);
	return 0;
}

size_t retrace_longint_to_sz(const void *data,
	const struct DataType *data_type,
	char *str)
{
	return retrace_real_impls.sprintf(str, "%ld",
		*((const long *) data));
}

size_t retrace_longint_get_sz_size(const void *data,
	const struct DataType *data_type)
{
	return retrace_real_impls.snprintf(NULL,
		0,
		"%ld",
		*((const long *) data));
}

int retrace_longint_to_size_t(const void *data, size_t *dst_size_t)
{
	/* warning, possible data loss */
	*dst_size_t = (size_t) *((long *) data);
	return 0;
};

int retrace_longint_get_size(const void *data,
	const struct DataType *data_type,
	size_t *dst_size_t)
{
	*dst_size_t = sizeof(long);
	return 0;
}

size_t retrace_ulong_to_sz(const void *data,
	const struct DataType *data_type,
	char *str)
{
	return retrace_real_impls.sprintf(str, "%lu",
		*((const unsigned long *) data));
}

size_t retrace_ulong_get_sz_size(const void *data,
	const struct DataType *data_type)
{
	return retrace_real_impls.snprintf(NULL,
		0,
		"%lu",
		*((const unsigned long *) data));
}

int retrace_ulong_to_size_t(const void *data, size_t *dst_size_t)
{
	/* warning, possible data loss */
	*dst_size_t = (size_t) *((unsigned long *) data);
	return 0;
};

int retrace_ulong_get_size(const void *data,
	const struct DataType *data_type,
	size_t *dst_size_t)
{
	*dst_size_t = sizeof(unsigned long);
	return 0;
}

size_t retrace_longlong_to_sz(const void *data,
	const struct DataType *data_type,
	char *str)
{
	return retrace_real_impls.sprintf(str, "%ll",
		*((const long long *) data));
}

size_t retrace_longlong_get_sz_size(const void *data,
	const struct DataType *data_type)
{
	return retrace_real_impls.snprintf(NULL,
		0,
		"%ll",
		*((const long long *) data));
}

int retrace_longlong_to_size_t(const void *data, size_t *dst_size_t)
{
	/* warning, possible data loss */
	*dst_size_t = (size_t) *((long long *) data);
	return 0;
};

int retrace_longlong_get_size(const void *data,
	const struct DataType *data_type,
	size_t *dst_size_t)
{
	*dst_size_t = sizeof(long long);
	return 0;
}

size_t retrace_sz_to_sz(const void *data,
	const struct DataType *data_type,
	char *str)
{
	if (data && str) {
		retrace_real_impls.strcpy(str, (const char *) data);
		return retrace_real_impls.strlen((const char *) data);
	}

	return 0;
}

size_t retrace_sz_get_sz_size(const void *data,
	const struct DataType *data_type)
{
	return retrace_real_impls.strlen((const char *) data);
}

int retrace_sz_to_size_t(const void *data, size_t *dst_size_t)
{
	/* cannot convert */
	return -1;
};

int retrace_sz_get_size(const void *data,
	const struct DataType *data_type,
	size_t *dst_size_t)
{
	if (data && dst_size_t)
		*dst_size_t = retrace_real_impls.strlen((const char *) data) + 1;
	return 0;
}

size_t retrace_char_to_sz(const void *data,
	const struct DataType *data_type,
	char *str)
{
	return retrace_real_impls.sprintf(str,
			"0x%02x",
			*((const char *) data));
}

size_t retrace_char_get_sz_size(const void *data,
	const struct DataType *data_type)
{
	return retrace_real_impls.snprintf(NULL,
		0,
		"0x%02x",
		*((const char *) data));
}

int retrace_char_to_size_t(const void *data, size_t *dst_size_t)
{
	/* cannot convert */
	return -1;
};

int retrace_char_get_size(const void *data,
	const struct DataType *data_type,
	size_t *dst_size_t)
{
	*dst_size_t = sizeof(char);
	return 0;
}

size_t retrace_size_t_to_sz(const void *data,
	const struct DataType *data_type,
	char *str)
{
	return retrace_real_impls.sprintf(str,
		"%zu",
		*((const size_t *) data));
}

size_t retrace_size_t_get_sz_size(const void *data,
	const struct DataType *data_type)
{
	return retrace_real_impls.snprintf(NULL,
		0,
		"%zu",
		*((const size_t *) data));
}


int retrace_size_t_to_size_t(const void *data, size_t *dst_size_t)
{
	*dst_size_t = *((size_t *) data);
	return 0;
};

int retrace_size_t_get_size(const void *data,
	const struct DataType *data_type,
	size_t *dst_size_t)
{
	*dst_size_t = sizeof(size_t);
	return 0;
}


size_t retrace_ptr_to_sz(const void *data,
	const struct DataType *data_type,
	char *str)
{
	return retrace_real_impls.sprintf(str, "%p", *((void **) data));
}


size_t retrace_ptr_get_sz_size(const void *data,
	const struct DataType *data_type)
{
	return retrace_real_impls.snprintf(NULL, 0, "%p", *((void **) data));
}


int retrace_ptr_to_size_t(const void *data, size_t *dst_size_t)
{
	/* cannot convert */
	return -1;
};

int retrace_ptr_get_size(const void *data,
	const struct DataType *data_type,
	size_t *dst_size_t)
{
	*dst_size_t = sizeof(void *);
	return 0;
}

retrace_datatype_define_prototypes(basic) = {
	{
		.name = "int",
		.struct_members[0] = {.name = ""},
		.pa_basic_type = PBT_INT,
		.pa_flag = PFM_UNK,
		.to_sz = retrace_int_to_sz,
		.get_sz_size = retrace_int_get_sz_size,
		.to_size_t = retrace_int_to_size_t,
		.get_size = retrace_int_get_size
	},
	{
		.name = "unsigned int",
		.struct_members[0] = {.name = ""},
		.to_sz = retrace_uint_to_sz,
		.get_sz_size = retrace_uint_get_sz_size,
		.to_size_t = retrace_uint_to_size_t,
		.get_size = retrace_uint_get_size
	},
	{
		.name = "int16_t",
		.struct_members[0] = {.name = ""},
		.pa_basic_type = PBT_INT,
		.pa_flag = PFM_SHORT,
		.to_sz = retrace_int16_to_sz,
		.get_sz_size = retrace_int16_get_sz_size,
		.to_size_t = retrace_int16_to_size_t,
		.get_size = retrace_int16_get_size
	},
	{
		.name = "long",
		.struct_members[0] = {.name = ""},
		.pa_basic_type = PBT_INT,
		.pa_flag = PFM_LONG,
		.to_sz = retrace_longint_to_sz,
		.get_sz_size = retrace_longint_get_sz_size,
		.to_size_t = retrace_longint_to_size_t,
		.get_size = retrace_longint_get_size
	},
	{
		.name = "unsigned long",
		.struct_members[0] = {.name = ""},
		.to_sz = retrace_ulong_to_sz,
		.get_sz_size = retrace_ulong_get_sz_size,
		.to_size_t = retrace_ulong_to_size_t,
		.get_size = retrace_ulong_get_size
	},
	{
		.name = "long long",
		.struct_members[0] = {.name = ""},
		.pa_basic_type = PBT_INT,
		.pa_flag = PFM_LONG_LONG,
		.to_sz = retrace_longlong_to_sz,
		.get_sz_size = retrace_longlong_get_sz_size,
		.to_size_t = retrace_longlong_to_size_t,
		.get_size = retrace_longlong_get_size
	},
	{
		.name = "sz",
		.struct_members[0] = {.name = ""},
		.to_sz = retrace_sz_to_sz,
		.get_sz_size = retrace_sz_get_sz_size,
		.to_size_t = retrace_sz_to_size_t,
		.get_size = retrace_sz_get_size
	},
	{
		.name = "size_t",
		.struct_members[0] = {.name = ""},
		.to_sz = retrace_size_t_to_sz,
		.get_sz_size = retrace_size_t_get_sz_size,
		.to_size_t = retrace_size_t_to_size_t,
		.get_size = retrace_size_t_get_size
	},
	{
		.name = "char",
		.struct_members[0] = {.name = ""},
		.pa_basic_type = PBT_CHAR,
		.pa_flag = PFM_UNK,
		.to_sz = retrace_char_to_sz,
		.get_sz_size = retrace_char_get_sz_size,
		.to_size_t = retrace_char_to_size_t,
		.get_size = retrace_char_get_size
	},
	{
		.name = "ptr",
		.struct_members[0] = {.name = ""},
		.pa_basic_type = PBT_POINTER,
		.pa_flag = PFM_UNK,
		.to_sz = retrace_ptr_to_sz,
		.get_sz_size = retrace_ptr_get_sz_size,
		.to_size_t = retrace_ptr_to_size_t,
		.get_size = retrace_ptr_get_size
	},
	{
		.name = "intptr_t",
		.struct_members[0] = {.name = ""},
		.to_sz = retrace_ptr_to_sz,
		.get_sz_size = retrace_ptr_get_sz_size,
		.to_size_t = retrace_ptr_to_size_t,
		.get_size = retrace_ptr_get_size
	}
};
