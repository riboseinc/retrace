/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Unit tests for jail emission + the profile JSON inverse
 * (TODO.trace-profile/10).
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "aggregate.h"
#include "jail.h"
#include "match.h"
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

static void add_json(struct Profile *p, const char *entry)
{
	JSON_Value *v = json_parse_string(entry);

	CHECK(v != NULL);
	prof_add_entry(json_value_get_object(v), p);
	json_value_free(v);
}

static void feed(struct Profile *p, const char *const *entries)
{
	size_t i;

	prof_init(p);
	for (i = 0; entries[i] != NULL; i++)
		add_json(p, entries[i]);
	prof_finish(p);
}

/* one observed function, one observed path */
static void test_jail_shape(void)
{
	static const char *const e[] = {
		"{\"message\":{\"func\":\"fopen\",\"params\":{\"path\":\"/data/in.dat\"}}}",
		NULL
	};
	struct Profile p;
	JSON_Value *jc;
	JSON_Object *root;
	JSON_Array *scripts;
	JSON_Object *script;
	JSON_Array *actions;
	JSON_Object *sandbox;
	JSON_Array *allow;

	feed(&p, e);
	CHECK(p.functions.count == 1);
	CHECK(p.accesses.count == 1);

	jc = prof_jail_config(&p, &p, NULL);
	root = json_value_get_object(jc);
	CHECK(root != NULL);

	scripts = json_object_get_array(root, "intercept_scripts");
	CHECK(scripts != NULL);
	CHECK(json_array_get_count(scripts) == 1);

	script = json_array_get_object(scripts, 0);
	CHECK(strcmp(json_object_get_string(script, "func_name"),
		"fopen") == 0);

	actions = json_object_get_array(script, "actions");
	CHECK(actions != NULL);
	CHECK(json_array_get_count(actions) == 2);

	sandbox = json_array_get_object(actions, 0);
	CHECK(strcmp(json_object_get_string(sandbox, "action_name"),
		"sandbox") == 0);
	CHECK(strcmp(json_object_get_string(
		json_array_get_object(actions, 1), "action_name"),
		"call_real") == 0);

	allow = json_object_get_array(
		json_object_get_object(sandbox, "action_params"),
		"allow_paths");
	CHECK(allow != NULL);
	CHECK(json_array_get_count(allow) == 1);
	CHECK(strcmp(json_array_get_string(allow, 0),
		"/data/in.dat") == 0);

	json_value_free(jc);
	prof_free(&p);
}

/* --inside: the allowlist comes from the DECLARED set */
static void test_jail_declared_allowlist(void)
{
	static const char *const observed[] = {
		"{\"message\":{\"func\":\"fopen\",\"params\":{\"path\":\"/esc.dat\"}}}",
		NULL
	};
	static const char *const declared[] = {
		"{\"message\":{\"func\":\"fopen\",\"params\":{\"path\":\"/inside/a.dat\"}}}",
		NULL
	};
	struct Profile obs, ins;
	JSON_Value *jc;
	JSON_Object *root;
	JSON_Object *script;
	JSON_Object *sandbox;
	JSON_Array *allow;

	feed(&obs, observed);
	feed(&ins, declared);

	jc = prof_jail_config(&obs, &ins, NULL);
	root = json_value_get_object(jc);
	script = json_array_get_object(
		json_object_get_array(root, "intercept_scripts"), 0);
	sandbox = json_array_get_object(
		json_object_get_array(script, "actions"), 0);
	allow = json_object_get_array(
		json_object_get_object(sandbox, "action_params"),
		"allow_paths");
	CHECK(allow != NULL);
	CHECK(json_array_get_count(allow) == 1);
	CHECK(strcmp(json_array_get_string(allow, 0), "/inside/a.dat")
		== 0);

	json_value_free(jc);
	prof_free(&obs);
	prof_free(&ins);
}

/* prof_to_json -> prof_from_json round trip: names, counts,
 * access classes and hit counts survive
 */
static void test_profile_round_trip(void)
{
	static const char *const e[] = {
		"{\"message\":{\"func\":\"open\",\"params\":{\"path\":\"/a/x.dat\"}}}",
		"{\"message\":{\"func\":\"open\",\"params\":{\"path\":\"/a/x.dat\"}}}",
		"{\"message\":{\"func\":\"fopen\",\"params\":{\"path\":\"/b/y.dat\"}}}",
		"{\"message\":{\"func\":\"unlink\",\"params\":{\"path\":\"/b/y.dat\"}}}",
		NULL
	};
	struct Profile a, b;
	JSON_Value *v;

	feed(&a, e);
	CHECK(a.functions.count == 3);
	CHECK(a.accesses.count == 2);

	v = prof_to_json(&a);
	CHECK(prof_from_json(json_value_get_object(v), &b) == 0);

	CHECK(b.entries == a.entries);
	CHECK(b.functions.count == 3);
	CHECK(prof_names_get(&b.functions, "open") == 2);
	CHECK(prof_names_get(&b.functions, "fopen") == 1);
	CHECK(prof_names_get(&b.functions, "unlink") == 1);
	CHECK(b.accesses.count == 2);
	{
		struct ProfAccess *x = prof_access_get(&b, "/a/x.dat");
		struct ProfAccess *y = prof_access_get(&b, "/b/y.dat");

		CHECK(x != NULL && x->hits == 2 && x->class_read);
		CHECK(y != NULL && y->class_write);
	}

	json_value_free(v);
	prof_free(&a);
	prof_free(&b);
}

