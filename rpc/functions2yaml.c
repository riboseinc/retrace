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

#include "../config.h"

#include <string.h>
#include <stdio.h>
#include <sys/queue.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>

static void
errexit(int exitval, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	exit(exitval);
}

struct type {
	const char *name;
	const char *ctype;
	const char *rpctype;
} types[] = {
	{"char",	"int ",			NULL		},
	{"cmsghdr",	"const struct msghdr *",	NULL	},
	{"cstring",	"const char *",		NULL		},
	{"dir",		"DIR *",		NULL		},
	{"dirent",	"struct dirent *",	NULL		},
	{"fd",		"int ",			NULL		},
	{"file",	"FILE *",		NULL		},
	{"int",		"int ",			NULL		},
	{"msghdr",	"struct msghdr *",	NULL		},
	{"pcvoid",	"const void *",		NULL		},
	{"pdirent",	"struct dirent **",	NULL		},
	{"pid_t",	"pid_t ",		NULL		},
	{"pint",	"int *",		NULL		},
	{"psize_t",	"size_t *",		NULL		},
	{"pstring",	"char **",		NULL		},
	{"pvoid",	"void *",		NULL		},
	{"size_t",	"size_t ",		NULL		},
	{"ssize_t",	"ssize_t ",		NULL		},
	{"string",	"char *",		NULL		},
	{"va_list",	"va_list ",		"void *"	},
	{"void",	"void ",		NULL		},
	{NULL,		NULL,			NULL		}
};

struct param {
	TAILQ_ENTRY(param) next;
	const char *name;
	const char *type;
	const char *ctype;
	const char *rpctype;
};

TAILQ_HEAD(param_list, param);

struct function {
	STAILQ_ENTRY(function) next;
	const char *name;
	const char *type;
	const char *ctype;
	const char *rpctype;
	struct param_list params;
	const char *va_fn;
	const char *guard;
};

STAILQ_HEAD(function_list, function);

void
*check_alloc(void *p)
{
	if (p == NULL)
		errexit(1, "Out of memory.");
	return p;
}

char *
_strdup(const char *s)
{
	if (s == NULL)
		return NULL;
	return check_alloc(strdup(s));
}

const struct type *
lookup_type(const char *s)
{
	struct type *p;

	for (p = types; p->name; p++)
		if (!strcmp(p->name, s))
			return p;
	return NULL;
}

void yaml(struct function_list *fns)
{
	struct function *fn;
	struct param *param;

	printf("---\n");
	printf("functions:\n");
	STAILQ_FOREACH(fn, fns, next) {

		printf("- name: %s\n", fn->name);
		printf("  type: \"%s\"\n", fn->type);
		printf("  ctype: \"%s\"\n", fn->ctype);
		printf("  rpctype: \"%s\"\n", fn->rpctype);
		if (strcmp(fn->type, "void") != 0)
			printf("  result: true\n");
		if (fn->guard != NULL)
			printf("  guard: \"%s\"\n", fn->guard);

		if (!TAILQ_EMPTY(&(fn->params))) {
			printf("  has_parameters: true\n");
			printf("  params:\n");
		}

		TAILQ_FOREACH(param, &(fn->params), next) {
			printf("  - name: %s\n", param->name);
			printf("    type: \"%s\"\n",
			    param->type);
			printf("    ctype: \"%s\"\n",
			    param->ctype);
			printf("    rpctype: \"%s\"\n",
			    param->rpctype);
		}

		if (!TAILQ_EMPTY(&(fn->params)))
			printf("    last: true\n");

		if (fn->va_fn) {
			printf("  variadic: %s\n", fn->va_fn);
			printf("  last_param: %s\n",
			    TAILQ_LAST(&(fn->params), param_list)->name);
		}
	}
	printf("---\n");
}

int main(void)
{
	char *buf = NULL;
	size_t buflen = 0;
	ssize_t len;
	unsigned int line;
	const char *tok;
	struct function *fn = NULL;
	struct function_list functions;
	struct param *param;
	const struct type *type;

	STAILQ_INIT(&functions);

	line = 0;
	for (;;) {
		len = getline(&buf, &buflen, stdin);

		if (len <= 0)
			break;

		++line;

		/* strip \n from buffer */
		if (buf[len - 1] == '\n')
			buf[len - 1] = '\0';

		if (*buf == ';')
			continue;

		tok = strtok(buf, " ");
		if (tok == NULL)
			continue;

		if (fn == NULL && strcmp(tok, "function") != 0)
			errexit(1, "First line must be a function.");

		if (strcmp(tok, "function") == 0) {
			if (fn) {
				if (fn->va_fn && TAILQ_EMPTY(&(fn->params)))
					errexit(1, "Variadic must have "
					    "parameters at line %d.",
					    line - 1);
				STAILQ_INSERT_TAIL(&functions, fn, next);
			}

			fn = check_alloc(malloc(sizeof(struct function)));
			memset(fn, 0, sizeof(struct function));
			TAILQ_INIT(&(fn->params));

			fn->name = _strdup(strtok(NULL, " "));
			if (fn->name == NULL)
				errexit(1, "Keyword 'function' must be "
				    "followed by a function name at line %d.",
				    line);

			tok = strtok(NULL, " ");

			if (tok == NULL)
				errexit(1, "Function without type "
				    "at line %d.", line);

			type = lookup_type(tok);

			if (type == NULL)
				errexit(1, "Bad function type [%s] at "
				    "line %d.", tok, line);

			fn->type = type->name;
			fn->ctype = type->ctype;
			fn->rpctype = type->rpctype;
			if (fn->rpctype == NULL)
				fn->rpctype = fn->ctype;

			if (strtok(NULL, " "))
				errexit(1, "Too many tokens at line %d.",
				    line);

		} else if (strcmp(tok, "parameter") == 0) {
			param = check_alloc(malloc(sizeof(struct param)));

			param->name = _strdup(strtok(NULL, " "));
			if (param->name == NULL)
				errexit(1, "Parameter name not found "
				    "at line %d.", line);

			tok = strtok(NULL, " ");
			if (tok == NULL)
				errexit(1, "Parameter must have a type "
				    "at line %d.", line);

			type = lookup_type(tok);

			if (type == NULL
			    || strcmp(tok, "void") == 0)
				errexit(1, "Bad parameter type [%s] at "
				    "line %d.", tok, line);

			param->type = type->name;
			param->ctype = type->ctype;
			param->rpctype = type->rpctype;
			if (param->rpctype == NULL)
				param->rpctype = param->ctype;

			if (strtok(NULL, " "))
				errexit(1, "Too many tokens at line %d.",
				    line);

			TAILQ_INSERT_TAIL(&fn->params, param, next);
		} else if (strcmp(tok, "variadic") == 0) {
			if (fn->va_fn != NULL)
				errexit(1, "Second 'variadic' found at line %d.", line);
			fn->va_fn = _strdup(strtok(NULL, " "));
			if (fn->va_fn == NULL)
				errexit(1, "Keyword 'variadic' must be followed the name of a fixed param version at line %d.", line);
		} else if (strcmp(tok, "guard") == 0)
			fn->guard = _strdup(strtok(NULL, "\n"));
	}

	if (fn)
		STAILQ_INSERT_TAIL(&functions, fn, next);

	yaml(&functions);

	return 0;
}
