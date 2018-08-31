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

#ifndef __RTR2_CONF_H__
#define __RTR2_CONF_H__

#define RTR2_DEFAULT_FUNC_NAME             "*"
#define RTR2_DEFAULT_ACT_NAME1             "log_params"
#define RTR2_DEFAULT_ACT_NAME2             "call_real"

/*
 * retrace v2 log level
 */

enum RTR2_LOG_LEVEL {
	RTR2_LOG_LEVEL_SILENT = 1,
	RTR2_LOG_LEVEL_NORMAL,
	RTR2_LOG_LEVEL_DEBUG,
	RTR2_LOG_LEVEL_VERBOSE,
};

/*
 * retrace v2 match types
 */

enum RTR2_MATCH_TYPE {
	RTR2_MATCH_TYPE_INT = 0,
	RTR2_MATCH_TYPE_STRING,
	RTR2_MATCH_TYPE_ARRAY_INT,
	RTR2_MATCH_TYPE_ARRAY_STRING,
	RTR2_MATCH_TYPE_CHAR,
	RTR2_MATCH_TYPE_ANY_INT,                    /* match data is ANY for integer type */
	RTR2_MATCH_TYPE_ANY_STRING,                 /* match data is ANY for string type */
	RTR2_MATCH_TYPE_ANY_RANDOM,                 /* match data is ANY && new data is RANDOM */
	RTR2_MATCH_TYPE_UNKNOWN
};

/*
 * retrace v2 action info
 */

typedef struct rtr2_action {
	char *name;

	char *param_name;
	enum RTR2_MATCH_TYPE match_type;

	void *match_data;
	void *new_data;

	/* these fields are available only when match type is array */
	int match_data_num;
	int new_data_num;
} rtr2_action_t;

/*
 * retrace v2 function info
 */

typedef struct rtr2_func {
	char *name;

	rtr2_action_t *actions;
	int actions_count;

	enum RTR2_LOG_LEVEL log_level;                       /* if log_level is not set, then it's same with global one */
} rtr2_func_t;

/*
 * retrace v2 configuration
 */

typedef struct rtr2_conf {
	char *log_path;
	enum RTR2_LOG_LEVEL log_level;

	rtr2_func_t *funcs;
	int funcs_count;

	int use_default;
} rtr2_conf_t;


/*
 * retrace v2 configuration API functions
 */

int rtr2_config_init(void);
void rtr2_config_free(void);

extern rtr2_conf_t *g_rtr2_config;

#endif /* __RTR2_CONF_H__ */
