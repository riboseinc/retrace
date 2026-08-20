/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * ntdll path decoders (TODO.windows/06). The ntdll file API does
 * not take a plain string: the path is OBJECT_ATTRIBUTES ->
 * ObjectName -> UNICODE_STRING {Length, Buffer (UTF-16)}. Two
 * data types expose it to the engine:
 *
 *   "ntoa" -- pointer to OBJECT_ATTRIBUTES: decodes to the
 *             UTF-8 path. NtCreateFile/NtOpenFile/
 *             NtQueryAttributesFile param.
 *   "ntus" -- pointer to UNICODE_STRING: decodes to the UTF-8
 *             string. LdrLoadDll's module-name param.
 *
 * MECE: the decoder lives with the datatype registry (where
 * every other to_sz lives); the prototype tables merely NAME
 * these types, exactly like "sz".
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "data_types.h"

#include <stdint.h>
#include <stddef.h>

struct nt_unicode_string {
	uint16_t length;        /* bytes, not chars */
	uint16_t maximum_length;
	uint32_t _pad;
	uint16_t *buffer;       /* UTF-16, not NUL-terminated */
};

/*
 * OBJECT_ATTRIBUTES (x64 layout): ULONG Length; (pad) HANDLE
 * RootDirectory; PUNICODE_STRING ObjectName; ULONG Attributes;
 * ... ObjectName sits at offset 16.
 */
#define NT_OA_OBJECT_NAME_OFF 16

static const struct nt_unicode_string *oa_object_name(const void *oa)
{
	const void *const *us;

	if (oa == NULL)
		return NULL;
	us = (const void *const *)((const unsigned char *)oa +
				   NT_OA_OBJECT_NAME_OFF);
	return (const struct nt_unicode_string *)*us;
}

/*
 * Decode one UNICODE_STRING into UTF-8. Returns the number of
 * bytes written including the NUL, or 0 on failure.
 */
static size_t us_to_utf8(const struct nt_unicode_string *us,
			 char *str, size_t str_size)
{
	int n;

	if (us == NULL || us->buffer == NULL || us->length == 0) {
		if (str_size > 0)
			str[0] = '\0';
		return 1;
	}

	n = WideCharToMultiByte(CP_UTF8, 0,
		(LPCWSTR)us->buffer, us->length / 2,
		str, (int)str_size, NULL, NULL);
	if (n <= 0) {
		if (str_size > 0)
			str[0] = '\0';
		return 1;
	}
	str[n] = '\0';
	return (size_t)n + 1;
}

/* worst case UTF-8 growth: 3 bytes per UTF-16 unit, + NUL */
static size_t us_sz_size(const struct nt_unicode_string *us)
{
	if (us == NULL || us->buffer == NULL)
		return 2;
	return (size_t)us->length * 3 / 2 + 1;
}

static size_t ntoa_get_sz_size(const void *data,
			       const struct DataType *data_type)
{
	(void)data_type;
	return us_sz_size(oa_object_name(data));
}

static size_t ntoa_to_sz(const void *data, const struct DataType *data_type,
			 char *str)
{
	(void)data_type;
	return us_to_utf8(oa_object_name(data), str, us_sz_size(
		oa_object_name(data)));
}

static int ntoa_get_size(const void *data, const struct DataType *data_type,
			 size_t *dst_size_t)
{
	(void)data;
	(void)data_type;
	*dst_size_t = sizeof(void *);
	return 0;
}

static int ntoa_to_size_t(const void *data, size_t *dst_size_t)
{
	(void)data;
	*dst_size_t = sizeof(void *);
	return 0;
}

static size_t ntus_get_sz_size(const void *data,
			       const struct DataType *data_type)
{
	(void)data_type;
	return us_sz_size((const struct nt_unicode_string *)data);
}

static size_t ntus_to_sz(const void *data, const struct DataType *data_type,
			 char *str)
{
	(void)data_type;
	return us_to_utf8((const struct nt_unicode_string *)data, str,
		us_sz_size((const struct nt_unicode_string *)data));
}

static int ntus_get_size(const void *data, const struct DataType *data_type,
			 size_t *dst_size_t)
{
	(void)data;
	(void)data_type;
	*dst_size_t = sizeof(void *);
	return 0;
}

static int ntus_to_size_t(const void *data, size_t *dst_size_t)
{
	(void)data;
	*dst_size_t = sizeof(void *);
	return 0;
}

retrace_datatype_define_prototypes(nt_decode) = {
	{
		.name = "ntoa",
		.struct_members[0] = {.name = ""},
		.to_sz = ntoa_to_sz,
		.get_sz_size = ntoa_get_sz_size,
		.to_size_t = ntoa_to_size_t,
		.get_size = ntoa_get_size
	},
	{
		.name = "ntus",
		.struct_members[0] = {.name = ""},
		.to_sz = ntus_to_sz,
		.get_sz_size = ntus_get_sz_size,
		.to_size_t = ntus_to_size_t,
		.get_size = ntus_get_size
	}
};
