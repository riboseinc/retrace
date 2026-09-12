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

#ifndef RETRACE_CORE_FILTER_DSL_H_
#define RETRACE_CORE_FILTER_DSL_H_

#include <stddef.h>
#include <stdint.h>

/* Forward declaration only: the config validator compiles
 * expressions without evaluating them, so the header carries
 * no engine dependency (the .c file owns the ThreadContext).
 */
struct ThreadContext;

/*
 * The filter expression language (TODO.impl/08, ADR-0016's
 * grammar-family sibling): the `filter` action's `expr` param.
 *
 *   disj   := conj ("or" conj)*
 *   conj   := neg  ("and" neg)*
 *   neg    := "not" neg | primary
 *   primary:= "(" disj ")" | operand OP operand
 *   OP     := == != < <= > >= ~ !~
 *
 * with `expr` = disj (the start production).
 *   operand:= ident | number | "quoted string"
 *
 * Operands: a param NAME (evaluated against the call's params;
 * string-typed params compare as strings, others as integers),
 * a number, or a quoted string. Builtins: `func` (the current
 * function's name, glob-comparable), `ret` (the thread
 * context's ret_val -- meaningful after a call_real action),
 * `caller` (the caller's symbol via dladdr, glob-comparable).
 * `~` is a glob match (`*`, `?`, `[...]`; no fnmatch -- this
 * is the portable matcher every lane shares).
 *
 * Relational operators (< <= > >=) require numeric operands;
 * mixing a string with them is a COMPILE error, not a runtime
 * surprise. An ident the call does not have (unknown param)
 * evaluates FALSE -- the filter aborts the script, same as the
 * legacy single-comparison form.
 *
 * A compiled predicate is an opaque tree, arena-allocated in
 * one block: compile once (at config validation), evaluate per
 * call. Errors report a message and the 1-based character
 * offset in the source expression.
 */

struct retrace_filter_ast;

/* Compile `expr`. Returns NULL on error and fills `err`
 * (reason + offset) when `err_cap` > 0. The result is freed
 * with retrace_filter_free().
 */
struct retrace_filter_ast *retrace_filter_compile(
	const char *expr, char *err, size_t err_cap);

void retrace_filter_free(struct retrace_filter_ast *ast);

/* Evaluate against a call. `func_name` is the intercepted
 * function (t_ctx->prototype->name on the engine path).
 * Returns 1 (match -- continue the script) or 0 (no match).
 * NULL ast matches everything (zero-cost when absent).
 */
int retrace_filter_eval(const struct retrace_filter_ast *ast,
	const struct ThreadContext *t_ctx, const char *func_name);

/*
 * Portable glob: `*`, `?`, `[...]` classes with `!`/`^`
 * negation and `-` ranges; `\` escapes. This is the single
 * matcher for expressions, caller globs, and anything else
 * that grows the grammar family (no fnmatch -- Windows).
 */
int retrace_filter_glob_match(const char *pattern, const char *text);

#endif /* RETRACE_CORE_FILTER_DSL_H_ */
