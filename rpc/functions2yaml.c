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

#include <error.h>
#include <string.h>
#include <stdio.h>
#include <sys/queue.h>
#include <stdlib.h>
#include <ctype.h>

struct type {
	const char *name;
	const char *ctype;
	const char *rpctype;
	int fixup;
} types[] = {
	{"char",	"char ",		"char ",	0},
	{"buffer",	"void *",
			"struct {void *address; size_t len;} ",	1},
	{"dir",		"DIR *",
			"struct {void *address; int fd;} ",	0},
	{"cstring",	"const char *",		NULL,		0},
	{"dirent",	"struct dirent *",	NULL,		0},
	{"file",	"FILE *",
			"struct {void *address; int fd;} ",	0},
	{"int",		"int ",			"int ",		0},
	{"pcvoid",	"const void *",		NULL,		0},
	{"pid_t",	"pid_t ",		"pid_t ",	0},
	{"pdirent",	"struct dirent **",	NULL,		0},
	{"pvoid",	"void *",		"void *",	0},
	{"size_t",	"size_t ",		"size_t ",	0},
	{"ssize_t",	"ssize_t ",		"ssize_t ",	0},
	{"string",	"char *",		"char *",	0},
	{"va_list",	"va_list ",		NULL,		0},
	{NULL,		NULL,			NULL,		0}
};

enum rpc_type {
	RPC_VOID,
	RPC_PTR,
	RPC_INT,
	RPC_UINT,
	RPC_STR
};

struct param {
	TAILQ_ENTRY(param) next;
	const char *name;
	const struct type *type;
	const struct type *pre;
	const struct type *post;
};

TAILQ_HEAD(param_list, param);

struct function {
	STAILQ_ENTRY(function) next;
	const char *name;
	const struct type *type;
	const struct type *post;
	struct param_list params;
	const char *va_fn;
};

STAILQ_HEAD(function_list, function);

void
*check_alloc(void *p)
{
	if (p == NULL)
		error(1, 0, "Out of memory.");
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
lookup_type(const char *s, int line)
{
	struct type *p;

	for (p = types; p->name; p++)
		if (!strcmp(p->name, s))
			return p;
	error(1, 0, "Parameter type [%s] not found at line %d.", s, line);
	return NULL;
}

void yaml(struct function_list *fns)
{
	struct function *fn;
	struct param *param;
	int num_params;

	printf("---\n");
	printf("functions:\n");
	STAILQ_FOREACH(fn, fns, next) {

		printf("- fname: %s\n", fn->name);
		printf("  enum: RPC_%s\n", fn->name);
		printf("  type: %s\n", fn->type->name);
		printf("  ctype: \"%s\"\n", fn->type->ctype);
		if (fn->post) {
			printf("  post: \"%s\"\n", fn->post->name);
			printf("  cpost: \"%s\"\n", fn->post->rpctype);
			if (fn->post->fixup)
				printf("  fixup: true\n");
		}

		if (fn->va_fn)
			printf("  variadic: %s\n", fn->va_fn);

		num_params = 0;
		if (!TAILQ_EMPTY(&(fn->params))) {
			printf("  params:\n");
			TAILQ_FOREACH(param, &(fn->params), next) {
				printf("  - pname: %s\n", param->name);
				printf("    pnum: %d\n", num_params);
				printf("    type: %s\n", param->type->name);
				printf("    ctype: \"%s\"\n", param->type->ctype);
				printf("    pre: %s\n", param->pre->name);
				printf("    cpre: \"%s\"\n", param->pre->rpctype);
				if (param->pre->fixup)
					printf("    prefixup: true\n");
				printf("    post: %s\n", param->post->name);
				printf("    cpost: \"%s\"\n", param->post->rpctype);
				if (param->post->fixup)
					printf("    postfixup: true\n");
				++num_params;
			}
			printf("    last: true\n");
		}
		if (num_params) {
			printf("  num_params: %d\n", num_params);
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
	unsigned int line = 0;
	const char *tok;
	struct function *fn = NULL;
	struct function_list functions;
	struct param *param;
	struct type voidtype = {"void", "void ", NULL};

	STAILQ_INIT(&functions);

	for (len = 0; len >= 0; ++line, len = getline(&buf, &buflen, stdin)) {

		if (len == 0)
			continue;

		if (*buf == ';')
			continue;

		/* strip \n from buffer */
		if (buf[len - 1] == '\n')
			buf[len - 1] = '\0';

		tok = strtok(buf, " ");
		if (tok == NULL)
			continue;

		if (fn == NULL && strcmp(tok, "function") != 0)
			error(1, 0, "First line must be a function.");

		if (strcmp(tok, "function") == 0) {
			if (fn) {
				if (fn->va_fn && TAILQ_EMPTY(&(fn->params)))
					error(1, 0, "Variadic must have "
					    "parameters at line %d.",
					    line - 1);
				STAILQ_INSERT_TAIL(&functions, fn, next);
			}

			fn = check_alloc(malloc(sizeof(struct function)));
			memset(fn, 0, sizeof(struct function));
			TAILQ_INIT(&(fn->params));

			fn->name = _strdup(strtok(NULL, " "));
			if (fn->name == NULL)
				error(1, 0, "Keyword 'function' must be "
				    "followed by a function name at line %d.",
				    line);

			tok = strtok(NULL, " ");

			if (tok == NULL)
				error(1, 0, "Function without type "
				    "at line %d.", line);

			if (!strcmp(tok, "void"))
				fn->type = &voidtype;
			else {
				fn->type = lookup_type(tok, line);

				tok = strtok(NULL, " ");
				fn->post = tok ? lookup_type(tok, line) : fn->type;

				if (fn->post->rpctype == NULL)
					error(1, 0, "Not an rpc type [%s] at "
					    "line %d.", fn->post->name, line);
			}
		} else if (strcmp(tok, "parameter") == 0) {
			param = check_alloc(malloc(sizeof(struct param)));

			param->name = _strdup(strtok(NULL, " "));
			if (param->name == NULL)
				error(1, 0, "Parameter name not found "
				    "at line %d.", line);

			tok = strtok(NULL, " ");
			if (tok == NULL)
				error(1, 0, "Parameter must have a type "
				    "at line %d.", line);

			param->type = lookup_type(tok, line);

			tok = strtok(NULL, " ");
			param->pre = tok ? lookup_type(tok, line) : param->type;
			if (param->pre->rpctype == NULL)
				error(1, 0, "Not an rpc type [%s] "
				    "at line %d.", param->pre->name, line);

			tok = strtok(NULL, " ");
			param->post = tok ? lookup_type(tok, line) : param->pre;
			if (param->post->rpctype == NULL)
				error(1, 0, "Not an rpc type [%s] "
				    "at line %d.", param->post->name, line);

			TAILQ_INSERT_TAIL(&fn->params, param, next);
		} else if (strcmp(tok, "variadic") == 0) {
			if (fn->va_fn != NULL)
				error(1, 0, "Second 'variadic' found at line %d.", line);
			fn->va_fn = _strdup(strtok(NULL, " "));
			if (fn->va_fn == NULL)
				error(1, 0, "Keyword 'variadic' must be followed the name of a fixed param version at line %d.", line);
		}
	}

	if (fn)
		STAILQ_INSERT_TAIL(&functions, fn, next);

	yaml(&functions);

	return 0;
}
