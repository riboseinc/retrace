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

#include <stdlib.h>
#include <pthread.h>

struct RetraceRealImpls {
	int (*pthread_key_create)(pthread_key_t *key,
		void (*destructor)(void *));

	void *(*pthread_getspecific)(pthread_key_t key);
	int (*pthread_setspecific)(pthread_key_t key, const void *value);
	int (*pthread_key_delete)(pthread_key_t key);

	void *(*malloc)(size_t size);
	void *(*free)(void *ptr);

	int (*atoi)(const char *nptr);

	void *(*memset)(void *s, int c, size_t n);
	int (*strncmp)(const char *s1, const char *s2, size_t n);
	void *(*memcpy)(void *dest, const void *src, size_t n);
	size_t (*strlen)(const char *s);
	int (*strcmp)(const char *s1, const char *s2);
	char *(*strcpy)(char *dest, const char *src);

	void *(*dlopen)(const char *filename, int flag);
	void *(*dlsym)(void *handle, const char *symbol);

	int (*sprintf)(char *str, const char *format, ...);
	int (*snprintf)(char *str, size_t size, const char *format, ...);
};

extern struct RetraceRealImpls retrace_real_impls;

int retrace_real_impls_init_safe(void);
