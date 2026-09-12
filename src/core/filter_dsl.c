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
 * The filter expression language (TODO.impl/08). See filter_dsl.h
 * for the grammar and semantics. This module is deliberately
 * self-contained: no logger, no JSON -- the parser walks plain
 * memory and the tree evaluates against a ThreadContext. The
 * `filter` action and the config validator are its only
 * consumers; the standalone unit test its proof.
 */

#include "filter_dsl.h"

#include <string.h>

#include "engine.h"
#include "real_impls.h"

/* No libc classification/conversion calls here: the parser runs
 * inside the engine (config validation at boot, first-action
 * compile later), where direct libc would recurse through the
 * trampolines. Tiny local helpers instead.
 */
static int is_digit(char c)
{
	return c >= '0' && c <= '9';
}

static int is_alpha(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static int is_ident_char(char c)
{
	return is_alpha(c) || is_digit(c) || c == '_';
}

#define STR_EQ(a, b) (retrace_real_impls.strcmp((a), (b)) == 0)
#define STR_NCMP(a, b, n) (retrace_real_impls.strncmp((a), (b), (n)))
/*
 * parenthesized: fortified <string.h> macro-substitutes bare
 * member-call syntax on macOS toolchains
 */
#define RI_MEMSET(s, c, n) (retrace_real_impls.memset)((s), (c), (n))
#define RI_MEMCPY(d, s, n) (retrace_real_impls.memcpy)((d), (s), (n))

/* ---- AST ----------------------------------------------------------------
 *
 * Nodes live in ONE arena block (malloc'd with the tree header):
 * strings reference slices copied into the same arena. Nothing
 * individual is freed -- one free() releases the compilation.
 */

enum fnode_kind {
	FN_OR,		/* lhs | rhs (children in seq[]) */
	FN_AND,		/* lhs & rhs */
	FN_NOT,		/* !child */
	FN_CMP,		/* operand OP operand */
};

enum fop_kind {
	FO_EQ,
	FO_NE,
	FO_LT,
	FO_LE,
	FO_GT,
	FO_GE,
	FO_GLOB,
	FO_NGLOB,
};

enum foperand_kind {
	FO_NUM,		/* literal number */
	FO_STR,		/* literal string */
	FO_PARAM,	/* param name -> value at eval */
	FO_FUNC,	/* builtin: function name */
	FO_RET,		/* builtin: ret_val */
	FO_CALLER,	/* builtin: caller symbol */
};

struct foperand {
	enum foperand_kind kind;
	long long num;
	const char *str;	/* arena slice */
	size_t str_len;
};

struct fnode {
	enum fnode_kind kind;
	union {
		struct {
			struct fnode *lhs;
			struct fnode *rhs;
		} bin;
		struct fnode *child;
		struct {
			enum fop_kind op;
			struct foperand lhs;
			struct foperand rhs;
		} cmp;
	} u;
};

struct retrace_filter_ast {
	char *arena;		/* the single allocation */
	size_t arena_len;
	struct fnode *root;
	/* number of nodes (for the unit test's size assertions) */
	size_t node_cnt;
};

/* ---- glob matcher ------------------------------------------------------ */

int
retrace_filter_glob_match(const char *pattern, const char *text)
{
	const char *p = pattern;
	const char *t = text;
	const char *star_p = NULL;
	const char *star_t = NULL;

	if (pattern == NULL || text == NULL)
		return 0;

	while (*t != '\0') {
		if (*p == '?') {
			p++;
			t++;
		} else if (*p == '\\' && p[1] != '\0') {
			if (*t == p[1]) {
				p += 2;
				t++;
			} else if (star_p != NULL) {
				p = star_p + 1;
				t = ++star_t;
			} else {
				return 0;
			}
		} else if (*p == '[') {
			const char *q = p + 1;
			int negate = 0;
			int matched = 0;

			if (*q == '!' || *q == '^') {
				negate = 1;
				q++;
			}
			/* ']' first = a literal ']' in the class */
			if (*q == ']') {
				if (*t == ']')
					matched = 1;
				q++;
			}
			while (*q != ']' && *q != '\0') {
				if (q[1] == '-' && q[2] != ']' &&
				    q[2] != '\0') {
					if (*t >= q[0] && *t <= q[2])
						matched = 1;
					q += 3;
				} else {
					if (*t == *q)
						matched = 1;
					q++;
				}
			}
			if (*q != ']') {
				/* unterminated class: literal '[' */
				if (*t == '[') {
					p++;
					t++;
				} else if (star_p != NULL) {
					p = star_p + 1;
					t = ++star_t;
				} else {
					return 0;
				}
			} else if (matched != negate) {
				p = q + 1;
				t++;
			} else if (star_p != NULL) {
				p = star_p + 1;
				t = ++star_t;
			} else {
				return 0;
			}
		} else if (*p == *t && *p != '\0') {
			p++;
			t++;
		} else if (*p == '*') {
			star_p = p;
			star_t = t;
			p++;
		} else if (star_p != NULL) {
			p = star_p + 1;
			t = ++star_t;
		} else {
			return 0;
		}
	}

	while (*p == '*')
		p++;
	return *p == '\0';
}

/* ---- parser ------------------------------------------------------------- */

struct fparser {
	const char *src;
	size_t pos;
	char *err;
	size_t err_cap;
	int failed;
	/* arena building */
	char *arena;
	size_t arena_cap;
	size_t arena_len;
	struct fnode *nodes;
	size_t node_cnt;
	size_t node_cap;
};

static void
perr(struct fparser *p, size_t at, const char *msg)
{
	if (p->failed)
		return;
	p->failed = 1;
	if (p->err != NULL && p->err_cap > 0) {
		size_t n = (size_t) retrace_real_impls.real_snprintf(
			p->err, p->err_cap, "%s (at char %zu)", msg, at + 1);

		(void) n;
	}
}

static char *
arena_push(struct fparser *p, size_t n)
{
	char *out;

	if (p->arena_len + n > p->arena_cap) {
		size_t want = (p->arena_cap * 2) + n + 64;
		char *grown = (char *) retrace_real_impls.malloc(want);

		if (grown == NULL) {
			perr(p, p->pos, "out of memory");
			return NULL;
		}
		RI_MEMCPY(grown, p->arena, p->arena_len);
		retrace_real_impls.free(p->arena);
		p->arena = grown;
		p->arena_cap = want;
	}
	out = p->arena + p->arena_len;
	p->arena_len += n;
	return out;
}

static struct fnode *
node_new(struct fparser *p)
{
	if (p->node_cnt == p->node_cap) {
		size_t want = p->node_cap * 2 + 16;
		struct fnode *grown = (struct fnode *)
			retrace_real_impls.malloc(want * sizeof(*grown));

		if (grown == NULL) {
			perr(p, p->pos, "out of memory");
			return NULL;
		}
		RI_MEMCPY(grown, p->nodes,
			p->node_cnt * sizeof(*grown));
		retrace_real_impls.free(p->nodes);
		p->nodes = grown;
		p->node_cap = want;
	}
	RI_MEMSET(&p->nodes[p->node_cnt], 0,
		sizeof(struct fnode));
	return &p->nodes[p->node_cnt++];
}

static void
skip_ws(struct fparser *p)
{
	while (p->src[p->pos] == ' ' || p->src[p->pos] == '\t')
		p->pos++;
}

/* keyword check: the identifier at src[at..] equals `kw` as a whole word */
static int
at_keyword(struct fparser *p, const char *kw)
{
	size_t klen = strlen(kw);

	if (STR_NCMP(&p->src[p->pos], kw, klen) != 0)
		return 0;
	return !is_ident_char(p->src[p->pos + klen]);
}

static struct fnode *parse_or(struct fparser *p);

static int
parse_operand(struct fparser *p, struct foperand *out)
{
	skip_ws(p);

	if (p->src[p->pos] == '"') {
		size_t start = p->pos + 1;
		size_t len = 0;
		char *dst;

		p->pos = start;
		while (p->src[p->pos] != '"') {
			if (p->src[p->pos] == '\\' &&
			    p->src[p->pos + 1] != '\0') {
				p->pos += 2;
				len += 1;
				continue;
			}
			if (p->src[p->pos] == '\0') {
				perr(p, p->pos, "unterminated string");
				return 0;
			}
			p->pos++;
			len++;
		}

		dst = arena_push(p, len + 1);
		if (dst == NULL)
			return 0;

		{
			size_t r = start;
			size_t w = 0;

			while (r < p->pos) {
				if (p->src[r] == '\\' &&
				    p->src[r + 1] != '\0') {
					dst[w++] = p->src[r + 1];
					r += 2;
				} else {
					dst[w++] = p->src[r++];
				}
			}
			dst[w] = '\0';
		}
		p->pos++;	/* closing quote */

		out->kind = FO_STR;
		out->str = dst;
		out->str_len = len;
		return 1;
	}

	if (is_digit(p->src[p->pos]) ||
	    (p->src[p->pos] == '-' && is_digit(p->src[p->pos + 1]))) {
		int neg = p->src[p->pos] == '-';
		long long v = 0;
		size_t start = p->pos;

		p->pos += neg ? 1 : 0;
		while (is_digit(p->src[p->pos])) {
			v = v * 10 + (p->src[p->pos] - '0');
			p->pos++;
		}
		if (p->pos == start + (neg ? 1 : 0)) {
			p->pos = start;
			perr(p, start, "bad number");
			return 0;
		}
		out->kind = FO_NUM;
		out->num = neg ? -v : v;
		return 1;
	}

	if (at_keyword(p, "and") || at_keyword(p, "or") ||
	    at_keyword(p, "not")) {
		perr(p, p->pos, "expected operand (name, number, or \"string\")");
		return 0;
	}

	if (is_alpha(p->src[p->pos]) || p->src[p->pos] == '_') {
		size_t start = p->pos;

		while (is_ident_char(p->src[p->pos]))
			p->pos++;

		{
			size_t len = p->pos - start;
			char *dst = arena_push(p, len + 1);

			if (dst == NULL)
				return 0;
			RI_MEMCPY(dst, &p->src[start], len);
			dst[len] = '\0';

			if (STR_EQ(dst, "func"))
				out->kind = FO_FUNC;
			else if (STR_EQ(dst, "ret"))
				out->kind = FO_RET;
			else if (STR_EQ(dst, "caller"))
				out->kind = FO_CALLER;
			else
				out->kind = FO_PARAM;
			out->str = dst;
			out->str_len = len;
			return 1;
		}
	}

	perr(p, p->pos, "expected operand (name, number, or \"string\")");
	return 0;
}

static struct fnode *
parse_primary(struct fparser *p)
{
	struct fnode *n;

	skip_ws(p);

	if (p->src[p->pos] == '(') {
		p->pos++;
		n = parse_or(p);
		skip_ws(p);
		if (p->src[p->pos] != ')') {
			perr(p, p->pos, "expected ')'");
			return NULL;
		}
		p->pos++;
		return n;
	}

	n = node_new(p);
	if (n == NULL)
		return NULL;
	n->kind = FN_CMP;

	if (!parse_operand(p, &n->u.cmp.lhs))
		return NULL;

	skip_ws(p);
	{
		enum fop_kind op;
		int oplen;

		if (STR_NCMP(&p->src[p->pos], "==", 2) == 0) {
			op = FO_EQ;
			oplen = 2;
		} else if (STR_NCMP(&p->src[p->pos], "!=", 2) == 0) {
			op = FO_NE;
			oplen = 2;
		} else if (STR_NCMP(&p->src[p->pos], "<=", 2) == 0) {
			op = FO_LE;
			oplen = 2;
		} else if (STR_NCMP(&p->src[p->pos], ">=", 2) == 0) {
			op = FO_GE;
			oplen = 2;
		} else if (p->src[p->pos] == '<') {
			op = FO_LT;
			oplen = 1;
		} else if (p->src[p->pos] == '>') {
			op = FO_GT;
			oplen = 1;
		} else if (STR_NCMP(&p->src[p->pos], "!~", 2) == 0) {
			op = FO_NGLOB;
			oplen = 2;
		} else if (p->src[p->pos] == '~') {
			op = FO_GLOB;
			oplen = 1;
		} else {
			perr(p, p->pos,
			      "expected operator (== != < <= > >= ~ !~)");
			return NULL;
		}
		p->pos += (size_t) oplen;
		n->u.cmp.op = op;
	}

	if (!parse_operand(p, &n->u.cmp.rhs))
		return NULL;

	/* Type rule: relational operators need numeric operands;
	 * glob needs a string RHS (pattern); == / != compare
	 * like-typed operands.
	 */
	{
		int str_lhs = (n->u.cmp.lhs.kind == FO_STR ||
			       n->u.cmp.lhs.kind == FO_FUNC ||
			       n->u.cmp.lhs.kind == FO_CALLER);
		int str_rhs = (n->u.cmp.rhs.kind == FO_STR ||
			       n->u.cmp.rhs.kind == FO_FUNC ||
			       n->u.cmp.rhs.kind == FO_CALLER);
		enum fop_kind op = n->u.cmp.op;

		if ((op == FO_LT || op == FO_LE || op == FO_GT ||
		     op == FO_GE) && (str_lhs || str_rhs)) {
			perr(p, p->pos,
			      "relational operators need numeric operands");
			return NULL;
		}
		if ((op == FO_GLOB || op == FO_NGLOB) &&
		    n->u.cmp.rhs.kind != FO_STR) {
			perr(p, p->pos,
			      "glob pattern must be a string literal");
			return NULL;
		}
	}

	return n;
}

static struct fnode *
parse_not(struct fparser *p)
{
	skip_ws(p);

	if (at_keyword(p, "not")) {
		struct fnode *n;

		p->pos += 3;
		n = node_new(p);
		if (n == NULL)
			return NULL;
		n->kind = FN_NOT;
		n->u.child = parse_not(p);
		if (n->u.child == NULL)
			return NULL;
		return n;
	}
	return parse_primary(p);
}

static struct fnode *
parse_and(struct fparser *p)
{
	struct fnode *lhs = parse_not(p);

	if (lhs == NULL)
		return NULL;

	for (;;) {
		skip_ws(p);
		if (!at_keyword(p, "and"))
			return lhs;
		p->pos += 3;
		{
			struct fnode *rhs = parse_not(p);
			struct fnode *n;

			if (rhs == NULL)
				return NULL;
			n = node_new(p);
			if (n == NULL)
				return NULL;
			n->kind = FN_AND;
			n->u.bin.lhs = lhs;
			n->u.bin.rhs = rhs;
			lhs = n;
		}
	}
}

static struct fnode *
parse_or(struct fparser *p)
{
	struct fnode *lhs = parse_and(p);

	if (lhs == NULL)
		return NULL;

	for (;;) {
		skip_ws(p);
		if (!at_keyword(p, "or"))
			return lhs;
		p->pos += 2;
		{
			struct fnode *rhs = parse_and(p);
			struct fnode *n;

			if (rhs == NULL)
				return NULL;
			n = node_new(p);
			if (n == NULL)
				return NULL;
			n->kind = FN_OR;
			n->u.bin.lhs = lhs;
			n->u.bin.rhs = rhs;
			lhs = n;
		}
	}
}

struct retrace_filter_ast *
retrace_filter_compile(const char *expr, char *err, size_t err_cap)
{
	struct fparser p;
	struct fnode *root;
	struct retrace_filter_ast *ast;

	RI_MEMSET(&p, 0, sizeof(p));
	p.src = expr;
	p.err = err;
	p.err_cap = err_cap;
	if (err != NULL && err_cap > 0)
		err[0] = '\0';

	if (expr == NULL || expr[0] == '\0') {
		perr(&p, 0, "empty expression");
		return NULL;
	}

	/* seed the arenas */
	p.arena = (char *) retrace_real_impls.malloc(128);
	p.nodes = (struct fnode *) retrace_real_impls.malloc(
		16 * sizeof(struct fnode));
	if (p.arena == NULL || p.nodes == NULL) {
		retrace_real_impls.free(p.arena);
		retrace_real_impls.free(p.nodes);
		perr(&p, 0, "out of memory");
		return NULL;
	}
	p.arena_cap = 128;
	p.node_cap = 16;

	root = parse_or(&p);
	if (root == NULL || p.failed) {
		retrace_real_impls.free(p.arena);
		retrace_real_impls.free(p.nodes);
		return NULL;
	}

	skip_ws(&p);
	if (p.src[p.pos] != '\0') {
		perr(&p, p.pos, "unexpected trailing input");
		retrace_real_impls.free(p.arena);
		retrace_real_impls.free(p.nodes);
		return NULL;
	}

	/* the final AST owns the strings + nodes in one arena */
	ast = (struct retrace_filter_ast *)
		retrace_real_impls.malloc(sizeof(*ast));
	if (ast == NULL) {
		retrace_real_impls.free(p.arena);
		retrace_real_impls.free(p.nodes);
		perr(&p, 0, "out of memory");
		return NULL;
	}

	/* relocate nodes into the string arena's tail so one free()
	 * releases everything: copy node array, fix nothing (node
	 * pointers are RELATIVE to the nodes array base -- rebuild
	 * by copying in order; children were allocated after their
	 * parents, so a stable copy preserves relative order).
	 */
	{
		size_t nodes_bytes = p.node_cnt * sizeof(struct fnode);
		char *all = (char *) retrace_real_impls.malloc(
			p.arena_len + nodes_bytes);
		struct fnode *new_nodes;

		if (all == NULL) {
			retrace_real_impls.free(p.arena);
			retrace_real_impls.free(p.nodes);
			retrace_real_impls.free(ast);
			perr(&p, 0, "out of memory");
			return NULL;
		}
		RI_MEMCPY(all, p.arena, p.arena_len);
		new_nodes = (struct fnode *) (all + p.arena_len);
		RI_MEMCPY(new_nodes, p.nodes, nodes_bytes);

		/* pointers inside operands referenced the OLD arena --
		 * strings moved by (new_base - old_base).
		 */
		{
			ptrdiff_t shift = (char *) all - p.arena;
			size_t i;

			for (i = 0; i < p.node_cnt; i++) {
				struct fnode *n = &new_nodes[i];

				if (n->kind == FN_CMP) {
					if (n->u.cmp.lhs.str != NULL)
						n->u.cmp.lhs.str += shift;
					if (n->u.cmp.rhs.str != NULL)
						n->u.cmp.rhs.str += shift;
				}
			}
			/* node-to-node pointers referenced the old
			 * nodes array -- shift by their delta.
			 */
			shift = (char *) new_nodes - (char *) p.nodes;
			for (i = 0; i < p.node_cnt; i++) {
				struct fnode *n = &new_nodes[i];

				if (n->kind == FN_CMP)
					continue;
				if (n->kind == FN_NOT) {
					if (n->u.child != NULL)
						n->u.child = (struct fnode *)
							((char *) n->u.child + shift);
				} else {
					if (n->u.bin.lhs != NULL)
						n->u.bin.lhs = (struct fnode *)
							((char *) n->u.bin.lhs + shift);
					if (n->u.bin.rhs != NULL)
						n->u.bin.rhs = (struct fnode *)
							((char *) n->u.bin.rhs + shift);
				}
			}
			/* root was the LAST allocated top-level node...
			 * no: root is the outermost, allocated last in
			 * the or/and chains. Recompute by pointer shift
			 * from the old array.
			 */
			root = (struct fnode *)
				((char *) root + shift);
		}

		retrace_real_impls.free(p.arena);
		retrace_real_impls.free(p.nodes);

		ast->arena = all;
		ast->arena_len = p.arena_len + nodes_bytes;
		ast->root = root;
		ast->node_cnt = p.node_cnt;
	}

	return ast;
}

void
retrace_filter_free(struct retrace_filter_ast *ast)
{
	if (ast == NULL)
		return;
	retrace_real_impls.free(ast->arena);
	retrace_real_impls.free(ast);
}

/* ---- evaluation --------------------------------------------------------- */

/* Resolve an operand to a string (returns NULL when it is not
 * string-typed or the value is unavailable).
 */
static const char *
operand_str(const struct foperand *o, const struct ThreadContext *t_ctx,
	    const char *func_name)
{
	switch (o->kind) {
	case FO_STR:
	case FO_FUNC:
		return o->kind == FO_FUNC ? func_name : o->str;
	case FO_CALLER:
		return NULL;	/* resolved by the action layer */
	case FO_PARAM: {
		int i;

		for (i = 0; i < t_ctx->params_cnt; i++) {
			const struct FuncParam *fp = &t_ctx->params[i];

			if (STR_EQ(fp->param_meta.name, o->str)) {
				if ((fp->param_meta.modifiers &
				     0x1 /* CDM_POINTER */) &&
				    fp->val != 0 &&
				    STR_EQ(fp->param_meta.ref_type_name, "sz"))
					return (const char *)
						(intptr_t) fp->val;
				return NULL;
			}
		}
		return NULL;
	}
	default:
		return NULL;
	}
}

static int
operand_num(const struct foperand *o, const struct ThreadContext *t_ctx,
	    long long *out, int *known)
{
	switch (o->kind) {
	case FO_NUM:
		*out = o->num;
		*known = 1;
		return 1;
	case FO_RET:
		*out = (long long) t_ctx->ret_val;
		*known = 1;
		return 1;
	case FO_PARAM: {
		int i;

		for (i = 0; i < t_ctx->params_cnt; i++) {
			const struct FuncParam *fp = &t_ctx->params[i];

			if (STR_EQ(fp->param_meta.name, o->str)) {
				/*
				 * string params are compared with ~ /
				 * == on strings, never as numbers
				 */
				if ((fp->param_meta.modifiers & 0x1) &&
				    fp->val != 0 &&
				    STR_EQ(fp->param_meta.ref_type_name,
					   "sz"))
					return 0;
				*out = (long long) fp->val;
				*known = 1;
				return 1;
			}
		}
		*known = 0;
		return 1;
	}
	default:
		return 0;	/* string-typed operand: not numeric */
	}
}

static int
eval_node(const struct fnode *n, const struct ThreadContext *t_ctx,
	  const char *func_name)
{
	switch (n->kind) {
	case FN_OR:
		return eval_node(n->u.bin.lhs, t_ctx, func_name) ||
		       eval_node(n->u.bin.rhs, t_ctx, func_name);
	case FN_AND:
		return eval_node(n->u.bin.lhs, t_ctx, func_name) &&
		       eval_node(n->u.bin.rhs, t_ctx, func_name);
	case FN_NOT:
		return !eval_node(n->u.child, t_ctx, func_name);
	case FN_CMP: {
		const struct foperand *lhs = &n->u.cmp.lhs;
		const struct foperand *rhs = &n->u.cmp.rhs;
		enum fop_kind op = n->u.cmp.op;
		long long l, r;
		int lknown = 1, rknown = 1;

		switch (op) {
		case FO_GLOB:
		case FO_NGLOB: {
			const char *s = operand_str(lhs, t_ctx, func_name);
			int m;

			if (s == NULL)
				return 0;
			m = retrace_filter_glob_match(rhs->str, s);
			return op == FO_GLOB ? m : !m;
		}
		default:
			break;
		}

		if (!operand_num(lhs, t_ctx, &l, &lknown) ||
		    !operand_num(rhs, t_ctx, &r, &rknown))
			return 0;	/* string vs relational/typed mix */
		if (!lknown || !rknown)
			return 0;	/* unknown param: no match */

		switch (op) {
		case FO_EQ: return l == r;
		case FO_NE: return l != r;
		case FO_LT: return l < r;
		case FO_LE: return l <= r;
		case FO_GT: return l > r;
		case FO_GE: return l >= r;
		default: return 0;
		}
	}
	}
	return 0;
}

int
retrace_filter_eval(const struct retrace_filter_ast *ast,
		    const struct ThreadContext *t_ctx,
		    const char *func_name)
{
	if (ast == NULL)
		return 1;
	if (t_ctx == NULL)
		return 0;
	return eval_node(ast->root, t_ctx, func_name);
}
