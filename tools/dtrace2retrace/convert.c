/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * retrace-dtrace2retrace -- the macOS kernel-truth converter
 * (TODO.trace-profile/14). Feeds retrace-profile --kernel (and
 * retrace-correlate --outside) from a dtrace/dtruss file
 * capture; dtrace/dtruss need SIP off (csrutil disable) for
 * system binaries.
 *
 * usage: retrace-dtrace2retrace [-o out.json] dtruss.log
 *
 * Recognized line shape (anything else is skipped):
 *   PID/TSYS  syscall("path\0", 0x0, 0x0)         = 0 0
 * e.g.
 *   84546/0x30d7:  open_nocancel("/etc/hosts\0", 0x0, 0x0) = 0 0
 *
 * dtruss shows C strings with a literal "\0" suffix -- stripped.
 * Name variants normalize to the POSIX names the correlate
 * classifier knows (open_nocancel -> open, stat64 -> stat).
 */

/*
 * dtrace2retrace: dtrace/dtruss log -> retrace trace JSON
 * (TODO.trace-profile/14). The macOS kernel layer (truth); see
 * dtrace2retrace.c for the recognized line shapes.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "convert.h"
#include "parson.h"

#define CORR_PATH_MAX 4096

static size_t unescape(const char *src, char *dst, size_t dstsz)
{
	size_t i = 1;
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
			case '0':
				/* dtruss C-string suffix: a real NUL byte;
				 * stripped from the path tail below
				 */
				c = '\0';
				break;
			default:
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
	return i + 1;
}

static const char *const g_file_funcs[] = {
	"open", "open_nocancel", "openat", "openat_nocancel",
	"stat", "stat64", "lstat", "lstat64", "fstatat", "stat64_extended",
	"access", "unlink", "unlink_nocancel", "rename", "renameat",
	"link", "symlink", "mkdir", "rmdir",
	"readlink", "execve", "chmod", "chown",
	NULL
};

/* normalize to the names the correlate classifier knows */
static const char *const g_normalize[][2] = {
	{ "open_nocancel", "open" },
	{ "openat_nocancel", "openat" },
	{ "stat64", "stat" },
	{ "lstat64", "lstat" },
	{ "unlink_nocancel", "unlink" },
	{ "stat64_extended", "stat" },
	{ NULL, NULL }
};

static const char *normalized(const char *name)
{
	size_t i;

	for (i = 0; g_normalize[i][0] != NULL; i++) {
		if (strcmp(name, g_normalize[i][0]) == 0)
			return g_normalize[i][1];
	}
	return name;
}

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

static int convert_line(const char *line, JSON_Array *out)
{
	const char *p = line;
	const char *name;
	size_t name_len;
	char path[CORR_PATH_MAX];
	char detail[256];
	char func_name[32];
	size_t used;
	long pid = 0;
	long tid = 0;
	JSON_Value *entry;
	JSON_Object *entry_o;
	JSON_Object *msg;
	char *outp;
	size_t plen;

	/* dtruss indents syscall lines -- skip the leading pad */
	while (*p == ' ' || *p == '\t')
		p++;

	/* "PID/TID:  " prefix -- decimal pid, hex tid */
	if (p[0] < '0' || p[0] > '9')
		return 0;
	pid = strtol(p, &outp, 10);
	if (*outp != '/')
		return 0;
	tid = strtol(outp + 1, &outp, 16);
	if (*outp != ':')
		return 0;
	p = outp + 1;
	while (*p == ' ')
		p++;

	/* "<name>(" */
	name = p;
	p = strchr(p, '(');
	if (p == NULL)
		return 0;
	name_len = (size_t)(p - name);
	if (name_len == 0 || name_len >= sizeof(func_name))
		return 0;
	memcpy(func_name, name, name_len);
	func_name[name_len] = '\0';
	if (!is_file_func(func_name, name_len))
		return 0;

	/* first quoted argument: the path */
	while (*p != '\0' && *p != '"')
		p++;
	if (*p != '"')
		return 0;
	used = unescape(p, path, sizeof(path));
	if (used == (size_t)-1)
		return 0;
	/* dtruss suffixes C strings with a literal \0 -- strip */
	plen = strlen(path);
	while (plen > 0 && path[plen - 1] == '\0')
		plen--;
	if (plen == 0)
		return 0;
	path[plen] = '\0';

	/* flags: from after the path to ',' ')' '=' */
	{
		const char *start = p + used;
		const char *end;
		size_t n;

		while (*start == ' ' || *start == ',')
			start++;
		end = start;
		while (*end != '\0' && *end != ',' && *end != ')' &&
		       *end != '=')
			end++;
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
	json_object_set_number(entry_o, "tid", (double)tid);
	json_object_set_string(entry_o, "module", "dtrace");
	json_object_set_string(entry_o, "severity", "INFO");

	{
		JSON_Value *mv = json_value_init_object();

		msg = json_value_get_object(mv);
		json_object_set_string(msg, "func", normalized(func_name));
		json_object_set_string(msg, "path", path);
		if (detail[0] != '\0')
			json_object_set_string(msg, "detail", detail);
		json_object_set_value(entry_o, "message", mv);
	}
	json_array_append_value(out, entry);
	return 1;
}

int dtrace_convert(FILE *in, JSON_Array *out)
{
	char line[2048];
	size_t converted = 0;

	while (fgets(line, sizeof(line), in) != NULL) {
		if (convert_line(line, out))
			converted++;
	}
	return (int)converted;
}
