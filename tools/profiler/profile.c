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

#include <otlp-c/exporter.h>
#include <otlp-c/metric.h>
#include "capability.h"
#include "capture.h"
#include "diff.h"
#include "jail.h"
#include "harden.h"
#include "validate.h"
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
 * Load a profile DOC ({"profile": ...}) or a trace. Doc form
 * wins when the root object carries "profile"; JSONL/array
 * traces fall through to the scanner. The jail subcommand
 * accepts either artifact.
 */
static int load_any(const char *path, struct ProfFeed *feed)
{
	char *text;
	size_t len = 0;
	JSON_Value *v;
	JSON_Object *root;

	prof_init(&feed->prof);
	feed->skipped = 0;
	text = read_file(path, &len);
	if (text == NULL)
		return -1;

	v = json_parse_string_with_comments(text);
	root = v != NULL ? json_value_get_object(v) : NULL;
	if (root != NULL &&
	    json_object_get_object(root, "profile") != NULL) {
		int rc = prof_from_json(
			json_object_get_object(root, "profile"),
			&feed->prof);

		json_value_free(v);
		free(text);
		return rc;
	}
	if (v != NULL)
		json_value_free(v);

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

/*
 * `retrace-profile diff baseline.json candidate.json [--json]`:
 * drift report between two profiles (TODO.trace-profile/02).
 * Exit 1 when drift exists (CI-able).
 */
static int diff_mode(int argc, char **argv)
{
	const char *baseline_path = NULL;
	const char *candidate_path = NULL;
	int json_out = 0;
	struct ProfFeed baseline, candidate;
	struct ProfDiff d;
	int drift;
	int i;

	for (i = 2; i < argc; i++) {
		if (strcmp(argv[i], "--json") == 0)
			json_out = 1;
		else if (argv[i][0] == '-' && argv[i][1] != '\0') {
			fprintf(stderr,
"Usage: retrace-profile diff <baseline.json> <candidate.json> [--json]\n");
			return 2;
		} else if (baseline_path == NULL)
			baseline_path = argv[i];
		else if (candidate_path == NULL)
			candidate_path = argv[i];
	}
	if (baseline_path == NULL || candidate_path == NULL) {
		fprintf(stderr,
"Usage: retrace-profile diff <baseline.json> <candidate.json> [--json]\n");
		return 2;
	}
	if (load_any(baseline_path, &baseline) != 0) {
		fprintf(stderr, "retrace-profile: cannot read %s\n",
			baseline_path);
		return 2;
	}
	if (load_any(candidate_path, &candidate) != 0) {
		fprintf(stderr, "retrace-profile: cannot read %s\n",
			candidate_path);
		return 2;
	}

	prof_diff_init(&d);
	drift = prof_diff_compute(&baseline.prof, &candidate.prof, &d);

	if (json_out) {
		char *ser;

		{
			JSON_Value *v = prof_diff_to_json(&d);

			ser = json_serialize_to_string_pretty(v);
			json_value_free(v);
		}
		printf("%s\n", ser);
		json_free_serialized_string(ser);
	} else {
		size_t k;

		printf("profile-diff: baseline %zu entries, candidate %zu entries\n",
			baseline.prof.entries, candidate.prof.entries);
		for (k = 0; k < d.count; k++) {
			const struct ProfPathChange *c = &d.changes[k];

			if (c->class_from < 0)
				printf("  + %s (added, %s, %zu hits)\n",
					c->path,
					corr_class_str((enum CorrClass)
						c->class_to),
					c->hits_to);
			else if (c->class_to < 0)
				printf("  - %s (removed)\n", c->path);
			else
				printf("  ! %s (%s -> %s)\n", c->path,
					corr_class_str((enum CorrClass)
						c->class_from),
					corr_class_str((enum CorrClass)
						c->class_to));
		}
		for (k = 0; k < d.new_functions_cnt; k++)
			printf("  + fn %s (new)\n", d.new_functions[k]);
		for (k = 0; k < d.new_env_cnt; k++)
			printf("  + env %s (new -- supply-chain signal)\n",
				d.new_env[k]);
		for (k = 0; k < d.new_net_cnt; k++)
			printf("  + net %s (new -- supply-chain signal)\n",
				d.new_net[k]);
		printf("%s\n", drift ?
			"profile-diff: DRIFT FOUND" :
			"profile-diff: no drift");
	}

	prof_diff_free(&d);
	prof_free(&baseline.prof);
	prof_free(&candidate.prof);
	return drift ? 1 : 0;
}

/*
 * `retrace-profile jail <profile.json> [--inside <declared.json>]
 *   [-o <jail.json>]` (TODO.trace-profile/10): emit a jail
 * config from an existing profile -- the "update the jail" step
 * of the upgrade story (profile old -> upgrade -> profile new ->
 * diff -> jail). Input may be a profile doc or a trace; the
 * allowlist comes from --inside when given (the declared set --
 * the observed trace would allowlist its own escapes), else from
 * the observed accesses (self-jail of a known-good run).
 */
static int jail_mode(int argc, char **argv)
{
	const char *in_path = NULL;
	const char *inside_path = NULL;
	const char *out_path = NULL;
	struct ProfFeed feed, inside_feed;
	struct ProfJailOpts jopts;
	const struct Profile *allow_src;
	JSON_Value *jc;
	int i;

	memset(&jopts, 0, sizeof(jopts));

	for (i = 2; i < argc; i++) {
		if (strcmp(argv[i], "--inside") == 0 && i + 1 < argc)
			inside_path = argv[++i];
		else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
			out_path = argv[++i];
		else if (strcmp(argv[i], "--read-only") == 0) {
			jopts.read_only = 1;
		} else if (strcmp(argv[i], "--decoy") == 0 &&
			   i + 1 < argc) {
			jopts.decoy_dir = argv[++i];
		} else if (strcmp(argv[i], "--pin-clock") == 0 &&
			   i + 1 < argc) {
			jopts.pin_clock = atoll(argv[++i]);
			jopts.pin_clock_set = 1;
		} else if (argv[i][0] == '-' && argv[i][1] != '\0') {
			fprintf(stderr,
	"Usage: retrace-profile jail <profile.json> [--inside d.json]\n"
	"        [--read-only] [--decoy <dir>] [--pin-clock <epoch>]\n"
	"        [-o jail.json]\n");
			return 2;
		} else if (in_path == NULL) {
			in_path = argv[i];
		}
	}
	if (in_path == NULL) {
		fprintf(stderr,
	"Usage: retrace-profile jail <profile.json> [--inside d.json]\n"
	"        [--read-only] [--decoy <dir>] [--pin-clock <epoch>]\n"
	"        [-o jail.json]\n");
		return 2;
	}

	if (load_any(in_path, &feed) != 0) {
		fprintf(stderr, "retrace-profile: cannot read %s\n",
			in_path);
		return 2;
	}
	if (feed.prof.functions.count == 0) {
		fprintf(stderr,
		"retrace-profile: no observed functions in %s\n",
			in_path);
		prof_free(&feed.prof);
		return 2;
	}

	allow_src = &feed.prof;
	if (inside_path != NULL) {
		if (load_any(inside_path, &inside_feed) != 0) {
			fprintf(stderr,
				"retrace-profile: cannot read %s\n",
				inside_path);
			prof_free(&feed.prof);
			return 2;
		}
		allow_src = &inside_feed.prof;
	}

	jc = prof_jail_config(&feed.prof, allow_src, &jopts);
	{
		char *ser = json_serialize_to_string_pretty(jc);

		if (out_path != NULL) {
			FILE *of = fopen(out_path, "w");

			if (of == NULL) {
				fprintf(stderr,
				"retrace-profile: cannot write %s\n",
					out_path);
				json_free_serialized_string(ser);
				json_value_free(jc);
				prof_free(&feed.prof);
				if (inside_path != NULL)
					prof_free(&inside_feed.prof);
				return 2;
			}
			fprintf(of, "%s\n", ser);
			fclose(of);
		} else {
			printf("%s\n", ser);
		}
		json_free_serialized_string(ser);
	}
	json_value_free(jc);

	fprintf(stderr,
"retrace-profile jail: %zu function(s) jailed, %zu allowed path(s) (allow: %s)\n",
		feed.prof.functions.count, allow_src->accesses.count,
		inside_path != NULL ? "declared (--inside)" : "observed");
	prof_free(&feed.prof);
	if (inside_path != NULL)
		prof_free(&inside_feed.prof);
	return 0;
}

/*
 * `retrace-profile capture [-o profile.json] [--inside d.json]
 *   [--jail-out j.json] [-- cmd args...]` -- the whole recipe-34
 * flow in one command: run the command under the preload, trace
 * it, reduce to a profile, optionally emit the jail
 * (TODO.trace-profile/03).
 */
static int capture_mode(int argc, char **argv)
{
	const char *out_path = NULL;
	const char *jail_path = NULL;
	const char *inside_path = NULL;
	char trace_path[1024];
	const char *lib;
	struct ProfFeed feed, inside_feed;
	int cmd_start = -1;
	int i;
	int rc;

	for (i = 2; i < argc; i++) {
		if (strcmp(argv[i], "--") == 0) {
			cmd_start = i + 1;
			break;
		} else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
			out_path = argv[++i];
		} else if (strcmp(argv[i], "--jail-out") == 0 &&
			   i + 1 < argc) {
			jail_path = argv[++i];
		} else if (strcmp(argv[i], "--inside") == 0 &&
			   i + 1 < argc) {
			inside_path = argv[++i];
		} else {
			fprintf(stderr,
"Usage: retrace-profile capture [-o profile.json] [--inside d.json]\n"
"                              [--jail-out jail.json] -- cmd [args...]\n");
			return 2;
		}
	}
	if (cmd_start < 0 || argv[cmd_start] == NULL) {
		fprintf(stderr,
"retrace-profile capture: no command after --\n");
		return 2;
	}

	lib = prof_capture_find_lib();
	if (lib == NULL) {
		fprintf(stderr,
"retrace-profile capture: libretrace not found; set RETRACE_V2_LIB\n");
		return 2;
	}

	/*
	 * Default config: the file/env/net function set. A wildcard
	 * would trace printf-family variadics too -- noisy for a
	 * profile and fragile on some platforms. An explicit
	 * RETRACE_JSON_CONFIG always wins.
	 */
	if (getenv("RETRACE_JSON_CONFIG") == NULL) {
		static const char *const prof_funcs[] = {
			"open", "openat", "close", "fopen", "fclose",
			"fread", "fwrite", "stat", "lstat", "fstatat",
			"access", "faccessat", "readlink", "unlink",
			"rename", "mkdir", "rmdir", "creat", "truncate",
			"read", "write", "getenv", "setenv", "putenv",
			"connect", "send", "recv", "dlopen", NULL
		};
		size_t k;
		FILE *cf;

		if (prof_capture_temp(trace_path, sizeof(trace_path),
				      "retrace-capture") != 0) {
			fprintf(stderr,
	"retrace-profile capture: temp file failed\n");
			return 2;
		}
		cf = fopen(trace_path, "w");
		if (cf == NULL) {
			remove(trace_path);
			return 2;
		}
		fputs("{\"intercept_scripts\":[", cf);
		for (k = 0; prof_funcs[k] != NULL; k++)
			fprintf(cf,
"%s{\"func_name\":\"%s\",\"actions\":["
"{\"action_name\":\"log_params\"},"
"{\"action_name\":\"call_real\"}]}",
				k ? "," : "", prof_funcs[k]);
		fputs("]}\n", cf);
		fclose(cf);
		prof_capture_setenv("RETRACE_JSON_CONFIG", trace_path);
	}

	if (prof_capture_temp(trace_path, sizeof(trace_path),
			      "retrace-trace") != 0) {
		fprintf(stderr, "retrace-profile capture: temp file failed\n");
		return 2;
	}

	fprintf(stderr, "retrace-profile capture: lib=%s trace=%s\n",
		lib, trace_path);
	rc = prof_capture_run(&argv[cmd_start], lib, trace_path);
	if (rc < 0) {
		fprintf(stderr, "retrace-profile capture: launch failed\n");
		remove(trace_path);
		return 2;
	}

	if (load_profile(trace_path, &feed) != 0) {
		fprintf(stderr, "retrace-profile capture: no trace\n");
		remove(trace_path);
		return 2;
	}

	{
		JSON_Value *out = json_value_init_object();
		JSON_Object *root = json_value_get_object(out);
		JSON_Value *cov = json_value_init_object();

		json_object_set_value(root, "profile",
			prof_to_json(&feed.prof));
		json_object_set_string(json_value_get_object(cov),
			"libc_layer", "captured");
		json_object_set_string(json_value_get_object(cov),
			"kernel_layer", "ABSENT");
		json_object_set_value(root, "coverage", cov);

		{
			char *ser = json_serialize_to_string_pretty(out);

			if (out_path != NULL) {
				FILE *of = fopen(out_path, "w");

				if (of == NULL) {
					fprintf(stderr,
"retrace-profile: cannot write %s\n", out_path);
					return 2;
				}
				fprintf(of, "%s\n", ser);
				fclose(of);
			} else {
				printf("%s\n", ser);
			}
			json_free_serialized_string(ser);
		}
		json_value_free(out);
	}

	if (jail_path != NULL) {
		struct ProfFeed *allow_src = &feed;

		if (inside_path != NULL &&
		    load_profile(inside_path, &inside_feed) == 0)
			allow_src = &inside_feed;
		{
			JSON_Value *jc = prof_jail_config(&feed.prof,
				&allow_src->prof, NULL);
			FILE *jf = fopen(jail_path, "w");

			if (jf == NULL) {
				fprintf(stderr,
"retrace-profile: cannot write %s\n", jail_path);
				return 2;
			}
			fprintf(jf, "%s\n",
				json_serialize_to_string_pretty(jc));
			fclose(jf);
			json_value_free(jc);
		}
	}

	fprintf(stderr,
"retrace-profile capture: %zu entries, %zu functions, %zu paths (cmd exit %d)\n",
		feed.prof.entries, feed.prof.functions.count,
		feed.prof.accesses.count, rc);

	remove(trace_path);
	return rc;
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

	if (argc >= 2 && strcmp(argv[1], "diff") == 0)
		return diff_mode(argc, argv);
	if (argc >= 2 && strcmp(argv[1], "capture") == 0)
		return capture_mode(argc, argv);
	if (argc >= 2 && strcmp(argv[1], "jail") == 0)
		return jail_mode(argc, argv);
	if (argc >= 2 && strcmp(argv[1], "export") == 0) {
		/*
		 * Wave A (TODO.trace-profile/30): timings as OTLP
		 * histogram metrics (one metric per function, every
		 * real sample recorded), posted via the vendored
		 * otlp-c client.
		 */
		const char *hp = NULL;
		const char *endpoint = NULL;
		struct ProfFeed xf;
		otlp_exporter_t *exp;
		otlp_exporter_opts_t opts;
		size_t ti;
		int xi;

		for (xi = 2; xi < argc; xi++) {
			if (strcmp(argv[xi], "--endpoint") == 0 &&
			    xi + 1 < argc)
				endpoint = argv[++xi];
			else if (argv[xi][0] != '-')
				hp = argv[xi];
			else {
				fprintf(stderr,
	"Usage: retrace-profile export <profile.json> --endpoint URL\n");
				return 2;
			}
		}
		if (hp == NULL || endpoint == NULL) {
			fprintf(stderr,
	"Usage: retrace-profile export <profile.json> --endpoint URL\n");
			return 2;
		}
		if (load_any(hp, &xf) != 0) {
			fprintf(stderr,
				"retrace-profile: cannot read %s\n", hp);
			return 2;
		}
		memset(&opts, 0, sizeof(opts));
		opts.endpoint = endpoint;
		opts.service_name = "retrace";
		exp = otlp_exporter_create(&opts);
		if (exp == NULL) {
			fprintf(stderr,
		"retrace-profile export: exporter create failed\n");
			return 2;
		}
		/* Honest metrics from the profile's AGGREGATES (raw
		 * samples are not serialized): gauge per stat,
		 * counter for calls. No fabricated distributions.
		 */
		for (ti = 0; ti < xf.prof.timings.count; ti++) {
			const struct ProfTiming *t =
				&xf.prof.timings.items[ti];
			static const struct {
				const char *name;
				double v;
			} stats[] = {
				{ "retrace.call_p99_us",
				  0 /* filled below */ },
				{ "retrace.call_max_us", 0 },
				{ "retrace.call_total_us", 0 },
			};
			double vals[3];
			int st;

			vals[0] = prof_timing_p99(
				(struct ProfTiming *)(size_t)t);
			vals[1] = t->max_us;
			vals[2] = t->total_us;
			for (st = 0; st < 3; st++) {
				otlp_metric_t *m = otlp_metric_create(
					OTLP_METRIC_GAUGE, stats[st].name,
					"us", "per-function call stat",
					NULL, 0);

				if (m == NULL)
					break;
				otlp_metric_set_attribute_string(m,
					"retrace.func", t->func);
				otlp_metric_record(m, vals[st]);
				otlp_exporter_emit_metric_move(exp, m);
			}
			{
				otlp_metric_t *m = otlp_metric_create(
					OTLP_METRIC_COUNTER,
					"retrace.call_count", "1",
					"calls per function", NULL, 0);

				if (m != NULL) {
					otlp_metric_set_attribute_string(m,
						"retrace.func", t->func);
					otlp_metric_record(m,
						(double)t->calls);
					otlp_exporter_emit_metric_move(exp,
						m);
				}
			}
		}
		otlp_exporter_flush(exp);
		otlp_exporter_shutdown(exp);
		otlp_exporter_free(exp);
		{
			size_t tf = xf.prof.timings.count;

			prof_free(&xf.prof);
			fprintf(stderr,
	"retrace-profile export: %zu timing functions -> %s\n",
				tf, endpoint);
		}
		return 0;
	}
	if (argc >= 2 && strcmp(argv[1], "harden") == 0) {
		const char *hp = NULL;
		const char *ho = NULL;
		struct ProfFeed hf;
		FILE *hf_out = stdout;
		int hi;

		for (hi = 2; hi < argc; hi++) {
			if (strcmp(argv[hi], "-o") == 0 && hi + 1 < argc)
				ho = argv[++hi];
			else if (argv[hi][0] != '-')
				hp = argv[hi];
			else {
				fprintf(stderr,
	"Usage: retrace-profile harden <profile.json> [-o compose.yaml]\n");
				return 2;
			}
		}
		if (hp == NULL) {
			fprintf(stderr,
	"Usage: retrace-profile harden <profile.json> [-o compose.yaml]\n");
			return 2;
		}
		if (load_any(hp, &hf) != 0) {
			fprintf(stderr,
				"retrace-profile: cannot read %s\n", hp);
			return 2;
		}
		if (ho != NULL) {
			hf_out = fopen(ho, "w");
			if (hf_out == NULL) {
				fprintf(stderr,
				"retrace-profile: cannot write %s\n", ho);
				return 2;
			}
		}
		prof_harden_compose(&hf.prof, hf_out);
		if (hf_out != stdout)
			fclose(hf_out);
		prof_free(&hf.prof);
		fprintf(stderr,
	"retrace-profile harden: compose fragment written\n");
		return 0;
	}
	if (argc >= 2 && strcmp(argv[1], "validate") == 0) {
		char err[2048];
		int n;

		if (argc != 3) {
			fprintf(stderr,
"Usage: retrace-profile validate <profile.json>\n");
			return 2;
		}
		n = prof_validate_file(argv[2], err, sizeof(err));
		if (n == 0) {
			printf("profile: valid\n");
			return 0;
		}
		if (n < 0) {
			printf("profile: NOT PARSEABLE\n%s\n", err);
			return 1;
		}
		printf("profile: %d violation%s\n%s\n", n,
			n == 1 ? "" : "s", err);
		return 1;
	}
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
	if (load_any(libc_path, &libc_feed) != 0) {
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

	/*
	 * Declared-set grading (TODO.trace-profile/19): observed
	 * accesses NOT covered by --inside are confinement
	 * violations (packaging audit headline).
	 */
	if (have_inside) {
		size_t vi;
		size_t violations = 0;

		for (vi = 0; vi < libc_feed.prof.accesses.count; vi++) {
			const struct ProfAccess *a =
				&libc_feed.prof.accesses.items[vi];

			if (prof_access_get(&inside_feed.prof, a->path)
				== NULL) {
				if (violations == 0)
					fprintf(stderr,
"retrace-profile: DECLARED-SET VIOLATIONS (observed, not granted):\n");
				fprintf(stderr, "  %s (%s)\n", a->path,
					a->class_write ? "write" :
					a->class_read ? "read" : "probe");
				violations++;
			}
		}
		if (violations == 0)
			fprintf(stderr,
"retrace-profile: declared set covers all observed accesses\n");
	}

	if (jail_path != NULL) {
		/*
		 * Allowlist source: the DECLARED set when --inside was
		 * given (the observed trace would allowlist its own
		 * escapes), otherwise the observed accesses (self-jail
		 * of a known-good run).
		 */
		const struct ProfFeed *allow_src = have_inside ?
			&inside_feed : &libc_feed;
		JSON_Value *jc = prof_jail_config(&libc_feed.prof,
			&allow_src->prof, NULL);
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
