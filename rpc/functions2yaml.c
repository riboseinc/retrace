#include <error.h>
#include <string.h>
#include <stdio.h>
#include <sys/queue.h>
#include <stdlib.h>
#include <ctype.h>

struct type {
	char *ctype;
	char *rpctype;
	char *rpctypenum;
	const char *header;
} types[] = {
	{"char *",	"char *",	"RPC_PTR",	NULL		},
	{"const char *", "char *",	"RPC_STR",	NULL		},
	{"DIR *",	"DIR *",	"RPC_PTR",	"dirent.h"	},
	{"FILE *",	"FILE *",	"RPC_PTR",	"stdio.h"	},
	{"int",		"int",		"RPC_INT",	NULL		},
	{"pid_t",	"pid_t",	"RPC_UINT",	"sys/types.h"	},
	{"size_t",	"size_t",	"RPC_UINT",	"stdlib.h"	},
	{"ssize_t",	"ssize_t",	"RPC_INT",	"sys/types.h"	},
	{"struct dirent *",
			"struct dirent*",
					"RPC_PTR",	"dirent.h"	},
	{"struct dirent **",
			"struct dirent**",
					"RPC_PTR",	"dirent.h"	},
	{"va_list",	"void *",	"RPC_PTR",	"stdarg.h"	},
	{"void",	NULL,		"RPC_VOID",	NULL		},
	{"void *",	"void *",	"RPC_PTR",	NULL		},
	{"const void *",
			"void *",	"RPC_PTR",	NULL		},
	{NULL,		NULL,		NULL,		NULL		}
};

struct param {
	STAILQ_ENTRY(param) next;
	char *name;
	struct type *type;
	char *inout;
};

STAILQ_HEAD(param_list, param);

struct header {
	STAILQ_ENTRY(header) next;
	char *name;
};

STAILQ_HEAD(header_list, header);

struct function {
	STAILQ_ENTRY(function) next;
	char *name;
	unsigned int num_params;
	struct type *type;
	struct param_list params;
	char *va_fn;
};

STAILQ_HEAD(function_list, function);

void *check_alloc(void *p)
{
	if (p == NULL)
		error(1, 0, "Out of memory.");
	return p;
}

struct type *
get_type(unsigned int line)
{
	struct type *p;
	char *ctype;

	/*
	 * type is remainder of line and won't contain a ';'
	 */
	ctype = strtok(NULL, ";");

	if (ctype == NULL)
		ctype = "";

	for (p = types; p->ctype != NULL; p++)
		if (!strcmp(p->ctype, ctype))
			return p;

	error(1, 0, "Unknown type [%s] at line %d.", ctype, line);
}

void add_parameter(unsigned int line, struct param_list *params)
{
	struct param *param;
	const char *name, *inout;

	param = check_alloc(malloc(sizeof(struct param)));

	inout = strtok(NULL, " ");
	if (inout == NULL)
		error(1, 0, "Parameter in/out not found at line %d.", line);

	if (!strcmp(inout, "in"))
		param->inout = "RPC_INPARAM";
	else if (!strcmp(inout, "out"))
		param->inout = "RPC_OUTPARAM";
	else if (!strcmp(inout, "inout"))
		param->inout = "RPC_INOUTPARAM";
	else
		error(1, 0, "Parameter missing in/out at line %d.", line);

	name = strtok(NULL, " ");
	if (name == NULL)
		error(1, 0, "Parameter name not found at line %d.", line);

	param->name = check_alloc(strdup(name));

	param->type = get_type(line);

	if (param->type->rpctype == NULL)
		error(1, 0, "Parameters can't have 'void' type at line %d.", line);

	STAILQ_INSERT_TAIL(params, param, next);
}

void add_header(struct header_list *headers, const char *name)
{
	struct header *header;

	if (name == NULL)
		return;

	STAILQ_FOREACH(header, headers, next)
		if (!strcmp(header->name, name))
			return;

	header = check_alloc(malloc(sizeof(struct header)));
	header->name = (char *)name;
	STAILQ_INSERT_TAIL(headers, header, next);
}

