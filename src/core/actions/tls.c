/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * The TLS content lane (TODO.impl/13): what was exfiltrated,
 * not just where. Two actions, one surface:
 *
 * tls_content -- SSL_write/SSL_read plaintext summaries to the
 * evidence plane. Reads the buffer param bounded by the
 * CALLER-SUPPLIED length (TLS buffers are not NUL-terminated);
 * a printable first line (an HTTP request line, a header)
 * rides the event, binary payloads contribute their length.
 * Emitted through the agent fan-out -- card 01's redaction
 * transform applies (secrets in evidence are a feature; leaked
 * secrets are not).
 *
 *   { "func_name": "SSL_write_ex",  -- OpenSSL 3's data path
 *                                  -- (classic SSL_write too)
 *     "actions": [
 *       { "action_name": "tls_content",
 *         "action_params": {"dir": "w"} },
 *       { "action_name": "call_real" } ] }
 *
 * tls_keylog -- SSL_CTX_new's returned context gets our
 * SSL_CTX_set_keylog_callback (the same injection
 * SSLKEYLOGFILE uses internally): every secret line the app's
 * OpenSSL would log lands in RETRACE_TLS_KEYLOG (default
 * retrace-keylog.log). Providers without the callback
 * (LibreSSL) are named and skipped, never fatal.
 *
 *   { "func_name": "SSL_CTX_new",
 *     "actions": [
 *       { "action_name": "call_real" },
 *       { "action_name": "tls_keylog" } ] }
 */

#include <stddef.h>

#ifndef _WIN32
#include <dlfcn.h>
#endif

#include "actions.h"
#include "action_utils.h"
#include "agent.h"
#include "logger.h"
#include "real_impls.h"
#include "tls_summary.h"

#define TLS_LINE_MAX 256

static int ia_tls_content(struct ThreadContext *t_ctx,
			  const JSON_Object *action_params)
{
	const char *dir = "w";
	const char *kv[6];
	char line[TLS_LINE_MAX];
	char len_s[24];
	size_t line_len;
	long buf_len = -1;
	int buf_idx;
	int len_idx;

	if (action_params != NULL) {
		const char *d =
			json_object_get_string(action_params, "dir");

		if (d != NULL && d[0] != '\0')
			dir = d;
	}
	buf_idx = retrace_action_find_param(t_ctx, "buf");
	if (buf_idx < 0)
		return 0;
	len_idx = retrace_action_find_param(t_ctx, "num");
	if (len_idx >= 0)
		buf_len = (long)t_ctx->params[len_idx].val;
	if (buf_len < 0)
		buf_len = TLS_LINE_MAX;	/* no length param: cap hard */
	if (buf_len == 0)
		return 0;

	line_len = retrace_tls_first_line(
		(const char *)t_ctx->params[buf_idx].val,
		(size_t)buf_len, line, sizeof(line));

	retrace_real_impls.real_snprintf(len_s, sizeof(len_s), "%ld",
		buf_len);
	if (line_len > 0)
		log_info("tls_content: [%s] %s", dir, line);
	else
		log_info("tls_content: [%s] %s bytes (binary)", dir,
			len_s);

	kv[0] = "dir";
	kv[1] = dir;
	kv[2] = "len";
	kv[3] = len_s;
	kv[4] = "line";
	kv[5] = line_len > 0 ? line : "";
	(void)retrace_agent_emit_event("retrace.tls.plain", kv, 3);
	return 0;
}

static void tls_keylog_cb(const void *ssl, const char *line)
{
	const char *path;
	FILE *f;

	(void)ssl;
	if (line == NULL)
		return;
	path = retrace_real_impls.getenv != NULL ?
		retrace_real_impls.getenv("RETRACE_TLS_KEYLOG") : NULL;
	if (path == NULL || path[0] == '\0')
		path = "retrace-keylog.log";
	f = retrace_real_impls.fopen(path, "a");
	if (f == NULL)
		return;
	retrace_real_impls.fprintf(f, "%s\n", line);
	retrace_real_impls.fclose(f);
}

static int ia_tls_keylog(struct ThreadContext *t_ctx,
			 const JSON_Object *action_params)
{
	typedef void (*set_keylog_cb_fn)(void *ctx,
		void (*cb)(const void *, const char *));
	static set_keylog_cb_fn setter;
	const char *kv[2];

	(void)action_params;
	if (t_ctx->ret_val == 0)
		return 0;	/* ctx alloc failed; nothing to arm */
#ifndef _WIN32
	if (setter == NULL) {
		if (retrace_real_impls.dlsym == NULL)
			return 0;
		setter = (set_keylog_cb_fn)
			retrace_real_impls.dlsym(RTLD_NEXT,
				"SSL_CTX_set_keylog_callback");
		if (setter == NULL) {
			/* providers without the slot (LibreSSL):
			 * named, skipped, never fatal
			 */
			log_info("tls_keylog: provider has no "
				"keylog callback; skipped");
			return 0;
		}
	}
	setter((void *)(intptr_t)t_ctx->ret_val, tls_keylog_cb);
	kv[0] = "file";
	kv[1] = "keylog";
	(void)retrace_agent_emit_event("retrace.tls.keylog", kv, 1);
#endif
	return 0;
}

retrace_actions_define_package(tls) = {
	{
		.name = "tls_content",
		.action = ia_tls_content
	},
	{
		.name = "tls_keylog",
		.action = ia_tls_keylog
	}
};
