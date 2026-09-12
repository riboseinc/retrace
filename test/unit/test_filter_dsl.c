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
 * Unit: the filter expression language (TODO.impl/08) --
 * STANDALONE, the card-06 lesson: the parser and evaluator are
 * compiled against fakes, no engine link, no interposition.
 *
 * Fixed cases pin the glob matcher, precedence, parens, not,
 * builtins, typing rules, and error offsets. The property part
 * builds random boolean trees over numeric params, renders
 * each tree to a random-shaped expression string, compiles it,
 * and evaluates over generated calls -- asserting the compiled
 * predicate agrees with the truth computed from the same tree
 * by construction. Deterministic (fixed seed).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine.h"
#include "filter_dsl.h"
#include "real_impls.h"

static int tests_run;
static int tests_pass;
static int tests_fail;

#define TEST(name) do { \
	tests_run++; \
	printf("  TEST %s ... ", #name); \
	test_##name(); \
	tests_pass++; \
	printf("OK\n"); \
} while (0)

#define CHECK(cond) do { \
	if (!(cond)) { \
		printf("FAIL [%s:%d] %s\n", __FILE__, __LINE__, #cond); \
		tests_fail++; \
		return; \
	} \
} while (0)

/* ---- fakes ------------------------------------------------------------- */

struct RetraceRealImpls retrace_real_impls;

static void *fake_malloc(size_t n) { return malloc(n); }
static void fake_free(void *p) { free(p); }
static void *fake_memset(void *d, int c, size_t n) { return memset(d, c, n); }
static void *fake_memcpy(void *d, const void *s, size_t n) { return memcpy(d, s, n); }
static int fake_strcmp(const char *a, const char *b) { return strcmp(a, b); }
static int fake_strncmp(const char *a, const char *b, size_t n) { return strncmp(a, b, n); }
static size_t fake_strlen(const char *s) { return strlen(s); }
static int fake_snprintf(char *s, size_t n, const char *f, ...)
{
	int rc;
	va_list ap;

	va_start(ap, f);
	rc = vsnprintf(s, n, f, ap);
	va_end(ap);
	return rc;
}

/* ---- glob --------------------------------------------------------------- */

static void test_glob_basics(void)
{
	CHECK(retrace_filter_glob_match("*.log", "app.log"));
	CHECK(!retrace_filter_glob_match("*.log", "app.txt"));
	CHECK(retrace_filter_glob_match("a?c", "abc"));
	CHECK(!retrace_filter_glob_match("a?c", "abbc"));
	CHECK(retrace_filter_glob_match("*", ""));
	CHECK(retrace_filter_glob_match("[a-c]x", "bx"));
	CHECK(!retrace_filter_glob_match("[a-c]x", "dx"));
	CHECK(retrace_filter_glob_match("[!a-c]x", "dx"));
	CHECK(!retrace_filter_glob_match("[!a-c]x", "ax"));
	CHECK(retrace_filter_glob_match("[]]x", "]x"));
	CHECK(retrace_filter_glob_match("\\*x", "*x"));
	CHECK(!retrace_filter_glob_match("\\*x", "ax"));
	CHECK(retrace_filter_glob_match("*.*.conf", "a.b.conf"));
	CHECK(retrace_filter_glob_match("etc/*", "etc/"));
}

/* ---- parse errors ------------------------------------------------------- */

static void expect_compile_fails(const char *expr, const char *needle)
{
	char err[128];
	struct retrace_filter_ast *ast = retrace_filter_compile(
		expr, err, sizeof(err));

	if (ast != NULL) {
		printf("FAIL: '%s' compiled but should not\n", expr);
		tests_fail++;
		retrace_filter_free(ast);
		return;
	}
	if (strstr(err, needle) == NULL) {
		printf("FAIL: '%s': error '%s' lacks '%s'\n",
			expr, err, needle);
		tests_fail++;
		return;
	}
}

static void expect_compiles(const char *expr)
{
	char err[128];
	struct retrace_filter_ast *ast = retrace_filter_compile(
		expr, err, sizeof(err));

	if (ast == NULL) {
		printf("FAIL: '%s': %s\n", expr, err);
		tests_fail++;
		return;
	}
	retrace_filter_free(ast);
}

static void test_compile_errors(void)
{
	expect_compile_fails("", "empty");
	expect_compile_fails("a ==", "expected operand");
	expect_compile_fails("(a == 1", "expected ')'");
	expect_compile_fails("a == 1)", "trailing");
	expect_compile_fails("\"unterminated == 1", "unterminated string");
	/*
	 * a STRING param under a relational op compiles (params
	 * are dynamically typed) and evaluates FALSE at runtime
	 * -- pinned in eval_basics, not here
	 */
	expect_compiles("s < 3");
	expect_compile_fails("3 > \"x\"", "numeric");
	expect_compile_fails("a ~ b", "string literal");
	expect_compile_fails("and", "expected operand");
	expect_compiles("a == 1");
	expect_compiles("not (a == 1 or b != 2) and c <= 3");
	expect_compiles("func ~ \"open*\" and path ~ \"*.log\"");
	expect_compiles("ret >= 0 or ret == -1");
}

