/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Unit tests for the correlate matcher (TODO.next-level/02).
 *
 * Covers:
 *   - corr_is_path_like across absolute, drive-letter, relative,
 *     UNC, and non-path shapes
 *   - corr_normalize: POSIX passthrough, trailing slash, root,
 *     NT "\??\" and "\\?\" prefixes, the HarddiskVolume drive
 * guess (3 -> C:), case preservation, overflow rejection,
 *     non-path rejection
 *   - corr_pathcmp: drive-letter case-insensitivity, everything
 * else case-sensitive, ordering, NULL handling
 *   - CorrSet: dedupe (via corr_pathcmp), finish, binary-search
 * contains, free
 *   - corr_collect_paths: nested extraction, non-path filtering
 *   - corr_entry_is_escape: the inside \ outside set-difference,
 *     prefix boundary (component, not substring), escape fields
 *
 * Note: function calls live OUTSIDE assert() so the side-effecting
 * call still happens under -DNDEBUG.
 */

#include "parson.h"
#include "match.h"

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

/* Always-on check. assert() alone is compiled out by NDEBUG. */
#define CHECK(cond)                                                             \
	do {                                                                    \
		if (!(cond)) {                                                  \
			printf("FAIL [%s:%d] %s\n", __FILE__, __LINE__, #cond); \
			tests_fail++;                                           \
			return;                                                 \
		}                                                               \
	} while (0)

static void
norm_case(const char *in, const char *want)
{
	char   out[CORR_PATH_MAX];
	size_t r;

	r = corr_normalize(in, out, sizeof(out));
	CHECK(r == strlen(want) + 1);
	CHECK(strcmp(out, want) == 0);
}

/* ----- corr_is_path_like ----- */

static void
test_is_path_like(void)
{
	CHECK(corr_is_path_like("/mnt/tfs/a") == 1);
	CHECK(corr_is_path_like("C:\\pkg\\a") == 1);
	CHECK(corr_is_path_like("c:/pkg/a") == 1);
	CHECK(corr_is_path_like("rel/path") == 1);
	CHECK(corr_is_path_like("\\\\server\\share") == 1);
	CHECK(corr_is_path_like("hello") == 0);
	CHECK(corr_is_path_like("a") == 0);
	CHECK(corr_is_path_like("") == 0);
	CHECK(corr_is_path_like(NULL) == 0);
}

/* ----- corr_normalize ----- */

static void
test_normalize_posix(void)
{
	norm_case("/mnt/tfs/pkg/file.so", "/mnt/tfs/pkg/file.so");
	norm_case("/mnt/tfs/", "/mnt/tfs");
	norm_case("/", "/");
	CHECK(corr_normalize("/mnt/tfs", NULL, 16) == 0);
}

static void
test_normalize_nt_prefixes(void)
{
	norm_case("\\??\\C:\\pkg\\file", "C:/pkg/file");
	norm_case("\\\\?\\C:\\pkg\\file", "C:/pkg/file");
	/* Case is preserved past the drive letter. */
	norm_case("\\??\\c:\\PKG\\File", "c:/PKG/File");
}

static void
test_normalize_harddisk_volume(void)
{
	/* Volume N -> 'A'+N-1; on a standard install volume 3 is C:. */
	norm_case("\\Device\\HarddiskVolume3\\pkg\\file", "C:/pkg/file");
	norm_case("\\Device\\HarddiskVolume4\\pkg\\file", "D:/pkg/file");
	norm_case("\\Device\\HarddiskVolume3\\", "C:");
	/*
	 * Out of letter range: no drive guess, but the path itself
	 * still normalizes (separator kept).
	 */
	{
		char out[CORR_PATH_MAX];

		CHECK(corr_normalize("\\Device\\HarddiskVolume27\\x", out, sizeof(out)) > 0);
		CHECK(strcmp(out, "/x") == 0);
	}
}

static void
test_normalize_rejects(void)
{
	char out[CORR_PATH_MAX];
	char big[2048];

	CHECK(corr_normalize("malloc", out, sizeof(out)) == 0);
	CHECK(corr_normalize(NULL, out, sizeof(out)) == 0);
	CHECK(corr_normalize("/a", out, 0) == 0);
	memset(big, 'a', sizeof(big) - 1);
	big[sizeof(big) - 1] = '\0';
	big[0] = '/';
	CHECK(corr_normalize(big, out, sizeof(out)) == 0);
}

/* ----- corr_pathcmp ----- */

static void
test_pathcmp(void)
{
	CHECK(corr_pathcmp("c:/pkg/a", "C:/pkg/a") == 0);
	CHECK(corr_pathcmp("C:/pkg/a", "C:/pkg/a") == 0);
	CHECK(corr_pathcmp("C:/pkg/a", "C:/pkg/b") < 0);
	CHECK(corr_pathcmp("C:/pkg/b", "C:/pkg/a") > 0);
	/* Past the drive letter everything is case-sensitive. */
	CHECK(corr_pathcmp("C:/PKG/a", "C:/pkg/a") != 0);
	CHECK(corr_pathcmp("/a/b", "/a/c") < 0);
	CHECK(corr_pathcmp(NULL, NULL) == 0);
	CHECK(corr_pathcmp(NULL, "/a") == -1);
	CHECK(corr_pathcmp("/a", NULL) == 1);
}

/* ----- CorrSet ----- */

static void
test_set_dedupe_and_contains(void)
{
	struct CorrSet s;

	corr_set_init(&s);
	CHECK(corr_set_add(&s, "/mnt/tfs/a") == 0);
	CHECK(corr_set_add(&s, "/mnt/tfs/b") == 0);
	/* Dedupe is by corr_pathcmp: drive case-insensitive. */
	CHECK(corr_set_add(&s, "C:/x") == 0);
	CHECK(corr_set_add(&s, "c:/x") == 0);
	CHECK(s.count == 3);
	corr_set_finish(&s);
	CHECK(corr_set_contains(&s, "/mnt/tfs/a") == 1);
	CHECK(corr_set_contains(&s, "/mnt/tfs/zz") == 0);
	CHECK(corr_set_contains(&s, "C:/x") == 1);
	CHECK(corr_set_contains(&s, "c:/x") == 1);
	corr_set_free(&s);
	CHECK(s.items == NULL && s.count == 0);
	corr_set_free(&s);
}

/* ----- corr_collect_paths ----- */

static void
test_collect_paths_nested(void)
{
	JSON_Value *v = json_value_init_object();
	JSON_Object *root = json_value_get_object(v);
	JSON_Value *msg = json_value_init_object();
	JSON_Object *msg_o = json_value_get_object(msg);
	JSON_Value *params = json_value_init_object();
	JSON_Object *params_o = json_value_get_object(params);
	JSON_Value *flags = json_value_init_array();
	JSON_Array *flags_a = json_value_get_array(flags);
	struct CorrSet s;

	json_object_set_string(msg_o, "func", "open");
	json_object_set_string(params_o, "path", "/mnt/tfs/a.so");
	json_object_set_string(params_o, "mode", "readme-not-a-path");
	json_array_append_string(flags_a, "/mnt/tfs/b.flag");
	json_object_set_value(msg_o, "params", params);
	json_object_set_value(msg_o, "flags", flags);
	json_object_set_value(root, "message", msg);

	corr_set_init(&s);
	CHECK(corr_collect_paths(root, &s) == 2);
	corr_set_finish(&s);
	CHECK(corr_set_contains(&s, "/mnt/tfs/a.so") == 1);
	CHECK(corr_set_contains(&s, "/mnt/tfs/b.flag") == 1);
	CHECK(corr_set_contains(&s, "/mnt/tfs/c") == 0);
	corr_set_free(&s);

	CHECK(corr_collect_paths(NULL, &s) == 0);
	json_value_free(v);
}

/* ----- corr_entry_is_escape ----- */

static JSON_Object *
entry_open(const char *func, const char *path, double tid)
{
	JSON_Value *v = json_value_init_object();
	JSON_Object *root = json_value_get_object(v);
	JSON_Value *msg = json_value_init_object();
	JSON_Object *msg_o = json_value_get_object(msg);

	json_object_set_number(root, "tid", tid);
	json_object_set_string(msg_o, "func", func);
	json_object_set_string(msg_o, "path", path);
	json_object_set_value(root, "message", msg);
	return root;
}

static void
entry_free(JSON_Object *o)
{
	json_value_free(json_object_get_wrapping_value(o));
}

static void
test_escape_decision(void)
{
	struct CorrSet	  inside;
	struct CorrEscape esc;
	JSON_Object *e;

	corr_set_init(&inside);
	corr_set_add(&inside, "/mnt/tfs/covered.so");
	corr_set_finish(&inside);

	memset(&esc, 0, sizeof(esc));
	e = entry_open("open", "/mnt/tfs/covered.so", 7);
	CHECK(corr_entry_is_escape(e, "/mnt/tfs", &inside, &esc) == 0);
	entry_free(e);

	e = entry_open("creat", "/mnt/tfs/escape.bin", 9);
	CHECK(corr_entry_is_escape(e, "/mnt/tfs", &inside, &esc) == 1);
	CHECK(strcmp(esc.path, "/mnt/tfs/escape.bin") == 0);
	CHECK(esc.func != NULL && strcmp(esc.func, "creat") == 0);
	CHECK(esc.tid == 9);
	entry_free(e);

	/* Prefix matches on a path component, never a substring. */
	e = entry_open("stat", "/mnt/tfs2/cache.bin", 9);
	CHECK(corr_entry_is_escape(e, "/mnt/tfs", &inside, &esc) == 0);
	entry_free(e);

	/* Path outside the prefix entirely. */
	e = entry_open("open", "/var/log/x", 9);
	CHECK(corr_entry_is_escape(e, "/mnt/tfs", &inside, &esc) == 0);
	entry_free(e);

	/* No paths at all. */
	e = entry_open("malloc", "not-a-path", 9);
	CHECK(corr_entry_is_escape(e, "/mnt/tfs", &inside, &esc) == 0);
	entry_free(e);

	CHECK(corr_entry_is_escape(NULL, "/mnt/tfs", &inside, &esc) == 0);
	corr_set_free(&inside);
}

int
main(void)
{
	printf("correlate match tests\n");
	TEST(is_path_like);
	TEST(normalize_posix);
	TEST(normalize_nt_prefixes);
	TEST(normalize_harddisk_volume);
	TEST(normalize_rejects);
	TEST(pathcmp);
	TEST(set_dedupe_and_contains);
	TEST(collect_paths_nested);
	TEST(escape_decision);
	printf("%d run, %d pass, %d fail\n", tests_run, tests_pass, tests_fail);
	return (tests_fail == 0) ? 0 : 1;
}
