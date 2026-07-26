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

#include "actions.h"
#include "logger.h"
#include "real_impls.h"
#include "data_types.h"

#define ARR_MAX_COUNT 64

/*
 * action_params:
 * omit_params [array] - skip logging of params in this list
 */
static int ia_log_params
	(struct ThreadContext *t_ctx,
		const JSON_Object *action_params)
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

	const struct FuncParam *param;
	const struct FuncParam *arr_cnt_param;

	const struct DataType *ref_data_type;


	int ret;
	size_t i;
	int param_idx;
	int cnt_param_idx;
	JSON_Value *root_value;
	JSON_Object *root_object;
	JSON_Value *arr_val;
	JSON_Array *omit_params;
	size_t sz_size;
	size_t arr_size;
	size_t ref_data_size;
	const void *ref_data;
	char *sz;
	/* one for * and one for '\0' */
	char deref_sz[MAXLEN_PARAM_NAME + 2];

	root_value = json_value_init_object();
	root_object = json_value_get_object(root_value);

	omit_params = NULL;
	if (action_params != NULL)
		omit_params = json_object_get_array(action_params, "omit_params");

	for (param_idx = 0; param_idx != t_ctx->params_cnt; param_idx++) {
		param = &t_ctx->params[param_idx];

		/* check if this param is in omit_params array */
		if (omit_params != NULL) {
			for (i = 0; i < json_array_get_count(omit_params); i++) {
				if (!retrace_real_impls.strcmp(
						param->param_meta.name,
						json_array_get_string(omit_params, i))) {

					log_info("omitting param '%s'",
						param->param_meta.name);

					goto next_param;


				}

			}

		}

		sz_size = param->data_type->get_sz_size(
			(const void *) &param->val,
			param->data_type);

		sz = (char *) retrace_real_impls.malloc(sz_size + 1);

		param->data_type->to_sz(
			(const void *) &param->val,
			param->data_type,
			sz);

		json_object_set_string(root_object,
			param->param_meta.name,
			sz);

		retrace_real_impls.free(sz);

		if (param->param_meta.modifiers & CDM_POINTER) {
			/* in case of pointers we want to dereference one level
			 * and handle arrays accordingly.
			 * FIXME: dereferencing may lead to loop if pointers
			 * reference each other
			 */

			/* TODO: Maybe should cast via data_type? */

			ref_data = (void *) param->val;
			ref_data_type =
				retrace_datatype_get(param->param_meta.ref_type_name);

			if (ref_data_type == NULL) {
				log_err("get_param_type() failed for %s",
					param->param_meta.ref_type_name);
				break;
			}

			/* pointer to ARRAY */
			if (param->param_meta.modifiers & CDM_ARRAY) {
				for (cnt_param_idx = 0;
						cnt_param_idx != t_ctx->params_cnt;
						cnt_param_idx++) {

					if (!retrace_real_impls.strcmp(
						t_ctx->params[cnt_param_idx].param_meta.name,
						param->param_meta.array_cnt_param))
						break;

				}

				if (cnt_param_idx == t_ctx->params_cnt) {
					log_err("wrong array_cnt_param for param '%s'",
						param->param_meta.name);

					break;
				}

				arr_cnt_param = &t_ctx->params[cnt_param_idx];

				ret = arr_cnt_param->data_type->to_size_t(
					(const void *) &arr_cnt_param->val,
					&arr_size);

				if (ret) {
					log_err("to_size_t() failed for %s, error %d",
						arr_cnt_param->param_meta.name, ret);
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
				"*%s", param->param_meta.name);

			json_object_set_value(root_object, deref_sz, arr_val);
		}

next_param:;
		/* process next member */
	}

	//serialized_string = json_serialize_to_string_pretty(root_value);

	/* finally */
	//log_info("%s", serialized_string);
	retrace_logger_log_json(ACTIONS, SEVERITY_INFO, root_value);

	//json_free_serialized_string(serialized_string);
	//json_value_free(root_value);

	/* 0 indicates successful processing */
	return 0;
}