const char *last_parameter_name(struct param_list *params)
{
	struct param *param;

	STAILQ_FOREACH(param, params, next)
		if (!STAILQ_NEXT(param, next))
			return param->name;

	return NULL;
}

void yaml(struct function_list *fns)
{
	struct function *function;
	struct param *param;
	int i;
	char *ucname, *p;
	struct header_list headers;
	struct header *header;

	printf("---\n");
	printf("functions:\n");
	STAILQ_FOREACH(function, fns, next) {
		ucname = check_alloc(strdup(function->name));
		for (p = ucname; *p; ++p)
			*p = toupper(*p);

		printf("- name: %s\n", function->name);
		printf("  enum: RPC_%s\n", ucname);
		printf("  type: %s\n", function->type->ctype);
		printf("  rpctypenum: %s\n", function->type->rpctypenum);
		if (function->type->rpctype) {
			printf("  rpctype: %s\n", function->type->rpctype);
			if (!strcmp(function->type->rpctypenum, "RPC_STR"))
				printf("  is_string: true\n");
		} else
			printf("  is_void: true\n");
		printf("  paramcount: %d\n", function->num_params);

		if (function->va_fn != NULL) {
			printf("  variadic:\n");
			printf("    function: %s\n", function->va_fn);
			printf("    start: %s\n", last_parameter_name(&(function->params)));
		}
		if (!STAILQ_EMPTY(&(function->params))) {
			printf("  params:\n");
			STAILQ_FOREACH(param, &(function->params), next) {
				printf("  - name: %s\n", param->name);
				printf("    type: %s\n", param->type->ctype);
				printf("    rpctype: %s\n", param->type->rpctype);
				printf("    rpctypenum: %s\n", param->type->rpctypenum);
				printf("    inout: %s\n", param->inout);
				printf("    spacing: %s\n", strchr(param->type->ctype, '*') ? "\"\"" : "\" \"");
				if (!strcmp(param->type->rpctypenum, "RPC_STR"))
					printf("    is_string: true\n");
				if (!STAILQ_NEXT(param, next))
					printf("    last: true\n");
			}
		}
		free(ucname);
	}

	STAILQ_INIT(&headers);

	printf("headers:\n");
	STAILQ_FOREACH(function, fns, next) {
		if (function->type)
			add_header(&headers, function->type->header);

		STAILQ_FOREACH(param, &(function->params), next)
			add_header(&headers, param->type->header);
	}

	STAILQ_FOREACH(header, &headers, next)
		printf("  - header: %s\n", header->name);

	printf("---\n");
}

int main(void)
{
	char *buf = NULL;
	size_t buflen = 0;
	ssize_t len;
	unsigned int line = 0, i;
	const char *tok;
	struct function *fn = NULL;
	struct function_list functions;

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
			if (fn)
				STAILQ_INSERT_TAIL(&functions, fn, next);

			fn = check_alloc(malloc(sizeof(struct function)));
			memset(fn, 0, sizeof(struct function));
			STAILQ_INIT(&(fn->params));

			tok = strtok(NULL, " ");
			if (tok == NULL)
				error(1, 0, "Keyword 'function' must be followed by a function name at line %d.", line);

			fn->name = check_alloc(strdup(tok));
			fn->type = get_type(line);
		} else if (strcmp(tok, "parameter") == 0) {
			++fn->num_params;
			add_parameter(line, &(fn->params));
		} else if (strcmp(tok, "variadic") == 0) {
			if (fn->va_fn != NULL)
				error(1, 0, "Second 'variadic' found at line %d.", line);
			tok = strtok(NULL, " ");
			if (tok == NULL)
				error(1, 0, "Keyword 'variadic' must be followed the name of a fixed param version at line %d.", line);
			fn->va_fn = check_alloc(strdup(tok));
		}
	}

	if (fn)
		STAILQ_INSERT_TAIL(&functions, fn, next);

	yaml(&functions);

	return 0;
}
