/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * etw2retrace: raw ETW rows -> retrace trace JSON
 * (TODO.trace-profile/24). See convert.h for the row shape and
 * the skip rules. Task names normalize to the POSIX-shaped
 * names the correlate classifier knows, exactly like
 * procmon2retrace.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "convert.h"
#include "parson.h"

/* ETW task name -> engine func name (NULL: skip the row). */
static const struct {
	const char *task;
	const char *func;
} g_task_map[] = {
	{ "CreateFile", "open" },
	{ "CreateNewFile", "open" },
	{ "ReadFile", "read" },
	{ "WriteFile", "write" },
	{ "DeleteFile", "unlink" },
	{ "SetRenameInformationFile", "rename" },
	{ "SetDispositionInformationFile", "unlink" },
	{ "QueryInformationFile", "stat" },
	{ "SetInformationFile", NULL },
	{ "CloseFile", NULL }, /* FileObject only, no name */
	{ "OperationEnd", NULL },
	/*
	 * Manifest decode is absent on some readers (CI evidence,
	 * TODO.trace-profile/24 round 2): the capture script then
	 * emits task "Id<N>" with the raw event Id. These numeric
	 * mappings are the Kernel-File manifest's task table,
	 * transcribed from the OS's OWN provider listing
	 * (Get-WinEvent -ListProvider, printed by the CI smoke on
	 * every Windows leg -- machine truth, not documentation
	 * guesswork). The v2.27.0 provisional Id14/Id15 mapping was
	 * WRONG (14 is Close, not Read); corrected in v2.29.2.
	 * Unmapped numeric Ids stay skipped (honest).
	 */
	{ "Id10", "open" },    /* NameCreate */
	{ "Id11", "unlink" },  /* NameDelete */
	{ "Id12", "open" },    /* Create */
	{ "Id13", NULL },      /* Cleanup (no name) */
	{ "Id14", NULL },      /* Close (no name) */
	{ "Id15", "read" },    /* Read */
	{ "Id16", "write" },   /* Write */
	{ "Id17", NULL },      /* SetInformation */
	{ "Id18", "unlink" },  /* SetDelete */
	{ "Id19", "rename" },  /* Rename */
	{ "Id20", NULL },      /* DirEnum */
	{ "Id21", NULL },      /* Flush */
	{ "Id22", "stat" },    /* QueryInformation */
	{ "Id23", NULL },      /* FSCTL */
	{ "Id24", NULL },      /* OperationEnd */
	{ "Id26", "unlink" },  /* DeletePath */
	{ "Id27", "rename" },  /* RenamePath */
	{ "Id30", "open" },    /* CreateNewFile */
	{ NULL, NULL }
};

static const char *task_to_func(const char *task)
{
	size_t i;

	for (i = 0; g_task_map[i].task != NULL; i++) {
		if (strcmp(g_task_map[i].task, task) == 0)
			return g_task_map[i].func;
	}
	return NULL;
}

static void append_entry(JSON_Array *out, double time, double pid,
	double tid, const char *func, const char *path,
	const char *detail)
{
	JSON_Value *entry = json_value_init_object();
	JSON_Object *entry_o = json_value_get_object(entry);
	JSON_Value *mv = json_value_init_object();
	JSON_Object *msg = json_value_get_object(mv);

	json_object_set_number(entry_o, "time", time);
	json_object_set_number(entry_o, "pid", pid);
	json_object_set_number(entry_o, "tid", tid);
	json_object_set_string(entry_o, "module", "etw");
	json_object_set_string(entry_o, "severity", "INFO");

	json_object_set_string(msg, "func", func);
	json_object_set_string(msg, "path", path);
	if (detail != NULL && detail[0] != '\0')
		json_object_set_string(msg, "detail", detail);
	json_object_set_value(entry_o, "message", mv);

	json_array_append_value(out, entry);
}

/*
 * Parse one jsonl row. Returns 1 when an entry was appended, 0
 * when skipped (no file, unmapped task, parse failure).
 */
static int convert_row(const char *line, JSON_Array *out)
{
	JSON_Value *root = json_parse_string(line);
	JSON_Object *row;
	const char *task;
	const char *file;
	const char *func;

	if (root == NULL)
		return 0;
	row = json_value_get_object(root);
	if (row == NULL) {
		json_value_free(root);
		return 0;
	}

	task = json_object_get_string(row, "task");
	file = json_object_get_string(row, "file");
	if (task == NULL || file == NULL || file[0] == '\0') {
		json_value_free(root);
		return 0;
	}
	func = task_to_func(task);
	if (func == NULL) {
		json_value_free(root);
		return 0;
	}

	append_entry(out,
		json_object_get_number(row, "time"),
		json_object_get_number(row, "pid"),
		json_object_get_number(row, "tid"),
		func, file, json_object_get_string(row, "detail"));

	json_value_free(root);
	return 1;
}

int etw_convert(FILE *in, JSON_Array *out)
{
	char line[8192];
	size_t converted = 0;

	while (fgets(line, sizeof(line), in) != NULL) {
		if (convert_row(line, out))
			converted++;
	}
	return (int)converted;
}