/* ---- evaluation --------------------------------------------------------- */

/* a call with two numeric params (a, b) and one string param (s) */
static struct ThreadContext *mkcall(long long a, long long b,
				    const char *s, const char *func)
{
	static struct ThreadContext ctx;
	static char sbuf[64];

	memset(&ctx, 0, sizeof(ctx));
	strncpy(sbuf, s ? s : "", sizeof(sbuf) - 1);

	ctx.prototype = NULL;
	ctx.params_cnt = 3;
	ctx.params[0].param_meta.name[0] = 'a';
	ctx.params[0].param_meta.name[1] = '\0';
	ctx.params[0].val = (intptr_t) a;
	ctx.params[1].param_meta.name[0] = 'b';
	ctx.params[1].param_meta.name[1] = '\0';
	ctx.params[1].val = (intptr_t) b;
	ctx.params[2].param_meta.name[0] = 's';
	ctx.params[2].param_meta.name[1] = '\0';
	ctx.params[2].param_meta.modifiers = 0x1;	/* CDM_POINTER */
	strcpy(ctx.params[2].param_meta.ref_type_name, "sz");
	ctx.params[2].val = (intptr_t) (s != NULL ? sbuf : 0);
	ctx.ret_val = a;
	(void) func;
	return &ctx;
}

static int evals(const char *expr, struct ThreadContext *c,
		 const char *func)
{
	char err[128];
	struct retrace_filter_ast *ast = retrace_filter_compile(
		expr, err, sizeof(err));
	int r;

	if (ast == NULL) {
		printf("FAIL compile '%s': %s\n", expr, err);
		tests_fail++;
		return -1;
	}
	r = retrace_filter_eval(ast, c, func);
	retrace_filter_free(ast);
	return r;
}

static void test_eval_basics(void)
{
	CHECK(evals("a == 1", mkcall(1, 0, NULL, "open"), "open") == 1);
	CHECK(evals("a == 1", mkcall(2, 0, NULL, "open"), "open") == 0);
	CHECK(evals("a != 1", mkcall(2, 0, NULL, "open"), "open") == 1);
	CHECK(evals("a < b", mkcall(1, 2, NULL, "open"), "open") == 1);
	CHECK(evals("a >= b", mkcall(1, 2, NULL, "open"), "open") == 0);
	CHECK(evals("s ~ \"*.log\"", mkcall(0, 0, "app.log", "open"),
		    "open") == 1);
	CHECK(evals("s ~ \"*.log\"", mkcall(0, 0, "app.txt", "open"),
		    "open") == 0);
	CHECK(evals("func ~ \"open*\"", mkcall(0, 0, NULL, "openat"),
		    "openat") == 1);
	CHECK(evals("ret == 5", mkcall(5, 0, NULL, "x"), "x") == 1);
	/* unknown param: no match, never an error */
	CHECK(evals("zz == 1", mkcall(1, 0, NULL, "open"), "open") == 0);
	/* precedence: and binds tighter than or */
	CHECK(evals("a == 1 or a == 2 and b == 3",
		    mkcall(1, 0, NULL, "f"), "f") == 1);
	CHECK(evals("(a == 1 or a == 2) and b == 3",
		    mkcall(1, 0, NULL, "f"), "f") == 0);
	/* not */
	CHECK(evals("not a == 1", mkcall(2, 0, NULL, "f"), "f") == 1);
	CHECK(evals("not not a == 1", mkcall(2, 0, NULL, "f"), "f") == 0);
	/* string param under relational: runtime-false, never true */
	CHECK(evals("s < 3", mkcall(0, 0, "x", "f"), "f") == 0);
}

/* ---- property part: random trees, truth by construction ---------------- */

static unsigned long long rng_state = 0x5eed1234ULL;

static unsigned long long rng(void)
{
	rng_state = rng_state * 6364136223846793005ULL + 1442695040888963407ULL;
	return rng_state >> 17;
}

static long long rng_num(void)
{
	return (long long) (rng() % 21) - 10;
}

/* a generated tree node */
enum gkind { G_CMP, G_NOT, G_AND, G_OR };
enum gop { G_EQ, G_NE, G_LT, G_LE, G_GT, G_GE };

struct gtree {
	enum gkind kind;
	/* G_CMP */
	int lhs_param;		/* 0=a, 1=b */
	enum gop op;
	long long rhs;
	/* G_NOT/G_AND/G_OR */
	struct gtree *l, *r;
};

static int gdepth;

