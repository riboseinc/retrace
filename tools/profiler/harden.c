/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Container hardening export (TODO.trace-profile/19): the jail,
 * exported as infrastructure. A profile becomes a
 * docker-compose service fragment: read_only root, capabilities
 * dropped, write-class paths as explicit rw binds, read-class
 * paths as ro binds, network disabled when the profile shows no
 * network activity, and the env whitelist as an environment
 * template. Hand-rolled YAML: the shape is fixed and every
 * scalar is quoted.
 */

#include "harden.h"

#include "parson.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Quote a scalar as a YAML double-quoted string. Escapes the
 * characters YAML double-quote syntax cares about; dstsz bounds
 * the write.
 */
static void yaml_q(const char *s, char *dst, size_t dstsz)
{
	size_t o = 0;

	dst[o++] = '"';
	for (; *s != '\0' && o + 7 < dstsz; s++) {
		switch (*s) {
		case '"':
		case '\\':
			dst[o++] = '\\';
			dst[o++] = *s;
			break;
		case '\n':
			dst[o++] = '\\';
			dst[o++] = 'n';
			break;
		case '\t':
			dst[o++] = '\\';
			dst[o++] = 't';
			break;
		default:
			dst[o++] = *s;
			break;
		}
	}
	dst[o++] = '"';
	dst[o] = '\0';
}

int prof_harden_compose(const struct Profile *p, FILE *out)
{
	size_t i;
	int has_net = p->net.count > 0;
	char q[1024];

	fprintf(out, "# retrace harden: the jail as container policy\n");
	fprintf(out, "# derived from the observed profile -- bind paths as\n");
	fprintf(out, "# your deployment requires; classes are observed\n");
	fprintf(out, "services:\n");
	fprintf(out, "  app:\n");
	fprintf(out, "    image: REPLACE_ME\n");
	fprintf(out, "    read_only: true\n");
	fprintf(out, "    cap_drop:\n      - ALL\n");
	fprintf(out, "    security_opt:\n      - no-new-privileges:true\n");
	if (!has_net)
		fprintf(out, "    network_mode: \"none\"\n");
	else
		fprintf(out, "    # profile observed network activity --\n"
			     "    # constrain it with your platform's policy\n");

	fprintf(out, "    volumes:\n");
	for (i = 0; i < p->accesses.count; i++) {
		const struct ProfAccess *a = &p->accesses.items[i];

		if (a->path == NULL || a->path[0] == '\0')
			continue;
		yaml_q(a->path, q, sizeof(q));
		if (a->class_write)
			fprintf(out, "      - %s:%s:rw\n", q, q);
		else
			fprintf(out, "      - %s:%s:ro\n", q, q);
	}

	if (p->env.count > 0) {
		fprintf(out, "    # env whitelist: fill the values\n");
		fprintf(out, "    environment:\n");
		for (i = 0; i < p->env.count; i++) {
			yaml_q(p->env.names[i], q, sizeof(q));
			fprintf(out, "      %s: \"\"\n", q);
		}
	}
	return 0;
}
