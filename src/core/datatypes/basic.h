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

#ifndef SRC_RETRACE_V2_DATATYPES_BASIC_H_
#define SRC_RETRACE_V2_DATATYPES_BASIC_H_

size_t retrace_struct_to_sz(const void *data,
		const struct DataType *data_type,
		char *str);

size_t retrace_struct_get_sz_size(const void *data,
		const struct DataType *data_type);

int retrace_struct_to_size_t(const void *data, size_t *dst_size_t);

int retrace_struct_get_size(const void *data,
	const struct DataType *data_type,
	size_t *dst_size_t);

size_t retrace_int_to_sz(const void *data,
	const struct DataType *data_type,
	char *str);

size_t retrace_int_get_sz_size(const void *data,
	const struct DataType *data_type);

int retrace_int_to_size_t(const void *data, size_t *dst_size_t);

int retrace_int_get_size(const void *data,
	const struct DataType *data_type,
	size_t *dst_size_t);

size_t retrace_uint_to_sz(const void *data,
	const struct DataType *data_type,
	char *str);

size_t retrace_uint_get_sz_size(const void *data,
		const struct DataType *data_type);

int retrace_uint_to_size_t(const void *data, size_t *dst_size_t);

int retrace_uint_get_size(const void *data,
	const struct DataType *data_type,
	size_t *dst_size_t);

size_t retrace_int16_to_sz(const void *data,
	const struct DataType *data_type,
	char *str);

size_t retrace_int16_get_sz_size(const void *data,
	const struct DataType *data_type);

int retrace_int16_to_size_t(const void *data, size_t *dst_size_t);

int retrace_int16_get_size(const void *data,
	const struct DataType *data_type,
	size_t *dst_size_t);

size_t retrace_longint_to_sz(const void *data,
	const struct DataType *data_type,
	char *str);

size_t retrace_longint_get_sz_size(const void *data,
	const struct DataType *data_type);

int retrace_longint_to_size_t(const void *data, size_t *dst_size_t);

int retrace_longint_get_size(const void *data,
	const struct DataType *data_type,
	size_t *dst_size_t);

size_t retrace_ulong_to_sz(const void *data,
	const struct DataType *data_type,
	char *str);

size_t retrace_ulong_get_sz_size(const void *data,
	const struct DataType *data_type);

int retrace_ulong_to_size_t(const void *data, size_t *dst_size_t);

int retrace_ulong_get_size(const void *data,
	const struct DataType *data_type,
	size_t *dst_size_t);

size_t retrace_sz_to_sz(const void *data,
	const struct DataType *data_type,
	char *str);

size_t retrace_sz_get_sz_size(const void *data,
	const struct DataType *data_type);

int retrace_sz_to_size_t(const void *data, size_t *dst_size_t);

int retrace_sz_get_size(const void *data,
	const struct DataType *data_type,
	size_t *dst_size_t);

size_t retrace_char_to_sz(const void *data,
	const struct DataType *data_type,
	char *str);

size_t retrace_char_get_sz_size(const void *data,
	const struct DataType *data_type);

int retrace_char_to_size_t(const void *data, size_t *dst_size_t);

int retrace_char_get_size(const void *data,
	const struct DataType *data_type,
	size_t *dst_size_t);

size_t retrace_size_t_to_sz(const void *data,
	const struct DataType *data_type,
	char *str);

size_t retrace_size_t_get_sz_size(const void *data,
	const struct DataType *data_type);

int retrace_size_t_to_size_t(const void *data, size_t *dst_size_t);

int retrace_size_t_get_size(const void *data,
	const struct DataType *data_type,
	size_t *dst_size_t);

size_t retrace_ptr_to_sz(const void *data,
	const struct DataType *data_type,
	char *str);

size_t retrace_ptr_get_sz_size(const void *data,
	const struct DataType *data_type);

int retrace_ptr_to_size_t(const void *data, size_t *dst_size_t);

int retrace_ptr_get_size(const void *data,
	const struct DataType *data_type,
	size_t *dst_size_t);

#endif /* SRC_RETRACE_V2_DATATYPES_BASIC_H_ */
