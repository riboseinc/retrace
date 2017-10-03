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

#include "config.h"
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "conf.h"

int
read_config_file(struct config *config, const char *filename)
{
	FILE *f;
	size_t buflen = 0;
	char *buf = NULL, *key, *value;
	int fail = 0;

	f = fopen(filename, "r");
	if (f == NULL)
		return 1;

	while (!fail && getline(&buf, &buflen, f) != -1) {
		key = strtok(buf, ",\n");
		if (key != NULL) {
			value = strtok(NULL, "\n");
			if (!add_config_entry(config, key, value))
				fail = 1;
		}
	}

	fclose(f);
	free(buf);

	return (!fail);
}

int
add_config_entry(struct config *config, const char *key, const char *value)
{
	struct config_entry *pentry;
	size_t klen, vlen;

	klen = strlen(key) + 1;
	vlen = (value != NULL) ? strlen(value) + 1 : 0;

	pentry = malloc(sizeof(struct config_entry) + klen + vlen);
	if (pentry == NULL)
		return 0;

	pentry->key = (char *)&pentry[1];
	strcpy(pentry->key, key);

	if (value != NULL) {
		pentry->value = pentry->key + klen;
		strcpy(pentry->value, value);
	} else
		pentry->value = NULL;

	STAILQ_INSERT_TAIL(config, pentry, next);

	return 1;
}

void
split_config(struct config *c1, struct config *c2, ...)
{
	struct config c;
	struct config_entry *pe;
	va_list ap;
	const char *key;

	STAILQ_INIT(&c);
	STAILQ_CONCAT(&c, c1);
	while (!STAILQ_EMPTY(&c)) {
		pe = STAILQ_FIRST(&c);
		STAILQ_REMOVE_HEAD(&c, next);
		va_start(ap, c2);
		for(;;) {
			key = va_arg(ap, const char *);
			if (key == NULL || !strcmp(key, pe->key))
				break;
		}
		va_end(ap);
		STAILQ_INSERT_TAIL((key == NULL) ? c1 : c2, pe, next);
	}
}

void
free_config_entries(struct config *c)
{
	struct config_entry *pe;

	while (!STAILQ_EMPTY(c)) {
		pe = STAILQ_FIRST(c);
		STAILQ_REMOVE_HEAD(c, next);
		free(pe);
	}
}
