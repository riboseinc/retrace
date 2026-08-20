/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * retrace-profile -- claims-vs-truth risk profiler
 * (TODO.windows/08).
 *
 * Generates a PROFILE of what a binary does from one or two
 * traces:
 *
 *   --libc <trace>    the retrace libc-layer capture (claims)
 *   --kernel <trace>  the kernel-layer truth (ptrace/eBPF on
 *                     Linux; procmon CSV via
 *                     retrace-procmon2retrace on Windows);
 *                     when present, every access is graded by
 *                     layer provenance and kernel-only accesses
 *                     (invisible to libc) are the risk headline
 *   --binary <path>   static capability scan (syscall gadgets,
 *                     ntdll imports)
 *   --inside <trace>  the DECLARED set (VFS materialize log /
 *                     inside.json): jail allowlist comes from
 *                     here when given -- the observed trace
 *                     would allowlist its own escapes
 *   --jail-out <cfg>  ALSO emit a retrace jail config: per
 *                     observed function, sandbox with the
 *                     allowlist as allow_paths (deny-by-default)
 *                     followed by call_real
 *   -o <profile>      output profile JSON (default stdout)
 *
 * Layer honesty: with --libc only, the profile's coverage
 * header says so -- a libc-only capture cannot rule out
 * sub-libc accesses; pair it with --kernel (and check the
 * binary's static capabilities) for risk work.
 */

#include "aggregate.h"
#include "capability.h"
#include "match.h"
#include "stream.h"
#include "parson.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_file(const char *path, size_t *len_out)
{
	FILE *f = fopen(path, "rb");
	long sz;
	char *buf = NULL;

	if (f == NULL)
		return NULL;
	if (fseek(f, 0, SEEK_END) != 0)
		goto fail;
	sz = ftell(f);
	if (sz < 0)
		goto fail;
	if (fseek(f, 0, SEEK_SET) != 0)
		goto fail;
	buf = (char *)malloc((size_t)sz + 1);
	if (buf == NULL)
		goto fail;
	if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
		free(buf);
		buf = NULL;
	}
	fclose(f);
	if (buf != NULL) {
		buf[sz] = '\0';
		*len_out = (size_t)sz;
	}
	return buf;

fail:
	fclose(f);
	return NULL;
}

struct ProfFeed {
	struct Profile prof;
	size_t skipped;
};

static void feed_cb(JSON_Object *entry, void *ctx)
{
	struct ProfFeed *feed = (struct ProfFeed *)ctx;

	prof_add_entry(entry, &feed->prof);
}

static int load_profile(const char *path, struct ProfFeed *feed)
{
	char *text;
	size_t len = 0;

	prof_init(&feed->prof);
	feed->skipped = 0;
	text = read_file(path, &len);
	if (text == NULL)
		return -1;
	(void)corr_stream_scan(text, len, feed_cb, feed, &feed->skipped);
	free(text);
	prof_finish(&feed->prof);
	return 0;
}

/*
 * Delta: grade every kernel access by whether the libc layer
 * claims it (same normalized path). Kernel-only = sub-libc
 * surface = risk.
 */
struct Delta {
	size_t agreed;
	size_t libc_only;
	size_t kernel_only;
	JSON_Value *risk_arr; /* kernel-only accesses */
};

static void delta_compute(const struct ProfFeed *libc,
			  const struct ProfFeed *kernel, struct Delta *d)
{
	size_t i;
	JSON_Array *arr;

	d->agreed = 0;
	d->libc_only = libc->prof.accesses.count;
	d->kernel_only = 0;
	d->risk_arr = json_value_init_array();
	arr = json_value_get_array(d->risk_arr);

	for (i = 0; i < kernel->prof.accesses.count; i++) {
		const struct ProfAccess *ka = &kernel->prof.accesses.items[i];
		JSON_Object *root = NULL;

		if (prof_access_get((struct Profile *)&libc->prof,
				    ka->path) != NULL) {
			d->agreed++;
			d->libc_only--;
			continue;
		}
		d->kernel_only++;
		{
			JSON_Value *o = json_value_init_object();

			root = json_value_get_object(o);
			json_object_set_string(root, "path", ka->path);
			json_object_set_number(root, "hits",
				(double)ka->hits);
			json_array_append_value(arr, o);
		}
	}
}

