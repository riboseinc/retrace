/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Unit tests for the profiler aggregation and capability scan
 * (TODO.windows/08).
 *
 * aggregate.c: reduce a trace (either schema: golden parity
 * message.func + params, or live logger direct keys with deref
 * arrays) to functions / accesses / env.
 *
 * capability.c: static syscall-gadget scan of executable segments
 * (synthetic ELF64).
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "aggregate.h"
#include "capability.h"
#include "parson.h"

static int tests_run;
static int tests_pass;
static int tests_fail;

#define TEST(name) do { \
	tests_run++; \
	printf("  TEST %s ... ", #name); \
	test_##name(); \
	tests_pass++; \
	printf("OK\n"); \
} while (0)

/* Always-on check: assert() compiles to nothing under NDEBUG. */
#define CHECK(cond) do { \
	if (!(cond)) { \
		printf("FAIL [%s:%d] %s\n", __FILE__, __LINE__, #cond); \
		tests_fail++; \
		return; \
	} \
} while (0)

/* Parse one entry and feed it to the profile. */
static void add_json(struct Profile *p, const char *entry)
{
	JSON_Value *v = json_parse_string(entry);
	JSON_Object *o;

	CHECK(v != NULL);
	o = json_value_get_object(v);
	CHECK(o != NULL);
	prof_add_entry(o, p);
	json_value_free(v);
}

static void test_golden_schema(void)
{
	struct Profile p;

	prof_init(&p);
	add_json(&p,
		"{\"pid\": 501, \"message\": {\"func\": \"open\","
		" \"params\": {\"path\": \"/mnt/pkg/lib.dat\"}}}");
	prof_finish(&p);

	CHECK(prof_names_get(&p.functions, "open") == 1);
	CHECK(p.accesses.count == 1);
	CHECK(strcmp(p.accesses.items[0].path, "/mnt/pkg/lib.dat") == 0);
	CHECK(p.accesses.items[0].class_read == 1);

	prof_free(&p);
}

static void test_live_schema(void)
{
	struct Profile p;

	prof_init(&p);
	add_json(&p,
		"{\"pid\": 9, \"message\": {\"func\": \"open\","
		" \"path\": \"0xdeadbeef\","
		" \"*path\": [\"/vfs/entry.dat\"], \"flags\": \"0\"}}");
	prof_finish(&p);

	CHECK(prof_names_get(&p.functions, "open") == 1);
	/* the pointer string itself must not become an access */
	CHECK(p.accesses.count == 1);
	CHECK(strcmp(p.accesses.items[0].path, "/vfs/entry.dat") == 0);

	prof_free(&p);
}

static void test_banner_skipped(void)
{
	struct Profile p;

	prof_init(&p);
	add_json(&p,
		"{\"message\": {\"text\":"
		" \"config file is set to: 'cfg/dir/trace.json'\"}}");
	prof_finish(&p);

	CHECK(p.entries == 1);
	CHECK(p.functions.count == 0);
	/* the tracer's own config path must not leak in */
	CHECK(p.accesses.count == 0);

	prof_free(&p);
}

static void test_return_summary_skipped(void)
{
	struct Profile p;

	prof_init(&p);
	add_json(&p,
		"{\"message\": {\"func\": \"open\","
		" \"call_duration_us\": 12, \"ret_val\": 3}}");
	prof_finish(&p);

	/* timing entries re-state func; counting them doubles calls */
	CHECK(p.functions.count == 0);

	prof_free(&p);
}

static void test_env_deref(void)
{
	struct Profile p;

	prof_init(&p);
	add_json(&p,
		"{\"message\": {\"func\": \"getenv\","
		" \"name\": \"0x1000\", \"*name\": [\"HOME\"]}}");
	prof_finish(&p);

	CHECK(prof_names_get(&p.env, "HOME") == 1);
	CHECK(prof_names_get(&p.env, "0x1000") == 0);

	prof_free(&p);
}

static void test_interleaved_merge(void)
{
	/*
	 * Regression: the names set is kept sorted by inserting at
	 * the bsearch position. Appending instead split duplicates
	 * ("open" twice with halved counts).
	 */
	struct Profile p;
	int i;

	prof_init(&p);
	for (i = 0; i < 3; i++) {
		add_json(&p, "{\"message\": {\"func\": \"open\","
			" \"params\": {\"path\": \"/a/e.dat\"}}}");
		add_json(&p, "{\"message\": {\"func\": \"close\","
			" \"params\": {\"fd\": \"3\"}}}");
	}
	prof_finish(&p);

	CHECK(p.functions.count == 2);
	CHECK(prof_names_get(&p.functions, "open") == 3);
	CHECK(prof_names_get(&p.functions, "close") == 3);

	prof_free(&p);
}

static void test_access_get(void)
{
	struct Profile p;

	prof_init(&p);
	add_json(&p, "{\"message\": {\"func\": \"open\","
		" \"params\": {\"path\": \"/a/b.dat\"}}}");
	prof_finish(&p);

	CHECK(prof_access_get(&p, "/a/b.dat") != NULL);
	CHECK(prof_access_get(&p, "/a/missing.dat") == NULL);

	prof_free(&p);
}

