/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Unit tests for the correlate matcher (TODO.windows/01-03).
 *
 * Covers:
 *   - corr_is_path_like across absolute, drive-letter, relative,
 *     UNC, and non-path shapes
 *   - corr_normalize: POSIX passthrough, trailing slash, root,
 *     NT "\??\", "\\?\" and '//?/' prefixes, the
 *     HarddiskVolume drive guess (3 -> C:), case preservation,
 *     overflow rejection, non-path rejection
 *   - corr_pathcmp: drive-letter case-insensitivity, everything
 *     else case-sensitive, ordering, NULL handling
 *   - CorrSet: dedupe, finish, binary-search contains, free
 *   - corr_classify: probe/read/write tables + detail heuristic
 *   - corr_index_add_entry: nested extraction with pid/time
 *   - corr_entry_is_escape: coverage semantics -- pure
 *     set-difference, pid-aware coverage, --pid filter,
 *     time-window for lazy materialization, --exclude-probes,
 *     prefix boundary (component, not substring)
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
	char out[CORR_PATH_MAX];
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
	/* libsass's forward-slash spelling before it flips the
	 * separators (src/file.cpp).
	 */
	norm_case("//?/C:\\pkg\\file", "C:/pkg/file");
	norm_case("//?/C:/pkg/file", "C:/pkg/file");
	/* POSIX UNC keeps its leading slashes. */
	norm_case("//server/share/x", "//server/share/x");
}