/*
 * The jail config: every function observed in the trace gets a
 * script whose first action is sandbox with the allowlist as
 * allow_paths, followed by call_real so allowed paths execute.
 * A path not on the list is denied before libc sees it.
 *
 * funcs_src scopes the scripts, paths_src supplies the
 * allowlist: with --inside these differ (declared set vs
 * observed functions). Scoped to observed functions rather than
 * a wildcard: a '*' jail also jails the dynamic loader's own
 * file opens and kills process startup. Re-profile to extend
 * coverage.
 */
static JSON_Value *jail_config(const struct Profile *funcs_src,
			       const struct Profile *paths_src)
{
	JSON_Value *v = json_value_init_object();
	JSON_Object *root = json_value_get_object(v);
	JSON_Value *scripts = json_value_init_array();
	size_t i, f;

	for (f = 0; f < funcs_src->functions.count; f++) {
		JSON_Value *script = json_value_init_object();
		JSON_Object *script_o = json_value_get_object(script);
		JSON_Value *actions = json_value_init_array();
		JSON_Value *action = json_value_init_object();
		JSON_Object *action_o = json_value_get_object(action);
		JSON_Value *params = json_value_init_object();
		JSON_Object *params_o = json_value_get_object(params);
		JSON_Value *allow = json_value_init_array();

		for (i = 0; i < paths_src->accesses.count; i++)
			json_array_append_string(
				json_value_get_array(allow),
				paths_src->accesses.items[i].path);

		json_object_set_value(params_o, "allow_paths", allow);
		json_object_set_string(action_o, "action_name",
			"sandbox");
		json_object_set_value(action_o, "action_params", params);
		json_array_append_value(json_value_get_array(actions),
			action);

		/* allowed paths must still reach the real call */
		{
			JSON_Value *cr = json_value_init_object();

			json_object_set_string(json_value_get_object(cr),
				"action_name", "call_real");
			json_array_append_value(
				json_value_get_array(actions), cr);
		}

		json_object_set_string(script_o, "func_name",
			funcs_src->functions.names[f]);
		json_object_set_value(script_o, "actions", actions);
		json_array_append_value(json_value_get_array(scripts),
			script);
	}
	json_object_set_value(root, "intercept_scripts", scripts);
	return v;
}

static void capability_to_json(JSON_Object *root,
			       const struct ProfCapability *c)
{
	JSON_Value *o = json_value_init_object();
	JSON_Object *cap = json_value_get_object(o);
	JSON_Value *arr = json_value_init_array();
	size_t i;

	json_object_set_number(cap, "raw_syscall_gadgets",
		(double)c->syscall_gadgets);
	json_object_set_number(cap, "ntdll_imports",
		(double)c->ntdll_imports);
	for (i = 0; i < c->ntdll_imports && i < 16; i++)
		json_array_append_string(json_value_get_array(arr),
			c->ntdll_names[i]);
	json_object_set_value(cap, "ntdll_names", arr);
	json_object_set_value(root, "static_capability", o);
}