/* the serialized inverse re-emits the same jail as the original */
static void test_jail_from_doc_matches_trace(void)
{
	static const char *const e[] = {
		"{\"message\":{\"func\":\"fopen\",\"params\":{\"path\":\"/data/in.dat\"}}}",
		NULL
	};
	struct Profile from_trace, from_doc;
	JSON_Value *doc = json_value_init_object();
	JSON_Value *j1, *j2;
	char *s1, *s2;
	int same;

	feed(&from_trace, e);
	json_object_set_value(json_value_get_object(doc), "profile",
		prof_to_json(&from_trace));
	CHECK(prof_from_json(
		json_object_get_object(json_value_get_object(doc),
				       "profile"),
		&from_doc) == 0);

	j1 = prof_jail_config(&from_trace, &from_trace, NULL);
	j2 = prof_jail_config(&from_doc, &from_doc, NULL);
	s1 = json_serialize_to_string(j1);
	s2 = json_serialize_to_string(j2);
	same = strcmp(s1, s2) == 0;
	CHECK(same);

	json_free_serialized_string(s1);
	json_free_serialized_string(s2);
	json_value_free(j1);
	json_value_free(j2);
	json_value_free(doc);
	prof_free(&from_trace);
	prof_free(&from_doc);
}

/* --read-only / --decoy / --pin-clock emission */
static void test_jail_opts_emission(void)
{
	static const char *const e[] = {
		"{\"message\":{\"func\":\"fopen\",\"params\":{\"path\":\"/data/in.dat\"}}}",
		NULL
	};
	struct Profile p;
	struct ProfJailOpts opts;
	JSON_Value *jc;
	JSON_Object *root;
	JSON_Array *scripts;
	JSON_Object *params;
	JSON_Array *dc;
	JSON_Object *time_script;
	JSON_Object *mr;

	feed(&p, e);
	memset(&opts, 0, sizeof(opts));
	opts.read_only = 1;
	opts.decoy_dir = "/decoys";
	opts.pin_clock = 1700000000LL;
	opts.pin_clock_set = 1;

	jc = prof_jail_config(&p, &p, &opts);
	root = json_value_get_object(jc);
	scripts = json_object_get_array(root, "intercept_scripts");
	CHECK(json_array_get_count(scripts) == 2); /* fopen + time */

	params = json_object_get_object(
		json_array_get_object(
			json_object_get_array(
				json_array_get_object(scripts, 0),
				"actions"),
			0),
		"action_params");
	dc = json_object_get_array(params, "deny_classes");
	CHECK(dc != NULL);
	CHECK(json_array_get_count(dc) == 1);
	CHECK(strcmp(json_array_get_string(dc, 0), "write") == 0);
	CHECK(strcmp(json_object_get_string(params, "decoy_dir"),
		"/decoys") == 0);

	time_script = json_array_get_object(scripts, 1);
	CHECK(strcmp(json_object_get_string(time_script, "func_name"),
		"time") == 0);
	mr = json_array_get_object(
		json_object_get_array(time_script, "actions"), 0);
	CHECK(strcmp(json_object_get_string(mr, "action_name"),
		"modify_return_value_int") == 0);
	CHECK((long long)json_object_get_number(mr, "new_int")
		== 1700000000LL);

	json_value_free(jc);
	prof_free(&p);
}

/* junk input: NULL root rejected, empty objects yield empty */
static void test_from_json_degenerate(void)
{
	struct Profile p;

	CHECK(prof_from_json(NULL, &p) == -1);
	prof_free(&p);

	{
		JSON_Value *v = json_parse_string(
			"{\"profile\":{\"functions\":[],\"accesses\":[]}}");

		CHECK(prof_from_json(
			json_object_get_object(
				json_value_get_object(v), "profile"),
			&p) == 0);
		CHECK(p.functions.count == 0);
		CHECK(p.accesses.count == 0);
		json_value_free(v);
	}
	prof_free(&p);
}

int main(void)
{
	printf("profile jail tests:\n");
	TEST(jail_shape);
	TEST(jail_declared_allowlist);
	TEST(profile_round_trip);
	TEST(jail_from_doc_matches_trace);
	TEST(jail_opts_emission);
	TEST(from_json_degenerate);

	printf("\nPass: %d, Fail: %d (of %d)\n",
		tests_pass, tests_fail, tests_run);
	return tests_fail == 0 ? 0 : 1;
}
