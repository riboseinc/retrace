/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Unit tests for the procmon -> retrace entry converter
 * (TODO.next-level/06): header mapping and the entry shape
 * (pid numeric, module ETW, severity from Result, NT paths
 * passed through for the correlate normalizer).
 */

#include "convert.h"
#include "csv.h"
#include "parson.h"

#include <stdio.h>
#include <string.h>

static int tests_run;
static int tests_pass;
static int tests_fail;

#define TEST(name)                               \
	do {                                     \
		tests_run++;                     \
		printf("  TEST %s ... ", #name); \
		test_##name();                   \
		tests_pass++;                    \
		printf("OK\n");                  \
	} while (0)

#define CHECK(cond)                                                             \
	do {                                                                    \
		if (!(cond)) {                                                  \
			printf("FAIL [%s:%d] %s\n", __FILE__, __LINE__, #cond); \
			tests_fail++;                                           \
			return;                                                 \
		}                                                               \
	} while (0)

/* Row-builder shim: scan exactly one literal line into a row. */
static struct PmCsvRow *g_dst;
static size_t *g_count;

/*
 * Rebase pointers into the copy: the scanner reuses its row
 * storage and frees it when the scan returns.
 */
static void
grab_row(const struct PmCsvRow *row, void *ctx)
{
	size_t i;

	(void) ctx;
	if (*g_count == 0) {
		memcpy(g_dst, row, sizeof(*row));
		for (i = 0; i < row->count; i++)
			g_dst->fields[i] = g_dst->storage + (row->fields[i] - row->storage);
	}
	(*g_count)++;
}

static size_t
scan_into(const char *text, struct PmCsvRow *dst)
{
	size_t count = 0;

	g_dst = dst;
	g_count = &count;
	(void) pmcsv_scan(text, strlen(text), grab_row, NULL, NULL);
	g_dst = NULL;
	g_count = NULL;
	return count;
}

static void
test_header_map(void)
{
	struct PmCsvRow header;
	int		colmap[PMCOL_COUNT];

	CHECK(scan_into("\"Time of Day\",\"Process Name\",\"PID\","
			"\"Operation\",\"Path\",\"Result\",\"Detail\"",
			&header) == 1);
	CHECK(pmconv_map_header(&header, colmap) == 0);
	CHECK(colmap[PMCOL_TOD] == 0);
	CHECK(colmap[PMCOL_PID] == 2);
	CHECK(colmap[PMCOL_OPERATION] == 3);
	CHECK(colmap[PMCOL_DETAIL] == 6);
}

static void
test_header_map_case_insensitive(void)
{
	struct PmCsvRow header;
	int		colmap[PMCOL_COUNT];

	CHECK(scan_into("\"TIME OF DAY\",\"Process Name\",\"pid\","
			"\"operation\",\"path\",\"result\",\"detail\"",
			&header) == 1);
	CHECK(pmconv_map_header(&header, colmap) == 0);
	CHECK(colmap[PMCOL_OPERATION] == 3);
}

static void
test_header_map_rejects_garbage(void)
{
	struct PmCsvRow row;
	int		colmap[PMCOL_COUNT];

	CHECK(scan_into("name,age,city", &row) == 1);
	CHECK(pmconv_map_header(&row, colmap) == -1);
	CHECK(pmconv_map_header(NULL, colmap) == -1);
}

static void
test_entry_shape_success(void)
{
	struct PmCsvRow row;
	JSON_Value *v;
	JSON_Object *root, *msg;
	int		colmap[PMCOL_COUNT];
	int		i;

	for (i = 0; i < PMCOL_COUNT; i++)
		colmap[i] = i;

	CHECK(
	  scan_into(
	    "\"10:33:45.1234567 "
	    "AM\",\"app.exe\",\"4212\",\"CreateFile\",\"\\Device\\HarddiskVolume3\\pkg\\main."
	    "dat\",\"SUCCESS\",\"Desired Access: Generic Read\"",
	    &row) == 1);

	v = pmconv_entry(&row, colmap);
	CHECK(v != NULL);
	root = json_value_get_object(v);
	CHECK(json_object_get_number(root, "pid") == 4212);
	CHECK(json_object_get_number(root, "tid") == 0);
	CHECK(strcmp(json_object_get_string(root, "module"), "ETW") == 0);
	CHECK(strcmp(json_object_get_string(root, "severity"), "INFO") == 0);
	msg = json_object_get_object(root, "message");
	CHECK(msg != NULL);
	CHECK(strcmp(json_object_get_string(msg, "func"), "CreateFile") == 0);
	CHECK(strcmp(json_object_get_string(msg, "path"),
		     "\\Device\\HarddiskVolume3\\pkg\\main.dat") == 0);
	CHECK(strcmp(json_object_get_string(msg, "process"), "app.exe") == 0);
	json_value_free(v);
}

static void
test_entry_shape_failure_warns(void)
{
	struct PmCsvRow row;
	JSON_Value *v;
	JSON_Object *root;
	int		colmap[PMCOL_COUNT];
	int		i;

	for (i = 0; i < PMCOL_COUNT; i++)
		colmap[i] = i;

	CHECK(scan_into("\"10:33:45.1 AM\",\"app.exe\",\"4212\","
			"\"CreateFile\",\"C:\\nope\",\"NAME NOT FOUND\","
			"\"SyncType: Sync+Create\"",
			&row) == 1);

	v = pmconv_entry(&row, colmap);
	CHECK(v != NULL);
	root = json_value_get_object(v);
	CHECK(strcmp(json_object_get_string(root, "severity"), "WARN") == 0);
	json_value_free(v);
}

static void
test_entry_missing_columns(void)
{
	struct PmCsvRow row;
	JSON_Value *v;
	JSON_Object *root, *msg;
	int		colmap[PMCOL_COUNT];
	int		i;

	for (i = 0; i < PMCOL_COUNT; i++)
		colmap[i] = -1;
	colmap[PMCOL_OPERATION] = 1; /* only Operation present */

	CHECK(scan_into("\"4212\",\"CloseFile\"", &row) == 1);
	CHECK(row.count == 2);

	v = pmconv_entry(&row, colmap);
	CHECK(v != NULL);
	root = json_value_get_object(v);
	CHECK(json_object_get_number(root, "pid") == 0);
	msg = json_object_get_object(root, "message");
	CHECK(json_object_get_string(msg, "path") == NULL);
	CHECK(strcmp(json_object_get_string(msg, "func"), "CloseFile") == 0);
	json_value_free(v);
	CHECK(pmconv_entry(NULL, colmap) == NULL);
	CHECK(pmconv_entry(&row, NULL) == NULL);
}

int
main(void)
{
	printf("procmon convert tests\n");
	TEST(header_map);
	TEST(header_map_case_insensitive);
	TEST(header_map_rejects_garbage);
	TEST(entry_shape_success);
	TEST(entry_shape_failure_warns);
	TEST(entry_missing_columns);
	printf("%d run, %d pass, %d fail\n", tests_run, tests_pass, tests_fail);
	return (tests_fail == 0) ? 0 : 1;
}
