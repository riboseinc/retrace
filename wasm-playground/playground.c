/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * retrace wasm playground demo (TODO.complete/36 P2).
 *
 * A minimal self-contained C module that simulates retrace's
 * intercept-log-trace pattern in WebAssembly.
 *
 * Compile:
 *   clang --target=wasm32 -O2 -nostdlib \
 *     -Wl,--no-entry -Wl,--export=run \
 *     -Wl,--export=get_trace \
 *     -o playground.wasm playground.c
 */

#define TRACE_BUF_SIZE 8192

static char trace_buf[TRACE_BUF_SIZE];
static int trace_pos;
static char heap[16384];
static int heap_pos;

static int my_strlen(const char *s)
{
	int n = 0;

	while (s[n])
		n++;
	return n;
}

static void my_strcpy(char *dst, const char *src)
{
	while ((*dst++ = *src++))
		;
}

static void my_memset(void *dst, int c, int n)
{
	char *d = (char *)dst;
	int i;

	for (i = 0; i < n; i++)
		d[i] = (char)c;
}

static void *my_malloc(int n)
{
	void *p = &heap[heap_pos];

	heap_pos += n;
	if (heap_pos > (int)sizeof(heap))
		return 0;
	return p;
}

static void trace_append_int(int val)
{
	char tmp[12];
	int len = 0;

	if (val < 0) {
		trace_buf[trace_pos++] = '-';
		val = -val;
	}
	if (val == 0)
		tmp[len++] = '0';
	while (val > 0) {
		tmp[len++] = '0' + (val % 10);
		val /= 10;
	}
	while (len > 0)
		trace_buf[trace_pos++] = tmp[--len];
}

static void trace(const char *func, int arg1, int arg2)
{
	if (trace_pos + 64 > TRACE_BUF_SIZE)
		return;

	my_strcpy(trace_buf + trace_pos, func);
	trace_pos += my_strlen(func);
	trace_buf[trace_pos++] = '(';
	trace_append_int(arg1);
	if (arg2 >= 0) {
		trace_buf[trace_pos++] = ',';
		trace_buf[trace_pos++] = ' ';
		trace_append_int(arg2);
	}
	trace_buf[trace_pos++] = ')';
	trace_buf[trace_pos++] = '\n';
}

static void trace_reset(void)
{
	trace_pos = 0;
	trace_buf[0] = '\0';
	heap_pos = 0;
}

int run(int n)
{
	int i;
	int result = 1;
	char *buf;

	trace_reset();

	trace("malloc", 32, -1);
	buf = (char *)my_malloc(32);
	if (buf == 0)
		return -1;

	trace("memset", (int)(long)buf, 0);
	my_memset(buf, 0, 32);

	for (i = 2; i <= n; i++) {
		result *= i;

		trace("strlen", (int)(long)buf, -1);
		(void)my_strlen(buf);

		{
			int v = result;
			int pos = 0;
			char tmp[12];
			int len = 0;

			if (v == 0)
				tmp[len++] = '0';
			while (v > 0) {
				tmp[len++] = '0' + (v % 10);
				v /= 10;
			}
			while (len > 0 && pos < 31)
				buf[pos++] = tmp[--len];
			buf[pos] = '\0';
		}

		trace("strcpy", (int)(long)buf, -1);
	}

	trace("free", (int)(long)buf, -1);

	return result;
}

const char *get_trace(void)
{
	return trace_buf;
}
