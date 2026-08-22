/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */


/*
 * truss2retrace: FreeBSD truss log -> retrace trace JSON
 * (TODO.trace-profile/14). The FreeBSD kernel layer (truth)
 * normalizes to retrace's entry shape; retrace-profile --kernel
 * (and retrace-correlate --outside) consume the output like any
 * other stream.
 *
 * Capture with:
 *   truss -f -o truss.log ./app
 *
 * Recognized line shape (anything else is skipped):
 *   1234: openat(AT_FDCWD,"/a/b",O_RDONLY,00) = 3 (0x0)
 * The first quoted string of a file syscall becomes the path;
 * the raw tail becomes "detail" (flags feed the classifier).
 */

/*
 * ktrace2retrace: OpenBSD/NetBSD kdump log -> retrace trace
 * JSON (TODO.trace-profile/21). See convert.c for the shapes.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "convert.h"
#include "parson.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parson.h"

/*
 * truss escapes special bytes inside quoted strings. Unescape
 * the common cases into dst (bounded); returns dst length or
 * (size_t)-1 when the input is not a clean quoted string.
 */
static size_t unescape(const char *src, char *dst, size_t dstsz)
{
	size_t i = 1; /* skip opening quote */
	size_t o = 0;

	if (src[0] != '"')
		return (size_t)-1;
	while (src[i] != '\0' && src[i] != '"') {
		char c = src[i];

		if (c == '\\') {
			i++;
			switch (src[i]) {
			case 'n':
				c = '\n';
				break;
			case 't':
				c = '\t';
				break;
			case 'r':
				c = '\r';
				break;
			case '"':
			case '\\':
				c = src[i];
				break;
			default:
				/* octal and other escapes: keep byte */
				c = src[i];
				break;
			}
		}
		if (o + 1 >= dstsz)
			return (size_t)-1;
		dst[o++] = c;
		i++;
	}
	if (src[i] != '"')
		return (size_t)-1;
	dst[o] = '\0';
	/* position after the closing quote */
	return i + 1;
}

/* File syscalls worth recording; anything else on the line is
 * noise for a path profile.
 */
static const char *const g_file_funcs[] = {
	"open", "openat", "openat2", "creat",
	"stat", "lstat", "fstatat", "newfstatat", "statx", "statfs",
	"access", "faccessat", "faccessat2",
	"unlink", "unlinkat", "rename", "renameat", "renameat2",
	"link", "linkat", "symlink", "symlinkat",
	"mkdir", "mkdirat", "rmdir",
	"truncate", "ftruncate",
	"readlink", "readlinkat",
	"execve", "execveat",
	"chown", "lchown", "fchownat", "chmod", "fchmodat",
	NULL
};

static int is_file_func(const char *name, size_t len)
{
	size_t i;

	for (i = 0; g_file_funcs[i] != NULL; i++) {
		if (strlen(g_file_funcs[i]) == len &&
		    strncmp(name, g_file_funcs[i], len) == 0)
			return 1;
	}
	return 0;
}

/*
 * Parse one truss line. Returns 1 when an entry was appended to
 * the output array, 0 when skipped.
 */
static int convert_line(const char *line, JSON_Array *out)
{
	const char *p = line;
	const char *name;
	size_t name_len;
	char path[1024];
	char detail[256];
	size_t used;
	size_t verb_len;
	int is_nami;
	long pid = 0;
	JSON_Value *entry;
	JSON_Object *entry_o;
	JSON_Object *msg;

	/* pid prefix: "NNNN VERB" (kdump); indent skipped */
	while (*p == ' ' || *p == '\t')
		p++;
	if (p[0] >= '0' && p[0] <= '9') {
		pid = strtol(p, (char **)&p, 10);
		if (*p == ' ')
			p++;
	}
	while (*p == ' ')
		p++;

	/* "<VERB>  rest" */
	{
		const char *verb_end = p;

		while (*verb_end != '\0' && *verb_end != ' ')
			verb_end++;
		verb_len = (size_t)(verb_end - p);
		is_nami = verb_len == 4 &&
			strncmp(p, "NAMI", 4) == 0;
		if (!is_nami) {
			/* CALL line: func name for context only -- the
			 * path arrives via NAMI; skip CALL lines v1
			 */
			return 0;
		}
		p = (char *)verb_end;
		while (*p == ' ')
			p++;
	}
	name = "open";            /* NAMI v1 attribution */
	name_len = 4;

	/* first quoted argument: the path (AT_FDCWD precedes it on
	 * *at syscalls and is not quoted).
	 */
	while (*p != '\0' && *p != '"')
		p++;
	if (*p != '"')
		return 0;
	used = unescape(p, path, sizeof(path));
	if (used == (size_t)-1)
		return 0;

	/* NAMI lines end after the quote: detail stays empty (the
	 * newline is not a flag); only real tokens land in detail
	 */
	{
		const char *start = p + used;
		const char *end;
		size_t n;

		while (*start == ' ' || *start == ',' ||
		       *start == '\n' || *start == '\r')
			start++;
		end = start;
		if (*start != '[' && *start != '{' && *start != '\0' &&
		    *start != '\n' && *start != '\r') {
			while (*end != '\0' && *end != ',' &&
			       *end != ')' && *end != '=' &&
			       *end != '\n' && *end != '\r')
				end++;
		}
		n = (size_t)(end - start);
		if (n >= sizeof(detail))
			n = sizeof(detail) - 1;
		memcpy(detail, start, n);
		detail[n] = '\0';
	}

	entry = json_value_init_object();
	entry_o = json_value_get_object(entry);
	json_object_set_number(entry_o, "time", 0);
	json_object_set_number(entry_o, "pid", (double)pid);
	json_object_set_number(entry_o, "tid", 0);
	json_object_set_string(entry_o, "module", "ktrace");
	json_object_set_string(entry_o, "severity", "INFO");

	/* message object */
	{
		JSON_Value *mv = json_value_init_object();
		char func_name[32];

		msg = json_value_get_object(mv);
		if (name_len >= sizeof(func_name))
			name_len = sizeof(func_name) - 1;
		memcpy(func_name, name, name_len);
		func_name[name_len] = '\0';
		json_object_set_string(msg, "func", func_name);
		json_object_set_string(msg, "path", path);
		if (detail[0] != '\0')
			json_object_set_string(msg, "detail", detail);
		json_object_set_value(entry_o, "message", mv);
	}
	json_array_append_value(out, entry);
	return 1;
}

int ktrace_convert(FILE *in, JSON_Array *out)
{
	char line[2048];
	size_t converted = 0;

	while (fgets(line, sizeof(line), in) != NULL) {
		if (convert_line(line, out))
			converted++;
	}
	return (int)converted;
}