static void test_write_classification(void)
{
	struct Profile p;
	struct ProfAccess *a;

	prof_init(&p);
	add_json(&p, "{\"message\": {\"func\": \"creat\","
		" \"params\": {\"path\": \"/a/new.dat\"}}}");
	prof_finish(&p);

	CHECK(p.accesses.count == 1);
	a = &p.accesses.items[0];
	CHECK(a->class_write == 1);

	prof_free(&p);
}

static void test_to_json_shape(void)
{
	struct Profile p;
	JSON_Value *v;
	JSON_Object *root;

	prof_init(&p);
	add_json(&p, "{\"message\": {\"func\": \"open\","
		" \"params\": {\"path\": \"/a/b.dat\"}}}");
	prof_finish(&p);

	v = prof_to_json(&p);
	root = json_value_get_object(v);
	CHECK(root != NULL);
	CHECK(json_object_get_array(root, "functions") != NULL);
	CHECK(json_object_get_array(root, "accesses") != NULL);
	CHECK(json_array_get_count(
		json_object_get_array(root, "accesses")) == 1);
	json_value_free(v);

	prof_free(&p);
}

/* ---- capability scan ---- */

#define ELF_TEST_FILE "prof_test_elf.bin"

static void wr16(unsigned char *p, unsigned int v)
{
	p[0] = (unsigned char)(v & 0xff);
	p[1] = (unsigned char)((v >> 8) & 0xff);
}

static void wr32(unsigned char *p, unsigned int v)
{
	wr16(p, v & 0xffff);
	wr16(p + 2, (v >> 16) & 0xffff);
}

static void wr64(unsigned char *p, unsigned long long v)
{
	wr32(p, (unsigned int)(v & 0xffffffffULL));
	wr32(p + 4, (unsigned int)(v >> 32));
}

/*
 * Minimal ELF64 LE x86-64 with one PT_LOAD segment whose contents
 * are `code`. p_flags selects PF_X.
 */
static int write_test_elf(const char *path, unsigned int p_flags,
			  const unsigned char *code, size_t codelen)
{
	unsigned char buf[256];
	FILE *f;
	size_t code_off = 128;

	memset(buf, 0, sizeof(buf));
	buf[0] = 0x7f;
	buf[1] = 'E';
	buf[2] = 'L';
	buf[3] = 'F';
	buf[4] = 2; /* 64-bit */
	buf[5] = 1; /* LE */
	wr16(buf + 16, 2); /* ET_EXEC */
	wr16(buf + 18, 0x3e); /* EM_X86_64 */
	wr64(buf + 32, 64); /* e_phoff */
	wr16(buf + 52, 64); /* e_ehsize */
	wr16(buf + 54, 56); /* e_phentsize */
	wr16(buf + 56, 1); /* e_phnum */

	/* phdr at 64 */
	wr32(buf + 64, 1); /* PT_LOAD */
	wr32(buf + 68, p_flags);
	wr64(buf + 72, code_off); /* p_offset */
	wr64(buf + 96, codelen); /* p_filesz */

	if (code_off + codelen > sizeof(buf))
		return 0;
	memcpy(buf + code_off, code, codelen);

	f = fopen(path, "wb");
	if (f == NULL)
		return 0;
	fwrite(buf, 1, code_off + codelen, f);
	fclose(f);
	return 1;
}

static void test_capability_syscall_gadget(void)
{
	struct ProfCapability c;
	static const unsigned char code[] = {
		0x90, 0x0f, 0x05, 0x90, /* syscall */
		0x90, 0x0f, 0x05, 0x90  /* syscall */
	};

	CHECK(write_test_elf(ELF_TEST_FILE, 1 /* PF_X */, code,
		sizeof(code)));
	CHECK(prof_capability_scan(ELF_TEST_FILE, &c) == 0);
	CHECK(c.syscall_gadgets == 2);
	CHECK(c.has_syscall_gadget == 1);
	remove(ELF_TEST_FILE);
}

static void test_capability_no_exec_flag(void)
{
	struct ProfCapability c;
	static const unsigned char code[] = { 0x0f, 0x05 };

	/* same bytes, non-executable segment: not a capability */
	CHECK(write_test_elf(ELF_TEST_FILE, 0 /* no PF_X */, code,
		sizeof(code)));
	CHECK(prof_capability_scan(ELF_TEST_FILE, &c) == 0);
	CHECK(c.syscall_gadgets == 0);
	CHECK(c.has_syscall_gadget == 0);
	remove(ELF_TEST_FILE);
}

static void test_capability_unknown_format(void)
{
	struct ProfCapability c;

	CHECK(prof_capability_scan("/dev/null", &c) == 0);
	CHECK(c.syscall_gadgets == 0);
	CHECK(c.has_syscall_gadget == 0);
	CHECK(c.ntdll_imports == 0);
}

int main(void)
{
	printf("profiler aggregate tests:\n");
	TEST(golden_schema);
	TEST(live_schema);
	TEST(banner_skipped);
	TEST(return_summary_skipped);
	TEST(env_deref);
	TEST(interleaved_merge);
	TEST(access_get);
	TEST(write_classification);
	TEST(to_json_shape);

	printf("profiler capability tests:\n");
	TEST(capability_syscall_gadget);
	TEST(capability_no_exec_flag);
	TEST(capability_unknown_format);

	printf("\nPass: %d, Fail: %d (of %d)\n",
		tests_pass, tests_fail, tests_run);
	return tests_fail == 0 ? 0 : 1;
}
