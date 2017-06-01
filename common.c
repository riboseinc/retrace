#include "common.h"
#include "str.h"
#include "id.h"

void
trace_printf(int hdr, char *buf, ...) {
	real_getpid = dlsym(RTLD_NEXT, "getpid");

	char str[1024];

	va_list arglist;
	va_start(arglist, buf);

	memset(str, 0, sizeof(str));

	vsnprintf(str, sizeof(str), buf, arglist);

	str[sizeof(str) -1 ] = '\0';

	if (hdr == 1)
		fprintf(stderr, "(%d) ", real_getpid());

	if(str != NULL)
		fprintf(stderr, "%s", str);
	else
		fprintf(stderr, "%s(NULL)%s\n", VAR, RST);

	va_end(arglist);
}

void
trace_printf_str(const char *string) {
	real_strlen = dlsym(RTLD_NEXT, "strlen");

	int i;
	size_t len = real_strlen(string);

	if (len > MAXLEN)
		len = MAXLEN;

	for (i = 0; i < len; i++)
		if (string[i] == '\n')
			trace_printf(0, "%s\\n%s", VAR, RST);
		else if (string[i] == '\t')
			trace_printf(0, "%s\\t%s", VAR, RST);
		else if (string[i] == '\r')
			trace_printf(0, "%s\\r%s", VAR, RST);
		else if (string[i] == '\0')
			trace_printf(0, "%s\\0%s", VAR, RST);
		else
			trace_printf(0, "%c", string[i]);

	if (len > (MAXLEN - 1))
		trace_printf(0, "%s[SNIP]%s", VAR, RST);
}
