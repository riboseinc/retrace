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

#ifndef SRC_RETRACE_V2_FUNCS_H_
#define SRC_RETRACE_V2_FUNCS_H_

#include "data_types.h"

#define MAXLEN_FUNC_NAME 64
#define MAXLEN_PARAM_NAME 32
#define MAXCOUNT_PARAMS 16

enum CallingConventions {
	CC_INVALID,
	CC_SYSTEM_V,
	CC_MICROSOFT,
	CC_CNT
};

enum ParamDirections {
	PDIR_IN,
	PDIR_OUT,
	PDIR_INOUT,
	PDIR_CNT
};

/*
 * As defined by gcc format attribute
 */
enum FormatArchTypes {
	FAT_NOVARARGS,
	FAT_PRINTF,
	FAT_SCANF,
	FAT_STRFTIME,
	/* TODO: Add more */
	FAT_CNT
};

struct ParamMeta {
	char name[MAXLEN_PARAM_NAME + 1];
	char type_name[MAXLEN_DATATYPE_NAME + 1];
	enum CDataModifiers modifiers;
	enum ParamDirections direction;

	/* in case modifiers & CDM_POINTER, this is the referenced type
	 */
	char ref_type_name[MAXLEN_DATATYPE_NAME + 1];

	/* in case modifiers & (CDM_POINTER | CDM_ARRAY), this param
	 * holds the number of elements
	 */
	char array_cnt_param[MAXLEN_PARAM_NAME + 1];
};

struct FuncPrototype {
	/* function name, same as exported symbol */
	char name[MAXLEN_FUNC_NAME + 1];

	/* calling convention, TODO: consider to remove */
	enum CallingConventions conv;

	/* static params, i.e. not varargs */
	struct ParamMeta params[MAXCOUNT_PARAMS];

	/* num of static params */
	int params_cnt;

	/* function return type name */
	char type_name[MAXLEN_DATATYPE_NAME + 1];

	/* gcc-style format support */
	enum FormatArchTypes fmt;

	/* index of param holding fmt template */
	int fmt_param_idx;
};

const struct FuncPrototype *retrace_func_get(const char *func_name);
int retrace_funcs_init(void);

/* aligned 1 is used because i group all FuncPrototype in the same
 * section, creating an array
 */
#define retrace_func_define_prototypes(lib_name) \
	retrace_as_define_var_in_sec(const struct FuncPrototype,\
		retrace_proto_##lib_name[], \
			"__DATA", "__retrace_funcs")__attribute__((aligned(1)))

#endif /* SRC_RETRACE_V2_FUNCS_H_ */