static void gtree_gen(struct gtree *out)
{
	if (gdepth >= 3 || (rng() % 100) < 45) {
		out->kind = G_CMP;
		out->lhs_param = (int) (rng() % 2);
		out->op = (enum gop) (rng() % 6);
		out->rhs = rng_num();
		out->l = out->r = NULL;
		return;
	}

	gdepth++;
	out->l = malloc(sizeof(struct gtree));
	out->r = malloc(sizeof(struct gtree));
	out->kind = (rng() % 100) < 25 ? G_NOT : (rng() % 2 ? G_AND : G_OR);
	if (out->kind == G_NOT) {
		gtree_gen(out->l);
		free(out->r);
		out->r = NULL;
	} else {
		gtree_gen(out->l);
		gtree_gen(out->r);
	}
	gdepth--;
}

static void gtree_free(struct gtree *t)
{
	if (t == NULL)
		return;
	gtree_free(t->l);
	gtree_free(t->r);
	free(t);
}

static const char *gop_str[] = { "==", "!=", "<", "<=", ">", ">=" };

static void gtree_render(const struct gtree *t, char *buf, size_t cap)
{
	if (t->kind == G_CMP) {
		snprintf(buf, cap, "%s %s %lld",
			t->lhs_param == 0 ? "a" : "b",
			gop_str[t->op], t->rhs);
		return;
	}
	if (t->kind == G_NOT) {
		snprintf(buf, cap, "not ");
		gtree_render(t->l, buf + 4, cap - 4);
		return;
	}
	{
		size_t half = cap / 2;

		snprintf(buf, cap, "(");
		gtree_render(t->l, buf + 1, half);
		strcat(buf, t->kind == G_AND ? " and " : " or ");
		gtree_render(t->r, buf + strlen(buf), cap - strlen(buf) - 1);
		strcat(buf, ")");
	}
}

static int gtree_truth(const struct gtree *t, long long a, long long b)
{
	long long l;

	switch (t->kind) {
	case G_CMP:
		l = t->lhs_param == 0 ? a : b;
		switch (t->op) {
		case G_EQ: return l == t->rhs;
		case G_NE: return l != t->rhs;
		case G_LT: return l < t->rhs;
		case G_LE: return l <= t->rhs;
		case G_GT: return l > t->rhs;
		case G_GE: return l >= t->rhs;
		}
		return 0;
	case G_NOT: return !gtree_truth(t->l, a, b);
	case G_AND:
		return gtree_truth(t->l, a, b) && gtree_truth(t->r, a, b);
	case G_OR:
		return gtree_truth(t->l, a, b) || gtree_truth(t->r, a, b);
	}
	return 0;
}

static void test_property_truth_tables(void)
{
	int tree_i;

	for (tree_i = 0; tree_i < 60; tree_i++) {
		struct gtree t;
		char expr[256];
		char err[128];
		struct retrace_filter_ast *ast;
		int call_i;
		int bad = 0;

		gdepth = 0;
		gtree_gen(&t);
		gtree_render(&t, expr, sizeof(expr));

		ast = retrace_filter_compile(expr, err, sizeof(err));
		if (ast == NULL) {
			printf("FAIL compile '%s': %s\n", expr, err);
			tests_fail++;
			gtree_free(t.l);
			gtree_free(t.r);
			continue;
		}

		for (call_i = 0; call_i < 40; call_i++) {
			long long a = rng_num();
			long long b = rng_num();
			struct ThreadContext *c = mkcall(a, b, NULL, "f");
			int want = gtree_truth(&t, a, b);
			int got = retrace_filter_eval(ast, c, "f");

			if (want != got) {
				printf("FAIL '%s' a=%lld b=%lld: want %d "
				       "got %d\n", expr, a, b, want, got);
				bad = 1;
				break;
			}
		}

		retrace_filter_free(ast);
		gtree_free(t.l);
		gtree_free(t.r);
		if (bad) {
			tests_fail++;
			return;
		}
	}
}

int
main(void)
{
	memset(&retrace_real_impls, 0, sizeof(retrace_real_impls));
	retrace_real_impls.malloc = fake_malloc;
	retrace_real_impls.free = fake_free;
	retrace_real_impls.memset = fake_memset;
	retrace_real_impls.memcpy = fake_memcpy;
	retrace_real_impls.strcmp = fake_strcmp;
	retrace_real_impls.strncmp = fake_strncmp;
	retrace_real_impls.strlen = fake_strlen;
	retrace_real_impls.real_snprintf = fake_snprintf;

	printf("filter DSL (TODO.impl/08)\n");

	TEST(glob_basics);
	TEST(compile_errors);
	TEST(eval_basics);
	TEST(property_truth_tables);

	printf("%d run, %d pass, %d fail\n",
		tests_run, tests_pass, tests_fail);
	return tests_fail == 0 ? 0 : 1;
}
