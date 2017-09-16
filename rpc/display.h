#ifndef __RETRACE_DISPLAY_H__
#define __RETRACE_DISPLAY_H__

#define DISPLAY_char(ep, c)	if (c) printf("'%c'", c); else printf("'\\0'");
#define DISPLAY_cmsghdr(ep, p)	DISPLAY_pvoid(ep, p)
#define DISPLAY_cstring(ep, p)	display_string(ep, p)
#define DISPLAY_dir(ep, p)	DISPLAY_pvoid(ep, p)
#define DISPLAY_dirent(ep, p)	DISPLAY_pvoid(ep, p)
#define DISPLAY_file(ep, p)	DISPLAY_pvoid(ep, p)
#define DISPLAY_fd(ep, i)	DISPLAY_int(ep, i)
#define DISPLAY_int(ep, i)	printf("%d", (i))
#define DISPLAY_long(ep, i)	printf("%ld", (i))
#define DISPLAY_msghdr(ep, p)	DISPLAY_pvoid(ep, p)
#define DISPLAY_pid_t(ep, i)	DISPLAY_int(ep, i)
#define DISPLAY_pcvoid(ep, p)	DISPLAY_pvoid(ep, p)
#define DISPLAY_pdirent(ep, p)	DISPLAY_pvoid(ep, p)
#define DISPLAY_pint(ep, p)	DISPLAY_pvoid(ep, p)
#define DISPLAY_psize_t(ep, p)	DISPLAY_pvoid(ep, p)
#define DISPLAY_pstring(ep, p)	DISPLAY_pvoid(ep, p)
#define DISPLAY_pvoid(ep, p)	printf("%p", (p))
#define DISPLAY_size_t(ep, i)	DISPLAY_ulong(ep, i)
#define DISPLAY_ssize_t(ep, i)	DISPLAY_long(ep, i)
#define DISPLAY_string(ep, p)	display_string(ep, p)
#define DISPLAY_ulong(ep, i)	printf("%lu", (i))
#define DISPLAY_va_list(ep, ap)	printf("ap")

struct display_info {
#if BACKTRACE
	int backtrace_functions[RPC_FUNCTION_COUNT];
	int backtrace_depth;
#endif
	int expand_strings;
};

void *display_buffer(void *buffer, size_t length);
void display_string(struct retrace_endpoint *ep, const char *s);
#endif