static int ia_modify_in_param_str
	(struct ThreadContext *t_ctx,
		const JSON_Object *action_params)
{
	const char *param_name;
	const char *match_str;
	const char *new_str;
	const struct FuncParam *param;
	int param_idx;

	if (action_params == NULL) {
		log_err("action_params must exists for modify_in_param_str");
		return -1;
	}

	param_name = json_object_get_string(action_params,
			"param_name");

	if (param_name == NULL) {
		log_err("param_name must exist in action_params "
				"for modify_in_param_str");
		return -1;
	}

	/* check that param exists in parsed params */
	for (param_idx = 0; param_idx != t_ctx->params_cnt; param_idx++) {
		if (!retrace_real_impls.strcmp(
			t_ctx->params[param_idx].param_meta.name,
			param_name))
			break;
	}

	if (param_idx == t_ctx->params_cnt) {
		log_err("param '%s', is not defined for func '%s'",
			param_name, t_ctx->prototype->name);

		return -1;
	}

	param = &t_ctx->params[param_idx];

	/* check param in or inout */
	if (param->param_meta.direction != PDIR_IN) {
		log_err("param '%s' is not an input param", param_name);

		return -1;
	}

	/* check param is a pointer to C string */
	if (!(param->param_meta.modifiers & CDM_POINTER) ||
		(retrace_real_impls.strcmp(
			param->param_meta.ref_type_name, "sz"))) {
		log_err("param '%s' is not a pointer to string", param_name);

		return -1;
	}

	new_str = json_object_get_string(action_params,
			"new_str");

	if (new_str == NULL) {
		log_err("new_str must exist in action_params "
				"for modify_in_param_str");
		return -1;
	}

	match_str = json_object_get_string(action_params,
			"match_str");

	if (match_str != NULL) {
		log_info("match requested for param '%s', val='%s'",
			param_name, match_str);

		/* Note avoiding the use of data_types funcs
		 * since we know its a pointer to C str
		 */
		if (retrace_real_impls.strcmp(match_str,
			(char *) param->val)) {

			log_info("no match for param '%s'", param_name);

			/* no match, do nothing */
			return 0;
		}

		log_info("match for param '%s'", param_name);

	} else
		log_info("match not requested for param '%s'",
			param_name);

	/* allocate new str */

	/* override if parameters is already modified  */
	if (t_ctx->params[param_idx].free_val) {
		retrace_real_impls.free((void *) t_ctx->params[param_idx].val);
		t_ctx->params[param_idx].free_val = 0;
	}

	t_ctx->params[param_idx].val =
		(long) retrace_real_impls.malloc(
			retrace_real_impls.strlen(new_str) + 1);
	t_ctx->params[param_idx].free_val = 1;

	retrace_real_impls.strcpy(
		(char *) t_ctx->params[param_idx].val,
		new_str);

	/* mark allocation for clean up */
	t_ctx->params[param_idx].free_val = 1;

	log_info("param '%s' set to '%s'",
		param_name, new_str);

	/* 0 indicates successful processing */
	return 0;
}

static int ia_modify_in_param_arr
	(struct ThreadContext *t_ctx,
		const JSON_Object *action_params)
{
	const char *param_name;
	const JSON_Array *match_arr;
	const JSON_Array *new_arr;
	struct FuncParam *param;
	struct FuncParam *cnt_param;
	int param_idx;
	int cnt_param_idx;
	char *match_str;
	size_t match_idx;
	char *param_arr;
	size_t new_arr_idx;
	char *new_str;

	if (action_params == NULL) {
		log_err("action_params must exists for modify_in_param_arr");
		return -1;
	}

	param_name = json_object_get_string(action_params,
			"param_name");

	if (param_name == NULL) {
		log_err("param_name must exist in action_params "
				"for modify_in_param_arr");
		return -1;
	}

	/* check that param exists in prototype */
	for (param_idx = 0; param_idx != t_ctx->params_cnt; param_idx++) {
		if (!retrace_real_impls.strcmp(
			t_ctx->params[param_idx].param_meta.name,
			param_name))
			break;
	}

	if (param_idx == t_ctx->params_cnt) {
		log_err("param '%s', is not defined for func '%s'",
			param_name, t_ctx->prototype->name);

		return -1;
	}

	param = &t_ctx->params[param_idx];

	/* check param in or inout */
	if (param->param_meta.direction != PDIR_IN) {
		log_err("param '%s' is not an input param", param_name);

		return -1;
	}

	/* check param is a pointer */
	if (!(param->param_meta.modifiers & CDM_POINTER)) {
		log_err("param '%s' is not a pointer", param_name);

		return -1;
	}

	new_arr = json_object_get_array(action_params, "new_arr");
	if (new_arr == NULL) {
		log_err("new_arr must exist in action_params "
				"for modify_in_param_arr");
		return -1;
	}

	if (!json_array_get_count(new_arr)) {
		log_err("new_arr cannot be empty for modify_in_param_arr");
		return -1;
	}

	/* get match bytes */
	match_arr = json_object_get_array(action_params, "match_arr");

	if (match_arr != NULL) {

		match_str = json_serialize_to_string_pretty(
			json_array_get_wrapping_value(match_arr));

		log_info("match requested for param '%s', val='%s'",
			param_name, match_str);

		json_free_serialized_string(match_str);

		/* match loop
		 * TODO Consider to use size param as well
		 */
		for (match_idx = 0,
			param_arr = (char *) param->val;
			match_idx != json_array_get_count(match_arr);
			match_idx++) {

			if (param_arr[match_idx] !=
				(char) json_array_get_number(match_arr, match_idx)) {
				log_info("no match for param '%s'", param_name);
				/* no match, do nothing */
				return 0;
			}
		}

		log_info("match for param '%s'", param_name);
	} else
		log_info("match not requested for param '%s'",
			param_name);

	/* allocate and override */
	if (param->free_val) {
		retrace_real_impls.free((void *) param->val);
		param->free_val = 0;
	}

	param->val =
		(long) retrace_real_impls.malloc(
			json_array_get_count(new_arr));
	param->free_val = 1;

	for (new_arr_idx = 0;
		new_arr_idx != json_array_get_count(new_arr);
		new_arr_idx++) {
		*((char *) (param->val + new_arr_idx)) =
			(char) json_array_get_number(new_arr, new_arr_idx);
	}

	/* update the size param if exists */
	if (retrace_real_impls.strlen(param->param_meta.array_cnt_param)) {
		for (cnt_param_idx = 0;
				cnt_param_idx != t_ctx->params_cnt;
				cnt_param_idx++) {

			if (!retrace_real_impls.strcmp(
				t_ctx->params[cnt_param_idx].param_meta.name,
				param->param_meta.array_cnt_param))
				break;

		}

		if (cnt_param_idx == t_ctx->params_cnt) {
			log_err("wrong array_cnt_param for param '%s'",
				param->param_meta.name);

			return -1;
		}

		cnt_param = &t_ctx->params[cnt_param_idx];

		/* not sure size_t or int or use virtual method */
		cnt_param->val = json_array_get_count(new_arr);

		log_info("array_cnt_param '%s' set to '%ld",
				cnt_param->param_meta.name,
				cnt_param->val);
	}

	new_str = json_serialize_to_string_pretty(
		json_array_get_wrapping_value(new_arr));

	log_info("param '%s' set to '%s",
		param_name, new_str);

	json_free_serialized_string(new_str);

	/* 0 indicates successful processing */
	return 0;
}

