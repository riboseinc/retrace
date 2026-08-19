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

#include "thread_context.h"


#include "real_impls.h"
#include "logger.h"

static rc_tss_t thread_ctx_key;

/*
 * Per-thread destructor. Frees the thread's ThreadContext.
 *
 * Does NOT delete the global pthread_key -- that would race
 * with other threads still using it (the flusher thread from
 * TODO.complete/19 PR C is the case that exposed this: it
 * exits before the producer threads, and deleting the global
 * key from its destructor invalidated the key for every
 * still-running producer, causing SIGBUS on the next
 * pthread_getspecific call). The key is process-global and
 * lives until exit; the OS reclaims it.
 */
static void thread_ctx_destructor(void *thread_ctx)
{
	retrace_real_impls.free(thread_ctx);
}

int retrace_thread_context_init(void)
{
	int rc = retrace_real_impls.rc_tss_create(&thread_ctx_key,
		thread_ctx_destructor);

	if (rc)
		log_err("failed to create pthread_key");

	return rc;
}

struct ThreadContext *retrace_thread_context_get(void)
{
	struct ThreadContext *thread_ctx;

	thread_ctx = (struct ThreadContext *)
		retrace_real_impls.rc_tss_get(thread_ctx_key);

	if (thread_ctx == NULL) {
		thread_ctx = (struct ThreadContext *)
			retrace_real_impls.malloc(sizeof(struct ThreadContext));

		if (thread_ctx == NULL)
			return NULL;

		if (retrace_real_impls.rc_tss_set(
			thread_ctx_key, thread_ctx)) {

			retrace_real_impls.free(thread_ctx);
			return NULL;
		}

		retrace_real_impls.memset(thread_ctx, 0, sizeof(*thread_ctx));
	}

	return thread_ctx;
}
