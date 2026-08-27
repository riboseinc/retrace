/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * retrace-enforce (TODO.beyond-libc/01): the generic kernel-
 * enforcement installer. Reads the spec emitted by
 * `retrace-profile enforce`, applies its Landlock ruleset and
 * seccomp floor, and execs the command -- the kernel-filter
 * deployment in one line:
 *
 *   retrace-enforce spec.json -- ./target args...
 *
 * Fail-closed: if a requested plane cannot be installed, the
 * exec never happens (exit 2) -- unless --allow-missing is
 * given for dev workflows.
 *
 * Audited artifacts (01 P2): --audit PATH appends a hash-chained
 * record binding (ts, pid, spec digest, backends, argv) to the
 * trail; --verify-audit PATH replays it. A requested trail that
 * cannot be appended (or whose existing chain is broken)
 * fail-closes the exec.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "enforce_spec.h"
#include "landlock_apply.h"
#include "seccomp_apply.h"
#include "sandbox_apply.h"
#include "artifact_audit.h"

static void enforce_usage(void)
{
	fprintf(stderr,
		"usage: retrace-enforce [--allow-missing] [--audit PATH]"
		" spec.json -- cmd [args...]\n"
		"       retrace-enforce --verify-audit PATH\n"
		"  installs the spec's kernel filters, then execs cmd\n"
		"  --allow-missing: proceed when the kernel lacks a"
		" plane (dev only)\n"
		"  --audit PATH: append a hash-chained record of this\n"
		"    exec (spec digest + backends + argv); a broken\n"
		"    trail refuses the exec\n"
		"  --verify-audit PATH: replay + verify a trail\n");
}

static void spec_backends(const struct enforce_spec *spec, char *out,
	size_t cap)
{
	size_t o = 0;

	out[0] = '\0';
	if (spec->rules_n > 0)
		o += (size_t)snprintf(out + o, cap - o, "landlock");
	if (spec->deny_n > 0)
		o += (size_t)snprintf(out + o, cap - o, "%sseccomp",
			o > 0 ? "+" : "");
	if (spec->sandbox_exec[0] != '\0')
		(void)snprintf(out + o, cap - o, "%ssandbox-exec",
			o > 0 ? "+" : "");
}

int main(int argc, char **argv)
{
	struct enforce_spec spec;
	const char *spec_path = NULL;
	const char *audit_path = NULL;
	const char *verify_path = NULL;
	int allow_missing = 0;
	int i;
	int rc;
	FILE *f;
	char *json;
	long sz;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--allow-missing") == 0) {
			allow_missing = 1;
		} else if (strcmp(argv[i], "--audit") == 0 &&
			   i + 1 < argc) {
			audit_path = argv[++i];
		} else if (strcmp(argv[i], "--verify-audit") == 0 &&
			   i + 1 < argc) {
			verify_path = argv[++i];
		} else if (strcmp(argv[i], "--") == 0) {
			break;
		} else if (argv[i][0] != '-' && spec_path == NULL) {
			spec_path = argv[i];
		} else {
			enforce_usage();
			return 2;
		}
	}
	if (verify_path != NULL) {
		char head[ENFORCE_DIGEST_HEX_MAX];
		long n = enforce_audit_verify(verify_path, head);

		if (n < 0) {
			fprintf(stderr,
				"retrace-enforce: audit trail %s: %s\n",
				verify_path,
				n == -2 ? "chain broken (tampered or torn)"
					: "unreadable");
			return 1;
		}
		printf("retrace-enforce: audit ok: %ld records, head %s\n",
			n, head);
		return 0;
	}
	if (spec_path == NULL || i + 1 >= argc) {
		enforce_usage();
		return 2;
	}
	f = fopen(spec_path, "rb");
	if (f == NULL) {
		perror("spec");
		return 2;
	}
	fseek(f, 0, SEEK_END);
	sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (sz <= 0 || sz > 4 * 1024 * 1024) {
		fclose(f);
		return 2;
	}
	json = malloc((size_t)sz + 1);
	if (json == NULL ||
	    fread(json, 1, (size_t)sz, f) != (size_t)sz) {
		fclose(f);
		return 2;
	}
	fclose(f);
	json[sz] = '\0';
	if (enforce_spec_parse(&spec, json) != 0) {
		fprintf(stderr, "retrace-enforce: spec unparseable\n");
		return 2;
	}

	rc = enforce_landlock_apply(&spec);
	if (rc == 1)
		fprintf(stderr,
			"retrace-enforce: kernel lacks landlock\n");
	if (rc < 0) {
		fprintf(stderr,
			"retrace-enforce: landlock apply failed: %s\n",
			strerror(errno));
		return 2;
	}
	if (rc == 1 && !allow_missing &&
	    spec.sandbox_exec[0] == '\0')
		return 2;

	rc = enforce_seccomp_apply(&spec);
	if (rc == 1)
		fprintf(stderr,
			"retrace-enforce: kernel lacks the seccomp floor\n");
	if (rc < 0) {
		fprintf(stderr,
			"retrace-enforce: seccomp apply failed: %s\n",
			strerror(errno));
		return 2;
	}
	if (rc == 1 && !allow_missing &&
	    spec.sandbox_exec[0] == '\0')
		return 2;

	/*
	 * The audit record lands only when the planes are in force:
	 * it binds the exec to (digest, backends, argv). A
	 * requested-but-unwritable or tampered trail fail-closes
	 * the exec (the evidence discipline).
	 */
	if (audit_path != NULL) {
		char digest[ENFORCE_DIGEST_HEX_MAX];
		const char *alg = NULL;
		char backends[64];

		if (enforce_spec_digest(json, (size_t)sz, digest,
			    &alg) != 0) {
			fprintf(stderr,
				"retrace-enforce: spec digest failed\n");
			return 2;
		}
		spec_backends(&spec, backends, sizeof(backends));
		if (enforce_audit_append(audit_path, (long)time(NULL),
			    (long)getpid(), digest, alg, backends,
			    &argv[i + 1]) != 0) {
			fprintf(stderr,
				"retrace-enforce: audit append failed; refusing exec\n");
			return 2;
		}
	}
	free(json);

	if (spec.sandbox_exec[0] != '\0') {
		static char wrap[16384];

		if (enforce_sandbox_exec_wrap(spec.sandbox_exec,
			    argc - i - 1, &argv[i + 1], wrap,
			    sizeof(wrap)) != 0) {
			fprintf(stderr, "retrace-enforce: sandbox wrap overflow\n");
			return 2;
		}
		{
			char *sh_argv[] = {"/bin/sh", "-c", wrap, NULL};

			execvp("/bin/sh", sh_argv);
			perror("execvp(sh)");
			return 2;
		}
	}

	execvp(argv[i + 1], &argv[i + 1]);
	perror("execvp");
	return 2;
}