int main(int argc, char **argv)
{
	const char *libc_path = NULL;
	const char *kernel_path = NULL;
	const char *binary_path = NULL;
	const char *out_path = NULL;
	const char *jail_path = NULL;
	const char *inside_path = NULL;
	struct ProfFeed libc_feed, kernel_feed, inside_feed;
	struct ProfCapability cap;
	int have_kernel = 0;
	int have_inside = 0;
	int have_cap = 0;
	JSON_Value *out;
	JSON_Object *root;
	int i;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--libc") == 0 && i + 1 < argc)
			libc_path = argv[++i];
		else if (strcmp(argv[i], "--kernel") == 0 && i + 1 < argc)
			kernel_path = argv[++i];
		else if (strcmp(argv[i], "--binary") == 0 && i + 1 < argc)
			binary_path = argv[++i];
		else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
			out_path = argv[++i];
		else if (strcmp(argv[i], "--jail-out") == 0 && i + 1 < argc)
			jail_path = argv[++i];
		else if (strcmp(argv[i], "--inside") == 0 && i + 1 < argc)
			inside_path = argv[++i];
		else {
			fprintf(stderr,
"Usage: retrace-profile --libc <trace.json> [--kernel <truth.json>]\n"
"                       [--inside <declared.json>] [--binary <target>]\n"
"                       [--jail-out <jail.json>] [-o profile.json]\n");
			return 2;
		}
	}
	if (libc_path == NULL) {
		fprintf(stderr, "retrace-profile: --libc is required\n");
		return 2;
	}
	if (load_profile(libc_path, &libc_feed) != 0) {
		fprintf(stderr, "retrace-profile: cannot read %s\n",
			libc_path);
		return 2;
	}
	if (inside_path != NULL) {
		if (load_profile(inside_path, &inside_feed) != 0) {
			fprintf(stderr,
				"retrace-profile: cannot read %s\n",
				inside_path);
			return 2;
		}
		have_inside = 1;
	}
	if (kernel_path != NULL) {
		if (load_profile(kernel_path, &kernel_feed) != 0) {
			fprintf(stderr,
				"retrace-profile: cannot read %s\n",
				kernel_path);
			return 2;
		}
		have_kernel = 1;
	}
	if (binary_path != NULL) {
		if (prof_capability_scan(binary_path, &cap) != 0) {
			fprintf(stderr,
				"retrace-profile: cannot read %s\n",
				binary_path);
			return 2;
		}
		have_cap = 1;
	}

	out = json_value_init_object();
	root = json_value_get_object(out);
	json_object_set_value(root, "profile",
		prof_to_json(&libc_feed.prof));

	{
		JSON_Value *cov = json_value_init_object();
		JSON_Object *cov_o = json_value_get_object(cov);

		json_object_set_string(cov_o, "libc_layer", "captured");
		json_object_set_string(cov_o, "kernel_layer",
			have_kernel ? "captured" : "ABSENT");
		json_object_set_value(root, "coverage", cov);
	}

	if (have_kernel) {
		struct Delta d;

		delta_compute(&libc_feed, &kernel_feed, &d);
		{
			JSON_Value *rv = json_value_init_object();
			JSON_Object *ro = json_value_get_object(rv);

			json_object_set_number(ro, "agreed",
				(double)d.agreed);
			json_object_set_number(ro, "libc_only",
				(double)d.libc_only);
			json_object_set_number(ro, "kernel_only",
				(double)d.kernel_only);
			json_object_set_string(ro, "verdict",
				d.kernel_only > 0 ?
				"SUBLIBC_ACCESS_FOUND" : "clean");
			json_object_set_value(ro, "kernel_only_accesses",
				d.risk_arr);
			json_object_set_value(root, "risk", rv);
		}
		/* risk_arr ownership moved into the tree above */
	}
	if (have_cap)
		capability_to_json(root, &cap);

	if (jail_path != NULL) {
		/*
		 * Allowlist source: the DECLARED set when --inside was
		 * given (the observed trace would allowlist its own
		 * escapes), otherwise the observed accesses (self-jail
		 * of a known-good run).
		 */
		const struct ProfFeed *allow_src = have_inside ?
			&inside_feed : &libc_feed;
		JSON_Value *jc = jail_config(&libc_feed.prof,
			&allow_src->prof);
		FILE *jf = fopen(jail_path, "w");

		if (jf == NULL) {
			fprintf(stderr,
				"retrace-profile: cannot write %s\n",
				jail_path);
			return 2;
		}
		fprintf(jf, "%s\n",
			json_serialize_to_string_pretty(jc));
		fclose(jf);
		json_value_free(jc);
		fprintf(stderr,
			"retrace-profile: jail config -> %s (allow: %s)\n",
			jail_path,
			have_inside ? "declared (--inside)" : "observed");
	}

	{
		char *ser = json_serialize_to_string_pretty(out);

		if (out_path != NULL) {
			FILE *of = fopen(out_path, "w");

			if (of == NULL) {
				fprintf(stderr,
					"retrace-profile: cannot write %s\n",
					out_path);
				return 2;
			}
			fprintf(of, "%s\n", ser);
			fclose(of);
		} else {
			printf("%s\n", ser);
		}
		json_free_serialized_string(ser);
	}

	fprintf(stderr,
		"retrace-profile: %zu entries, %zu functions, %zu paths%s\n",
		libc_feed.prof.entries, libc_feed.prof.functions.count,
		libc_feed.prof.accesses.count,
		have_kernel ? "; kernel truth: present" :
		"; kernel truth: ABSENT (libc-only capture)");
	return 0;
}