static void
test_normalize_harddisk_volume(void)
{
	/* Volume N -> 'A'+N-1; on a standard install volume 3 is C:. */
	norm_case("\\Device\\HarddiskVolume3\\pkg\\file", "C:/pkg/file");
	norm_case("\\Device\\HarddiskVolume4\\pkg\\file", "D:/pkg/file");
	norm_case("\\Device\\HarddiskVolume3\\", "C:");
	/* Out of letter range: no drive guess, but the path itself
	 * still normalizes (separator kept).
	 */
	{
		char out[CORR_PATH_MAX];

		CHECK(corr_normalize("\\Device\\HarddiskVolume27\\x",
			out, sizeof(out)) > 0);
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

/* ----- corr_classify ----- */

static void
test_classify(void)
{
	CHECK(corr_classify(NULL, NULL) == CORR_CLS_NONE);
	CHECK(corr_classify("QueryOpen", NULL) == CORR_CLS_PROBE);
	CHECK(corr_classify("GetFileAttributesW", NULL) == CORR_CLS_PROBE);
	CHECK(corr_classify("stat", NULL) == CORR_CLS_PROBE);
	CHECK(corr_classify("access", NULL) == CORR_CLS_PROBE);
	CHECK(corr_classify("CreateFile",
		"Desired Access: Generic Read") == CORR_CLS_READ);
	CHECK(corr_classify("CreateFile",
		"Desired Access: Generic Write") == CORR_CLS_WRITE);
	CHECK(corr_classify("NtCreateFile",
		"Desired Access: Write Data") == CORR_CLS_WRITE);
	CHECK(corr_classify("fopen", "mode w") == CORR_CLS_WRITE);
	CHECK(corr_classify("fopen", "mode r") == CORR_CLS_READ);
	CHECK(corr_classify("WriteFile", NULL) == CORR_CLS_WRITE);
	CHECK(corr_classify("unlink", NULL) == CORR_CLS_WRITE);
	CHECK(corr_classify("ReadFile", NULL) == CORR_CLS_READ);
	CHECK(corr_classify("open", NULL) == CORR_CLS_READ);
	CHECK(strcmp(corr_class_str(CORR_CLS_PROBE), "probe") == 0);
	CHECK(strcmp(corr_class_str(CORR_CLS_READ), "read") == 0);
	CHECK(strcmp(corr_class_str(CORR_CLS_WRITE), "write") == 0);
	CHECK(strcmp(corr_class_str(CORR_CLS_NONE), "none") == 0);
}

/* ----- index + escape decision ----- */

static JSON_Object *entry_full(const char *func, const char *path,
			       double pid, double tid, double t)
{
	JSON_Value *v = json_value_init_object();
	JSON_Object *root = json_value_get_object(v);
	JSON_Value *msg = json_value_init_object();
	JSON_Object *msg_o = json_value_get_object(msg);

	json_object_set_number(root, "pid", pid);
	json_object_set_number(root, "tid", tid);
	json_object_set_number(root, "time", t);
	if (func != NULL)
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
criteria(struct CorrCriteria *c, const char *prefix)
{
	memset(c, 0, sizeof(*c));
	c->prefix = prefix;
}

static void
test_escape_set_semantics(void)
{
	struct CorrIndex inside;
	struct CorrCriteria c;
	struct CorrEscape esc;
	JSON_Object *e;

	corr_index_init(&inside);
	e = entry_full("open", "/mnt/tfs/covered.so", 601, 3, 100);
	corr_index_add_entry(e, &inside);
	entry_free(e);
	corr_index_finish(&inside);

	memset(&esc, 0, sizeof(esc));
	criteria(&c, "/mnt/tfs");

	e = entry_full("open", "/mnt/tfs/covered.so", 601, 3, 101);
	CHECK(corr_entry_is_escape(e, &c, &inside, &esc) == 0);
	entry_free(e);

	e = entry_full("creat", "/mnt/tfs/escape.bin", 601, 9, 102);
	CHECK(corr_entry_is_escape(e, &c, &inside, &esc) == 1);
	CHECK(strcmp(esc.path, "/mnt/tfs/escape.bin") == 0);
	CHECK(esc.func != NULL && strcmp(esc.func, "creat") == 0);
	CHECK(esc.tid == 9);
	CHECK(esc.pid == 601);
	CHECK(esc.cls == CORR_CLS_WRITE);
	entry_free(e);

	/* Prefix matches on a path component, never a substring. */
	e = entry_full("stat", "/mnt/tfs2/cache.bin", 601, 9, 103);
	CHECK(corr_entry_is_escape(e, &c, &inside, &esc) == 0);
	entry_free(e);

	e = entry_full("open", "/var/log/x", 601, 9, 104);
	CHECK(corr_entry_is_escape(e, &c, &inside, &esc) == 0);
	entry_free(e);

	CHECK(corr_entry_is_escape(NULL, &c, &inside, &esc) == 0);
	corr_index_free(&inside);
}

static void
test_escape_pid_coverage(void)
{
	struct CorrIndex inside;
	struct CorrCriteria c;
	struct CorrEscape esc;
	JSON_Object *e;

	/* pid 601 saw the path; pid 777 touching the same path is
	 * NOT covered (TODO.windows/01).
	 */
	corr_index_init(&inside);
	e = entry_full("open", "/mnt/tfs/a", 601, 1, 100);
	corr_index_add_entry(e, &inside);
	entry_free(e);
	corr_index_finish(&inside);

	criteria(&c, "/mnt/tfs");
	e = entry_full("open", "/mnt/tfs/a", 777, 1, 101);
	CHECK(corr_entry_is_escape(e, &c, &inside, &esc) == 1);
	entry_free(e);

	/* Same pid: covered. */
	e = entry_full("open", "/mnt/tfs/a", 601, 1, 101);
	CHECK(corr_entry_is_escape(e, &c, &inside, &esc) == 0);
	entry_free(e);

	/* Pid-less outside entry: wildcard, covered. */
	e = entry_full("open", "/mnt/tfs/a", 0, 1, 101);
	CHECK(corr_entry_is_escape(e, &c, &inside, &esc) == 0);
	entry_free(e);

	/* --pid filter drops other pids entirely. */
	c.pid = 601;
	e = entry_full("open", "/mnt/tfs/other", 777, 1, 102);
	CHECK(corr_entry_is_escape(e, &c, &inside, &esc) == 0);
	entry_free(e);
	c.pid = 777;
	e = entry_full("open", "/mnt/tfs/other", 777, 1, 102);
	CHECK(corr_entry_is_escape(e, &c, &inside, &esc) == 1);
	entry_free(e);

	corr_index_free(&inside);
}

static void
test_escape_time_window(void)
{
	struct CorrIndex inside;
	struct CorrCriteria c;
	struct CorrEscape esc;
	JSON_Object *e;

	/* Lazy materialization: the open at t=100 PRECEDES the
	 * materialize record at t=101 (TODO.windows/02).
	 */
	corr_index_init(&inside);
	e = entry_full("materialize", "/mnt/tfs/lazy.dat", 42, 1, 101);
	corr_index_add_entry(e, &inside);
	entry_free(e);
	corr_index_finish(&inside);

	/* Pure set semantics: covered (the path is eventually
	 * seen, whenever the materialize landed).
	 */
	criteria(&c, "/mnt/tfs");
	e = entry_full("open", "/mnt/tfs/lazy.dat", 42, 7, 100);
	CHECK(corr_entry_is_escape(e, &c, &inside, &esc) == 0);
	entry_free(e);

	/* Window 2s covers the 1s lazy gap. */
	c.window = 2.0;
	e = entry_full("open", "/mnt/tfs/lazy.dat", 42, 7, 100);
	CHECK(corr_entry_is_escape(e, &c, &inside, &esc) == 0);
	entry_free(e);

	/* Open at t=98: the materialize 3s later is too late to
	 * be the server of this open -- an escape under the
	 * window.
	 */
	e = entry_full("open", "/mnt/tfs/lazy.dat", 42, 7, 98);
	CHECK(corr_entry_is_escape(e, &c, &inside, &esc) == 1);
	entry_free(e);

	corr_index_free(&inside);
}

static void
test_escape_exclude_probes(void)
{
	struct CorrIndex inside;
	struct CorrCriteria c;
	struct CorrEscape esc;
	JSON_Object *e;

	corr_index_init(&inside);
	corr_index_finish(&inside);

	criteria(&c, "/mnt/tfs");
	c.exclude_probes = 1;

	e = entry_full("QueryOpen", "/mnt/tfs/probe.dat", 5, 1, 100);
	CHECK(corr_entry_is_escape(e, &c, &inside, &esc) == 0);
	entry_free(e);

	e = entry_full("CreateFile", "/mnt/tfs/data.dat", 5, 1, 100);
	CHECK(corr_entry_is_escape(e, &c, &inside, &esc) == 1);
	CHECK(esc.cls == CORR_CLS_READ);
	entry_free(e);

	/* Without the flag the probe is reported (and classified). */
	c.exclude_probes = 0;
	e = entry_full("QueryOpen", "/mnt/tfs/probe.dat", 5, 1, 100);
	CHECK(corr_entry_is_escape(e, &c, &inside, &esc) == 1);
	CHECK(esc.cls == CORR_CLS_PROBE);
	entry_free(e);

	corr_index_free(&inside);
}

static void
test_index_nested_extraction(void)
{
	struct CorrIndex idx;
	JSON_Object *root;
	JSON_Object *msg;

	root = entry_full("open", "/mnt/tfs/a.so", 7, 8, 42);
	msg = json_object_get_object(root, "message");
	json_object_set_string(msg, "extra", "/mnt/tfs/b.flag");

	corr_index_init(&idx);
	corr_index_add_entry(root, &idx);
	corr_index_finish(&idx);
	CHECK(idx.count == 2);
	CHECK(corr_set_contains(&idx.set, "/mnt/tfs/a.so") == 1);
	CHECK(corr_set_contains(&idx.set, "/mnt/tfs/b.flag") == 1);
	/* Records carry the entry's pid and time. */
	CHECK(idx.recs[0].pid == 7);
	CHECK(idx.recs[0].time == 42);
	corr_index_free(&idx);
	entry_free(root);

	corr_index_init(&idx);
	corr_index_add_entry(NULL, &idx);
	corr_index_free(&idx);
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
	TEST(classify);
	TEST(escape_set_semantics);
	TEST(escape_pid_coverage);
	TEST(escape_time_window);
	TEST(escape_exclude_probes);
	TEST(index_nested_extraction);
	printf("%d run, %d pass, %d fail\n", tests_run, tests_pass,
		tests_fail);
	return (tests_fail == 0) ? 0 : 1;
}
