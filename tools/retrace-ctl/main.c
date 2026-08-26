/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * retrace-ctl -- the fleet CLI (TODO.supervisor/07, P0).
 *
 * One command per invocation against a local retraced's
 * controller socket (newline-JSON both ways). JSON out by
 * default; pipe through jq for humans, -t tables land in P1.
 *
 * Exit codes: 0 ok, 1 daemon reported an error, 2 usage/conn.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define CTL_DEFAULT "/tmp/retraced.ctl.sock"

/* JSON string escape (policy blobs are JSON-in-JSON); caller frees */
static char *jesc(const char *s)
{
	size_t n = 0, o = 0;
	const char *p;
	char *out;

	for (p = s; *p != '\0'; p++) {
		if (*p == '"' || *p == '\\')
			n += 2;
		else if ((unsigned char)*p < 0x20)
			n += 6;
		else
			n++;
	}
	out = malloc(n + 1);
	if (out == NULL)
		return NULL;
	for (p = s; *p != '\0'; p++) {
		if (*p == '"' || *p == '\\') {
			out[o++] = '\\';
			out[o++] = *p;
		} else if ((unsigned char)*p < 0x20) {
			o += (size_t)snprintf(out + o, 7, "\\u%04x",
				(unsigned int)*p);
		} else {
			out[o++] = *p;
		}
	}
	out[o] = '\0';
	return out;
}

static char *read_file(const char *path)
{
	FILE *f = fopen(path, "rb");
	long sz;
	char *buf;

	if (f == NULL)
		return NULL;
	fseek(f, 0, SEEK_END);
	sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (sz <= 0 || sz > 60000) {
		fclose(f);
		return NULL;
	}
	buf = malloc((size_t)sz + 1);
	if (buf == NULL) {
		fclose(f);
		return NULL;
	}
	if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
		free(buf);
		fclose(f);
		return NULL;
	}
	fclose(f);
	buf[sz] = '\0';
	return buf;
}

/* one round trip: send the request line, print the reply line */
static int ctl_roundtrip(const char *sock_path, const char *request)
{
	struct sockaddr_un sa;
	char reply[8192];
	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	ssize_t n;

	if (fd < 0) {
		perror("socket");
		return 2;
	}
	memset(&sa, 0, sizeof(sa));
	sa.sun_family = AF_UNIX;
	snprintf(sa.sun_path, sizeof(sa.sun_path), "%s", sock_path);
	if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) != 0) {
		fprintf(stderr, "retrace-ctl: cannot reach %s "
			"(is retraced running with --ctl?)\n", sock_path);
		close(fd);
		return 2;
	}
	if (write(fd, request, strlen(request)) < 0) {
		perror("write");
		close(fd);
		return 2;
	}
	n = read(fd, reply, sizeof(reply) - 1);
	close(fd);
	if (n <= 0) {
		fprintf(stderr, "retrace-ctl: no reply\n");
		return 2;
	}
	reply[n] = '\0';
	fputs(reply, stdout);
	if (strstr(reply, "\"ok\":1") == NULL &&
	    strstr(reply, "\"ok\": true") == NULL)
		return 1;
	return 0;
}

static int ctl_usage(void)
{
	fprintf(stderr,
		"usage: retrace-ctl [--sock PATH] COMMAND\n"
		"  status                 daemon info, agent count\n"
		"  ps                     registry table (JSON)\n"
		"  policy-push FILE       push a policy to all agents\n"
		"  freeze                 hold every agent (wildcard freeze)\n"
		"  thaw                   restore the pre-freeze policy\n"
		"  kill PID               SIGTERM one target\n");
	return 2;
}

int main(int argc, char **argv)
{
	const char *sock_path = CTL_DEFAULT;
	char req[64000];
	int i;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--sock") == 0 && i + 1 < argc)
			sock_path = argv[++i];
		else
			break;
	}
	if (i >= argc)
		return ctl_usage();

	if (strcmp(argv[i], "status") == 0) {
		snprintf(req, sizeof(req),
			"{\"cmd\":\"status\"}\n");
	} else if (strcmp(argv[i], "ps") == 0) {
		snprintf(req, sizeof(req), "{\"cmd\":\"ps\"}\n");
	} else if (strcmp(argv[i], "policy-push") == 0 &&
		   i + 1 < argc) {
		char *blob = read_file(argv[i + 1]);
		char *esc;
		int rc;

		if (blob == NULL) {
			fprintf(stderr, "retrace-ctl: cannot read %s\n",
				argv[i + 1]);
			return 2;
		}
		esc = jesc(blob);
		free(blob);
		if (esc == NULL)
			return 2;
		rc = snprintf(req, sizeof(req),
			"{\"cmd\":\"policy_push\",\"blob\":\"%s\"}\n", esc);
		free(esc);
		if (rc < 0 || (size_t)rc >= sizeof(req)) {
			fprintf(stderr, "retrace-ctl: policy too large\n");
			return 2;
		}
	} else if (strcmp(argv[i], "freeze") == 0) {
		snprintf(req, sizeof(req), "{\"cmd\":\"freeze\"}\n");
	} else if (strcmp(argv[i], "thaw") == 0) {
		snprintf(req, sizeof(req), "{\"cmd\":\"thaw\"}\n");
	} else if (strcmp(argv[i], "kill") == 0 && i + 1 < argc) {
		long pid = strtol(argv[i + 1], NULL, 10);

		if (pid <= 0)
			return ctl_usage();
		snprintf(req, sizeof(req),
			"{\"cmd\":\"kill\",\"pid\":%ld}\n", pid);
	} else {
		return ctl_usage();
	}
	return ctl_roundtrip(sock_path, req);
}
