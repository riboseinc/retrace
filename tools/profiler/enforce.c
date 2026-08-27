/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * enforce mode (TODO.beyond-libc/01): the kernel-enforcement
 * compiler. The declared-set (profile accesses, or --inside)
 * that feeds the userspace jail feeds the kernel filters too --
 * one input, two enforcement planes, graded against each other.
 *
 * The Landlock rules: read-class paths get rd + x (the dynamic
 * loader maps libraries PROT_EXEC, which the kernel gates on
 * EXECUTE -- at Landlock granularity read-to-execute is the
 * loader's reality), write-class adds wr. System roots under
 * which anything executes are added read+exec so the binary and
 * its libraries can start at all; the --exec path gets execute
 * explicitly. The seccomp floor: the unsafe syscall classes
 * (sockets, ptrace, mounts, module loading, ...) denied unless
 * the profile observed the corresponding libc call -- proof of
 * use beats suspicion.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aggregate.h"
#include "enforce.h"
#include "prof_feed.h"
#include "parson.h"

static const char *const g_sys_prefixes[] = {
	"/usr", "/lib", "/lib64", "/bin", "/sbin", "/opt",
	"/etc/ld.so.cache", "/etc/ld.so.preload", "/proc/self",
	NULL,
};

static const char *const g_unsafe_syscalls[] = {
	"socket", "socketpair", "connect", "accept", "accept4",
	"sendto", "recvfrom", "bind", "listen", "shutdown",
	"ptrace", "mount", "umount2", "reboot", "swapon",
	"swapoff", "setuid", "setgid", "unshare", "setns", "bpf",
	"keyctl", "request_key", "add_key", "pivot_root",
	"kexec_load", "perf_event_open", "fanotify_init",
	"userfaultfd", "init_module", "finit_module",
	"delete_module", NULL,
};

static void usage(void)
{
	fprintf(stderr,
"Usage: retrace-profile enforce <profile.json> [--inside d.json]\n"
"        [--backend landlock|seccomp|sandbox-exec|appcontainer|all]\n"
"        [--exec <path>]\n"
"        [-o spec.json]\n");
}

static int profile_uses(const struct Profile *p, const char *fn)
{
	size_t i;

	for (i = 0; i < p->functions.count; i++) {
		if (strcmp(p->functions.names[i], fn) == 0)
			return 1;
	}
	return 0;
}

static JSON_Value *build_spec(const struct Profile *allow_src,
	const struct Profile *usage_src, const char *exec_path,
	int want_landlock, int want_seccomp, int want_sb,
	int want_ac)
{
	JSON_Value *root_v = json_value_init_object();
	JSON_Object *root = json_value_get_object(root_v);
	size_t i;

	json_object_set_boolean(root, "no_new_privs", 1);

	if (want_landlock) {
		JSON_Value *ll_v = json_value_init_object();
		JSON_Object *ll = json_value_get_object(ll_v);
		JSON_Value *rules_v = json_value_init_array();
		JSON_Array *rules = json_value_get_array(rules_v);

		json_object_set_value(ll, "rules", rules_v);
		json_object_set_value(root, "landlock", ll_v);

		for (i = 0; i < allow_src->accesses.count; i++) {
			const struct ProfAccess *a =
				&allow_src->accesses.items[i];
			JSON_Value *r = json_value_init_object();
			JSON_Object *ro = json_value_get_object(r);
			JSON_Value *acc_v = json_value_init_array();
			JSON_Array *acc = json_value_get_array(acc_v);

			json_object_set_string(ro, "path", a->path);
			json_array_append_string(acc, "rd");
			if (a->class_write)
				json_array_append_string(acc, "wr");
			json_array_append_string(acc, "x");
			json_object_set_value(ro, "access", acc_v);
			json_array_append_value(rules, r);
		}
		/* the runtime roots: binaries and libraries must
		 * start (documented P0 honesty: read+exec under the
		 * system prefixes)
		 */
		for (i = 0; g_sys_prefixes[i] != NULL; i++) {
			JSON_Value *r = json_value_init_object();
			JSON_Object *ro = json_value_get_object(r);
			JSON_Value *acc_v = json_value_init_array();
			JSON_Array *acc = json_value_get_array(acc_v);

			json_object_set_string(ro, "path",
				g_sys_prefixes[i]);
			json_array_append_string(acc, "rd");
			json_array_append_string(acc, "x");
			json_object_set_value(ro, "access", acc_v);
			json_array_append_value(rules, r);
		}
		if (exec_path != NULL) {
			JSON_Value *r = json_value_init_object();
			JSON_Object *ro = json_value_get_object(r);
			JSON_Value *acc_v = json_value_init_array();
			JSON_Array *acc = json_value_get_array(acc_v);

			json_object_set_string(ro, "path", exec_path);
			json_array_append_string(acc, "rd");
			json_array_append_string(acc, "x");
			json_object_set_value(ro, "access", acc_v);
			json_array_append_value(rules, r);
		}
	}

#ifdef __APPLE__
	if (want_sb) {
		/*
		 * Seatbelt (01 P1): allow default, then deny writes
		 * under the mutable roots, then re-allow each
		 * declared write path (deny-before-allow ordering;
		 * macOS resolves /tmp -> /private/tmp so both
		 * spellings are denied). Reads ride "allow default"
		 * -- the P0 honesty documented in the slice.
		 */
		static const char *const roots[] = {
			"/private/tmp", "/tmp", "/Users", "/var",
			"/Library", "/private/var", NULL,
		};
		char prof[8192];
		size_t o = 0;
		size_t k;

		o += (size_t)snprintf(prof + o, sizeof(prof) - o,
			"(version 1)(allow default)");
		for (k = 0; roots[k] != NULL; k++)
			o += (size_t)snprintf(prof + o,
				sizeof(prof) - o,
				"(deny file-write* (subpath \"%s\"))",
				roots[k]);
		for (i = 0; i < allow_src->accesses.count; i++) {
			const struct ProfAccess *a =
				&allow_src->accesses.items[i];
			const char *alt_path = NULL;
			const char *suffix = NULL;

			if (!a->class_write || a->path[0] != '/')
				continue;
			o += (size_t)snprintf(prof + o,
				sizeof(prof) - o,
				"(allow file-write* (subpath \"%s\"))",
				a->path);
			/*
			 * Seatbelt evaluates RESOLVED paths. A
			 * declared allow under /tmp must also
			 * allow /private/tmp (and /var <->
			 * /private/var); otherwise a write the
			 * profile intends to permit is still
			 * denied after symlink resolution. The
			 * deny list above already carries both
			 * spellings; the allow list must too.
			 */
			if (strcmp(a->path, "/tmp") == 0) {
				alt_path = "/private/tmp";
				suffix = "";
			} else if (strncmp(a->path, "/tmp/", 5) == 0) {
				alt_path = "/private/tmp";
				suffix = a->path + 4;
			} else if (strcmp(a->path, "/private/tmp") == 0) {
				alt_path = "/tmp";
				suffix = "";
			} else if (strncmp(a->path, "/private/tmp/",
					 13) == 0) {
				alt_path = "/tmp";
				suffix = a->path + 12;
			} else if (strcmp(a->path, "/var") == 0) {
				alt_path = "/private/var";
				suffix = "";
			} else if (strncmp(a->path, "/var/", 5) == 0) {
				alt_path = "/private/var";
				suffix = a->path + 4;
			} else if (strcmp(a->path, "/private/var") == 0) {
				alt_path = "/var";
				suffix = "";
			} else if (strncmp(a->path, "/private/var/",
					 13) == 0) {
				alt_path = "/var";
				suffix = a->path + 12;
			}
			if (alt_path != NULL) {
				o += (size_t)snprintf(prof + o,
					sizeof(prof) - o,
					"(allow file-write* (subpath \"%s%s\"))",
					alt_path, suffix);
			}
		}
		{
			JSON_Value *sb_v =
				json_value_init_string(prof);

			json_object_set_value(root, "sandbox_exec",
				sb_v);
		}
	}
#endif
	if (want_seccomp) {
		JSON_Value *sc_v = json_value_init_object();
		JSON_Object *sc = json_value_get_object(sc_v);
		JSON_Value *deny_v = json_value_init_array();
		JSON_Array *deny = json_value_get_array(deny_v);

		json_object_set_value(sc, "deny", deny_v);
		json_object_set_value(root, "seccomp", sc_v);
		for (i = 0; g_unsafe_syscalls[i] != NULL; i++) {
			const char *fn = g_unsafe_syscalls[i];
			char libcn[32];

			/* socket-class libc calls appear verbatim in
			 * the profile; module/mount classes never do
			 */
			snprintf(libcn, sizeof(libcn), "%s", fn);
			if (profile_uses(usage_src, libcn))
				continue;
			json_array_append_string(deny, fn);
		}
	}

	/*
	 * AppContainer (01 P1, the Windows sibling): coarse and
	 * fail-closed by construction -- a container with no
	 * capabilities has no network, no registry, and no
	 * filesystem access beyond explicit ACL grants. The spec
	 * carries the container name (derived from the exec
	 * identity), the capability set (empty = least privilege),
	 * and the declared paths the installer turns into ACL
	 * grants. POSIX hosts compile this unchanged; the installer
	 * reports the plane missing.
	 */
	if (want_ac) {
		JSON_Value *ac_v = json_value_init_object();
		JSON_Object *ac = json_value_get_object(ac_v);
		JSON_Value *rd_v = json_value_init_array();
		JSON_Value *wr_v = json_value_init_array();
		JSON_Array *rd = json_value_get_array(rd_v);
		JSON_Array *wr = json_value_get_array(wr_v);
		char name[128];
		unsigned long long h = 1469598103934665603ULL;
		const char *seed = exec_path != NULL ? exec_path :
			"retrace";

		for (i = 0; seed[i] != '\0'; i++) {
			h ^= (unsigned char)seed[i];
			h *= 1099511628211ULL;
		}
		snprintf(name, sizeof(name), "retrace.%016llx", h);
		for (i = 0; i < allow_src->accesses.count; i++) {
			const struct ProfAccess *a =
				&allow_src->accesses.items[i];

			if (a->path == NULL || a->path[0] == '\0')
				continue;
			if (a->class_write)
				json_array_append_string(wr, a->path);
			else if (a->class_read)
				json_array_append_string(rd, a->path);
		}
		json_object_set_string(ac, "name", name);
		json_object_set_value(ac, "read_paths", rd_v);
		json_object_set_value(ac, "write_paths", wr_v);
		json_object_set_value(root, "appcontainer", ac_v);
	}
	return root_v;
}

int enforce_mode(int argc, char **argv)
{
	const char *in_path = NULL;
	const char *inside_path = NULL;
	const char *out_path = NULL;
	const char *exec_path = NULL;
	const char *backend = "both";
	struct ProfFeed feed, inside_feed;
	const struct Profile *allow_src;
	JSON_Value *spec;
	int i;
	int want_ll = 1, want_sc = 1;
#ifdef __APPLE__
	int want_sb = 0;
#else
	int want_sb_unused = 0;
#define want_sb want_sb_unused
#endif
	int want_ac = 0;

	memset(&feed, 0, sizeof(feed));
	memset(&inside_feed, 0, sizeof(inside_feed));

	for (i = 2; i < argc; i++) {
		if (strcmp(argv[i], "--inside") == 0 && i + 1 < argc)
			inside_path = argv[++i];
		else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
			out_path = argv[++i];
		else if (strcmp(argv[i], "--exec") == 0 && i + 1 < argc)
			exec_path = argv[++i];
		else if (strcmp(argv[i], "--backend") == 0 &&
			 i + 1 < argc)
			backend = argv[++i];
		else if (argv[i][0] == '-' && argv[i][1] != '\0') {
			usage();
			return 2;
		} else if (in_path == NULL) {
			in_path = argv[i];
		}
	}
	if (in_path == NULL) {
		usage();
		return 2;
	}
	if (strcmp(backend, "landlock") == 0) {
		want_sc = 0;
	} else if (strcmp(backend, "seccomp") == 0) {
		want_ll = 0;
	} else if (strcmp(backend, "sandbox-exec") == 0) {
		want_sb = 1;
		want_ll = 0;
		want_sc = 0;
	} else if (strcmp(backend, "appcontainer") == 0) {
		want_ac = 1;
		want_ll = 0;
		want_sc = 0;
	} else if (strcmp(backend, "all") == 0) {
		want_sb = 1;
		want_ac = 1;
	} else if (strcmp(backend, "both") != 0) {
		usage();
		return 2;
	}

	if (load_any(in_path, &feed) != 0) {
		fprintf(stderr,
			"retrace-profile: cannot read %s\n", in_path);
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

	spec = build_spec(allow_src, &feed.prof, exec_path,
		want_ll, want_sc, want_sb, want_ac);
	{
		char *ser = json_serialize_to_string_pretty(spec);

		if (out_path != NULL) {
			FILE *f = fopen(out_path, "w");

			if (f == NULL) {
				perror("enforce: -o");
				json_value_free(spec);
				prof_free(&feed.prof);
				if (inside_path != NULL)
					prof_free(&inside_feed.prof);
				return 2;
			}
			fputs(ser, f);
			fputc('\n', f);
			fclose(f);
		} else {
			fputs(ser, stdout);
			fputc('\n', stdout);
		}
		json_free_serialized_string(ser);
	}
	json_value_free(spec);
	prof_free(&feed.prof);
	if (inside_path != NULL)
		prof_free(&inside_feed.prof);
	return 0;
}
