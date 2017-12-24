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
#include <stdio.h>
#include <stdlib.h>

#include "engine.h"
#include "arch_spec.h"
#include "real_impls.h"
#include "data_types.h"
#include "parson.h"
#include "actions.h"

/* TODO change to logger */
#define log_err(fmt, ...) printf("[ERROR] " fmt "\n", ##__VA_ARGS__)
#define log_info(fmt, ...) printf("[INFO] " fmt "\n", ##__VA_ARGS__)

#define ARR_MAX_COUNT 32

static const struct DataType *get_param_type(
	const char *param_type_name)
{
	/* TODO: improve speed */

	const struct DataType *p;

	p = retrace_data_types;
	while (retrace_real_impls.strlen(p->name) &&
			retrace_real_impls.strcmp(p->name, param_type_name))
		p++;

	if (!retrace_real_impls.strlen(p->name))
		return NULL;

	return p;
}

static int get_param(const struct ThreadContext *t_ctx,
		const char *name,
		const struct DataType **data_type,
		const struct ParamMeta **meta,
		const void **data)
{
	/* TODO: improve speed */
	const struct DataType *param_data_type;
	int i;

	i = 0;
	/* calc index */
	while (retrace_real_impls.strlen(t_ctx->prototype->params[i].name) &&
			retrace_real_impls.strcmp(name,
					t_ctx->prototype->params[i].name)) {

		i++;
	}

	if (!retrace_real_impls.strlen(t_ctx->prototype->params[i].name))
		return -1;

	/* find data type */
	param_data_type = retrace_data_types;
	while (retrace_real_impls.strlen(param_data_type->name) &&
		retrace_real_impls.strcmp(t_ctx->prototype->params[i].type_name,
			param_data_type->name)) {
		param_data_type++;
	}

	if (!retrace_real_impls.strlen(param_data_type->name))
		return -2;

	if (data_type != NULL)
		*data_type = param_data_type;

	if (meta != NULL)
		*meta = &t_ctx->prototype->params[i];

	if (data != NULL)
		*data = t_ctx->params[i];

	return 0;
}

static enum InterceptResults ia_log_params_json(struct ThreadContext *t_ctx)
{
	/*
	 * serialization to JSON format.
	 * Shall be the same for struct data type
	 * under presumption that both serializations define a complex data,
	 * this allows important unification between the serialization formats
	 *
	 * example
	 *
	 *	 * func(int_param,
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

	const struct ParamMeta *meta;
	const struct ParamMeta *count_meta;

	const struct DataType *data_type;
	const struct DataType *count_data_type;
	const struct DataType *ref_data_type;

	const void *data;
	const void *count_data;
	const void *ref_data;

	int ret;
	char *serialized_string;
	JSON_Value *root_value;
	JSON_Object *root_object;
	JSON_Value *arr_val;
	size_t sz_size;
	size_t arr_size;
	size_t ref_data_size;
	char *sz;
	/* one for * and one for '\0' */
	char deref_sz[MAXLEN_PARAM_NAME + 2];

	meta = t_ctx->prototype->params;
	root_value = json_value_init_object();
	root_object = json_value_get_object(root_value);

	while (retrace_real_impls.strlen(meta->name)) {
		ret = get_param(t_ctx, meta->name, &data_type, NULL, &data);
		if (ret) {
			log_err("Could not get param info for %s, error %d",
				meta->name, ret);
				break;
		}

		sz_size = data_type->get_sz_size(data, data_type);
		sz = (char *) retrace_real_impls.malloc(sz_size + 1);
		data_type->to_sz(data, data_type, sz);
		json_object_set_string(root_object, meta->name, sz);
		retrace_real_impls.free(sz);

		if (meta->modifiers & CDM_POINTER) {
			/* in case of pointers we want to dereference one level
			 * and handle arrays accordingly.
			 * FIXME: dereferencing may lead to loop if pointers
			 * reference each other
			 */

			/* TODO: Maybe should cast via data_type? */
			ref_data = *((char **) data);
			ref_data_type = get_param_type(meta->ref_type_name);
			if (ref_data_type == NULL) {
				log_err("get_param_type() failed for %s",
					meta->ref_type_name);
				break;
			}

			/* pointer to ARRAY */
			if (meta->modifiers & CDM_ARRAY) {

				ret = get_param(t_ctx, meta->array_cnt_param,
					&count_data_type,
					&count_meta,
					&count_data);

				if (ret) {
					log_err(
						"Could not get param info for %s, error %d",
						meta->array_cnt_param,
						ret);
					break;
				}

				ret = count_data_type->to_size_t(count_data,
					&arr_size);

				if (ret) {
					log_err("to_size_t() failed for %s, error %d",
						meta->array_cnt_param, ret);
					break;
				}
			} else
				arr_size = 1;

			if (arr_size > ARR_MAX_COUNT)
				arr_size = ARR_MAX_COUNT;

			arr_val = json_value_init_array();

			while (arr_size) {

				sz_size = ref_data_type->get_sz_size(ref_data,
						ref_data_type);

				sz = (char *) retrace_real_impls.malloc(
						sz_size + 1);

				ref_data_type->to_sz(ref_data,
						ref_data_type, sz);

				json_array_append_string(json_array(arr_val),
						sz);

				retrace_real_impls.free(sz);

				/* advance to the next element */
				ret = ref_data_type->get_size(ref_data,
						ref_data_type,
						&ref_data_size);

				if (ret) {

					log_err(
						"get_size() failed for type %s, error %d",
						ref_data_type->name,
						ret);

					break;
				}

				ref_data += ref_data_size;
				arr_size--;
			}

			retrace_real_impls.snprintf(deref_sz,
				sizeof(deref_sz),
				"*%s", meta->name);

			json_object_set_value(root_object, deref_sz, arr_val);
		}

		/* process next member */
		meta++;
	}

	serialized_string = json_serialize_to_string_pretty(root_value);

	/* finally */
	log_info("%s", serialized_string);

	json_free_serialized_string(serialized_string);
	json_value_free(root_value);

	return IR_NEXT;
}

static enum InterceptResults ia_call_real(struct ThreadContext *t_ctx)
{
	log_info("calling real at 0x%lx for %s...",
			(long int) t_ctx->real_impl,
			t_ctx->prototype->name);

	t_ctx->ret_val = retrace_as_call_real_ret(t_ctx->real_impl,
		t_ctx->prototype->params,
		t_ctx->params);

	return IR_NEXT;
}

static enum InterceptResults ia_do_nothing(struct ThreadContext *t_ctx)
{
	return IR_NEXT;
}

enum InterceptResults(*retrace_intercept_actions[])
	(struct ThreadContext *t_ctx) = {
		[IA_NA] = ia_do_nothing,
		[IA_LOG_PARAMS] = ia_log_params_json,
		[IA_LOG_PARAMS_JSON] = ia_log_params_json,
		[IA_CALL_REAL] = ia_call_real
};