static int ia_modify_in_param_int
	(struct ThreadContext *t_ctx,
		const JSON_Object *action_params)
{
	const char *param_name;
	double match_int;
	double new_int;
	int param_idx;
	struct FuncParam *param;

	if (action_params == NULL) {
		log_err("action_params must exists for modify_in_param_int");
		return -1;
	}

	param_name = json_object_get_string(action_params,
			"param_name");

	if (param_name == NULL) {
		log_err("param_name must exist in action_params "
				"for modify_in_param_int");
		return -1;
	}

	/* check that param exists in prototype */
	for (param_idx = 0; param_idx != t_ctx->params_cnt; param_idx++) {
		if (!retrace_real_impls.strcmp(
			t_ctx->params[param_idx].param_meta.name,
			param_name))
			break;
	}

	if (param_idx == t_ctx->params_cnt) {
		log_err("param '%s', is not defined for func '%s'",
			param_name, t_ctx->prototype->name);

		return -1;
	}

	param = &t_ctx->params[param_idx];

	/* check param in or inout */
	if (param->param_meta.direction != PDIR_IN) {
		log_err("param '%s' is not an input param", param_name);

		return -1;
	}

	if (!json_object_has_value(action_params, "new_int")) {
		log_err("new_int must exist in action_params "
				"for modify_in_param_int");
		return -1;
	}

	new_int = json_object_get_number(action_params, "new_int");

	if (json_object_has_value(action_params, "match_int")) {

		match_int = json_object_get_number(action_params, "match_int");

		log_info("match requested for param '%s', val='%d",
			param_name, (int) match_int);
		if (((int) param->val) != (int) match_int) {

			log_info("no match for param '%s'", param_name);

			/* no match, do nothing */
			return 0;
		}

		log_info("match for param '%s'", param_name);

	} else
		log_info("match not requested for param '%s'",
			param_name);

	/* direct modification */
	param->val = (long) new_int;

	log_info("param '%s' set to '%d'",
		param_name, (int) new_int);

	/* 0 indicates successful processing */
	return 0;
}

static int ia_call_real
	(struct ThreadContext *t_ctx,
		const JSON_Object *action_params)
{
	(void)(action_params);

	log_dbg("calling real at 0x%lx for %s...",
			(long) t_ctx->real_impl,
			t_ctx->prototype->name);

	t_ctx->ret_val = retrace_as_call_real(t_ctx->real_impl,
		t_ctx->params,
		t_ctx->params_cnt);

	log_dbg("real returned val=0x%lx", t_ctx->ret_val);

	/* 0 indicates successful processing */
	return 0;
}

static int ia_modify_return_value_int
	(struct ThreadContext *t_ctx,
		const JSON_Object *action_params)
{
	double new_int;

	if (action_params == NULL) {
		log_err("action_params must exists for modify_return_value_int");
		return -1;
	}

	if (!json_object_has_value(action_params, "retval_int")) {
		log_err("retval_int must exist in action_params "
				"for modify_return_value_int");
		return -1;
	}

	new_int = json_object_get_number(action_params, "retval_int");

	t_ctx->ret_val = (long) new_int;

	log_info("retval set to '%d'",
		(int) new_int);

	/* 0 indicates successful processing */
	return 0;
}

retrace_actions_define_package(basic) = {
	{
		.name = "log_params",
		.action = ia_log_params
	},
	{
		.name = "call_real",
		.action = ia_call_real
	},
	{
		.name = "modify_in_param_str",
		.action = ia_modify_in_param_str
	},
	{
		.name = "modify_in_param_int",
		.action = ia_modify_in_param_int
	},
	{
		.name = "modify_in_param_arr",
		.action = ia_modify_in_param_arr
	},
	{
		.name = "modify_return_value_int",
		.action = ia_modify_return_value_int
	}
};
