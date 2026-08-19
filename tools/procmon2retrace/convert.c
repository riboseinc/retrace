/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#include "convert.h"

#include <stdlib.h>
#include <string.h>

static const char *g_colnames[PMCOL_COUNT] = {
	"time of day", "process name", "pid", "operation",
	"path", "result", "detail",
};

static int
ieq(const char *a, const char *b)
{
	while (*a != '\0' && *b != '\0') {
		char ca = *a;
		char cb = *b;

		if (ca >= 'A' && ca <= 'Z')
			ca = (char) (ca - 'A' + 'a');
		if (cb >= 'A' && cb <= 'Z')
			cb = (char) (cb - 'A' + 'a');
		if (ca != cb)
			return 0;
		a++;
		b++;
	}
	return *a == '\0' && *b == '\0';
}

int
pmconv_map_header(const struct PmCsvRow *header, int colmap[PMCOL_COUNT])
{
	int mapped = 0;
	int col;

	for (col = 0; col < PMCOL_COUNT; col++)
		colmap[col] = -1;

	if (header == NULL)
		return -1;

	for (col = 0; col < (int) header->count; col++) {
		int role;

		for (role = 0; role < PMCOL_COUNT; role++) {
			if (colmap[role] == -1 && ieq(header->fields[col], g_colnames[role])) {
				colmap[role] = col;
				mapped++;
				break;
			}
		}
	}

	/* A recognizable procmon header at least names the operation. */
	if (colmap[PMCOL_OPERATION] == -1)
		return -1;
	return mapped > 1 ? 0 : -1;
}

static const char *
field_or_null(const struct PmCsvRow *row, int idx)
{
	if (idx < 0 || (size_t) idx >= row->count)
		return NULL;
	return row->fields[idx];
}

static void
set_if(JSON_Object *o, const char *key, const char *v)
{
	if (v != NULL)
		json_object_set_string(o, key, v);
}

JSON_Value *
pmconv_entry(const struct PmCsvRow *row, const int colmap[PMCOL_COUNT])
{
	JSON_Value *v;
	JSON_Value *msg_v;
	JSON_Object *root;
	JSON_Object *msg;
	const char *result;
	double	     pid = 0;

	if (row == NULL || colmap == NULL)
		return NULL;

	v = json_value_init_object();
	if (v == NULL)
		return NULL;
	root = json_value_get_object(v);

	msg_v = json_value_init_object();
	if (msg_v == NULL) {
		json_value_free(v);
		return NULL;
	}
	msg = json_value_get_object(msg_v);

	if (colmap[PMCOL_PID] >= 0) {
		const char *pid_s = field_or_null(row, colmap[PMCOL_PID]);

		if (pid_s != NULL)
			pid = strtod(pid_s, NULL);
	}

	result = field_or_null(row, colmap[PMCOL_RESULT]);

	json_object_set_number(root, "time", 0);
	json_object_set_number(root, "pid", pid);
	json_object_set_number(root, "tid", 0);
	json_object_set_string(root, "module", "ETW");
	json_object_set_string(
	  root, "severity", (result != NULL && ieq(result, "success")) ? "INFO" : "WARN");

	set_if(msg, "func", field_or_null(row, colmap[PMCOL_OPERATION]));
	set_if(msg, "process", field_or_null(row, colmap[PMCOL_PROCESS]));
	set_if(msg, "time_of_day", field_or_null(row, colmap[PMCOL_TOD]));
	set_if(msg, "path", field_or_null(row, colmap[PMCOL_PATH]));
	set_if(msg, "result", result);
	set_if(msg, "detail", field_or_null(row, colmap[PMCOL_DETAIL]));

	json_object_set_value(root, "message", msg_v);
	return v;
}
