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

#include "../config.h"

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "frontend.h"
#include "display.h"
#include "tracefd.h"
#include "handlers.h"

struct flag_name {
	int flag;
	const char *name;
};

static char *
format_char(char *buf, int c)
{
	static const char hex[] = "0123456789ABCDEF";

	if (isprint(c) && c != '\\')
		*(buf++) = c;
	else {
		*(buf++) = '\\';
		switch (c) {
		case '\\':
			*(buf++) = '\\';
			break;
		case '\0':
			*(buf++) = '0';
			break;
		case '\a':
			*(buf++) = 'a';
			break;
		case '\b':
			*(buf++) = 'b';
			break;
		case '\t':
			*(buf++) = 't';
			break;
		case '\n':
			*(buf++) = 'n';
			break;
		case '\v':
			*(buf++) = 'v';
			break;
		case '\f':
			*(buf++) = 'f';
			break;
		case '\r':
			*(buf++) = 'r';
			break;
		default:
			*(buf++) = 'x';
			*(buf++) = hex[c >> 4 & 0xf];
			*(buf++) = hex[c & 0xf];
			break;
		}
	}
	return buf;
}

void
print_string(const char *s)
{
	char buf[256], *p = buf, *e = buf + 250;

	while (*s) {
		if (p >= e) {
			*p = '\0';
			printf("%s", buf);
			p = buf;
		}
		p = format_char(p, *s);
		s++;
	}
	if (p != buf) {
		*p = '\0';
		printf("%s", buf);
	}
}

void
display_string(struct retrace_endpoint *ep, const char *s)
{
	struct handler_info *hi = ep->handle->user_data;
	char buf[hi->expand_strings + 1];
	int snipped;

	if (s && hi->expand_strings) {
		buf[hi->expand_strings] = '\0';
		retrace_fetch_string(ep->fd, s, buf, sizeof(buf));
		snipped = buf[hi->expand_strings] != '\0';
		buf[hi->expand_strings] = '\0';
		printf("\"");
		print_string(buf);
		printf(snipped ? "\"+" : "\"");
	} else {
		printf("%p", s);
	}
}

void
display_char(int c)
{
	char buf[16], *e;

	e = format_char(buf, c);
	*e = '\0';
	printf("'%s'", buf);
}

void
display_fd(struct retrace_endpoint *ep, int fd)
{
	struct handler_info *hi = ep->handle->user_data;
	const struct fdinfo *fi = NULL;

	if (hi->tracefds)
		fi = get_fdinfo(&hi->fdinfos, ep->pid, fd);

	if (fi != NULL)
		printf("%d:%s", fd, fi->info);
	else
		printf("%d", fd);
}

void
display_stream(struct retrace_endpoint *ep, FILE *stream)
{
	struct handler_info *hi = ep->handle->user_data;
	const struct fdinfo *fi = NULL;

	if (hi->tracefds)
		fi = get_streaminfo(&hi->fdinfos, ep->pid, stream);

	if (fi != NULL)
		printf("%p:%s", stream, fi->info);
	else
		printf("%p", stream);
}

void
display_dir(struct retrace_endpoint *ep, DIR *dir)
{
	struct handler_info *hi = ep->handle->user_data;
	const struct fdinfo *fi = NULL;

	if (hi->tracefds)
		fi = get_dirinfo(&hi->fdinfos, ep->pid, dir);

	if (fi != NULL)
		printf("%p:%s", dir, fi->info);
	else
		printf("%p", dir);
}

void
display_fflags(struct retrace_endpoint *ep, int flags)
{
	static struct flag_name flag_names[] = {
		{O_APPEND,	"O_APPEND"},
		{O_ASYNC,	"O_ASYNC"},
		{O_CLOEXEC,	"O_CLOEXEC"},
		{O_CREAT,	"O_CREAT"},
		{O_DIRECTORY,	"O_DIRECTORY"},
		{O_EXCL,	"O_EXCL"},
		{O_NOCTTY,	"O_NOCTTY"},
		{O_NOFOLLOW,	"O_NOFOLLOW"},
		{O_NONBLOCK,	"O_NONBLOCK"},
		{O_NDELAY,	"O_NDELAY"},
		{O_SYNC,	"O_SYNC"},
		{O_TRUNC,	"O_TRUNC"},
#if defined __APPLE__ || defined __OpenBSD__ || defined __linux__
		{O_DSYNC,	"O_DSYNC"},
#endif
#ifdef __linux__
		{O_LARGEFILE,	"O_LARGEFILE"},
		{O_NOATIME,	"O_NOATIME"},
		{O_PATH,	"O_PATH"},
		{O_TMPFILE,	"O_TMPFILE"},
#endif
#if defined __linux__ || defined __FreeBSD__
		{O_DIRECT,	"O_DIRECT"},
#endif
#ifdef __APPLE__
		{O_EVTONLY,	"O_EVTONLY"},
		{O_SYMLINK,	"O_SYMLINK"},
#endif
#ifdef __FreeBSD__
		{O_EXEC,	"O_EXEC"},
		{O_FSYNC,	"O_FSYNC"},
		{O_TTY_INIT,	"O_TTY_INIT"},
		{O_VERIFY,	"O_VERIFY"},
#endif
#if defined __APPLE__ || defined __OpenBSD__ || defined __FreeBSD__
		{O_EXLOCK,	"O_EXLOCK"},
		{O_SHLOCK,	"O_SHLOCK"},
#endif
		{0,	NULL} };
	struct flag_name *f;

	switch (flags & O_ACCMODE) {
	case O_RDONLY:
		printf("O_RDONLY");
		break;
	case O_WRONLY:
		printf("O_WRONLY");
		break;
	case O_RDWR:
		printf("O_RDWR");
		break;
	}

	flags &= ~O_ACCMODE;

	for (f = flag_names; f->name != NULL; ++f) {
		if ((flags & f->flag) != 0)
			printf(" | %s", f->name);
		flags &= ~f->flag;
	}

	if (flags != 0)
		printf(" | UNKNOWN(%x)", flags);
}

void
display_msgflags(struct retrace_endpoint *ep, int flags)
{
	static struct flag_name flag_names[] = {
		{MSG_DONTROUTE,	"MSG_DONTROUTE"},
		{MSG_OOB,	"MSG_OOB"},
		{MSG_PEEK,	"MSG_PEEK"},
		{MSG_WAITALL,	"MSG_WAITALL"},
#ifdef __linux__
		{MSG_CMSG_CLOEXEC,	"MSG_CMSG_CLOEXEC"},
		{MSG_CONFIRM,	"MSG_CONFIRM"},
		{MSG_ERRQUEUE,	"MSG_ERRQUEUE"},
		{MSG_MORE,	"MSG_MORE"},
		{MSG_TRUNC,	"MSG_TRUNC"},
#endif
#if defined __linux__ || defined __OpenBSD__
		{MSG_DONTWAIT,	"MSG_DONTWAIT"},
		{MSG_NOSIGNAL,	"MSG_NOSIGNAL"},
		{MSG_EOR,	"MSG_EOR"},
#endif
		{0,	NULL} };
	struct flag_name *f;
	const char *fmt = "%s";

	if (flags == 0)
		printf("0");
	else {
		for (f = flag_names; f->name != NULL; ++f) {
			if ((flags & f->flag) != 0) {
				printf(fmt, f->name);
				fmt = " | %s";
			}
		}
	}

	if (flags != 0) {
		printf(fmt, "UNKNOWN");
		printf("(%x)", flags);
	}
}

void
display_errno(int _errno)
{
	static struct flag_name flag_names[] = {
		{E2BIG,	"E2BIG"},
		{EACCES,	"EACCES"},
		{EADDRINUSE,	"EADDRINUSE"},
		{EADDRNOTAVAIL,	"EADDRNOTAVAIL"},
		{EAFNOSUPPORT,	"EAFNOSUPPORT"},
		{EAGAIN,	"EAGAIN"},
		{EALREADY,	"EALREADY"},
		{EBADF,	"EBADF"},
		{EBUSY,	"EBUSY"},
		{ECANCELED,	"ECANCELED"},
		{ECHILD,	"ECHILD"},
		{ECONNABORTED,	"ECONNABORTED"},
		{ECONNREFUSED,	"ECONNREFUSED"},
		{ECONNRESET,	"ECONNRESET"},
		{EDEADLK,	"EDEADLK"},
		{EDESTADDRREQ,	"EDESTADDRREQ"},
		{EDOM,	"EDOM"},
		{EDQUOT,	"EDQUOT"},
		{EEXIST,	"EEXIST"},
		{EFAULT,	"EFAULT"},
		{EFBIG,	"EFBIG"},
		{EHOSTDOWN,	"EHOSTDOWN"},
		{EHOSTUNREACH,	"EHOSTUNREACH"},
		{EIDRM,	"EIDRM"},
		{EILSEQ,	"EILSEQ"},
		{EINPROGRESS,	"EINPROGRESS"},
		{EINTR,	"EINTR"},
		{EINVAL,	"EINVAL"},
		{EIO,	"EIO"},
		{EISCONN,	"EISCONN"},
		{EISDIR,	"EISDIR"},
		{ELOOP,	"ELOOP"},
		{EMFILE,	"EMFILE"},
		{EMLINK,	"EMLINK"},
		{EMSGSIZE,	"EMSGSIZE"},
		{ENAMETOOLONG,	"ENAMETOOLONG"},
		{ENETDOWN,	"ENETDOWN"},
		{ENETRESET,	"ENETRESET"},
		{ENETUNREACH,	"ENETUNREACH"},
		{ENFILE,	"ENFILE"},
		{ENOBUFS,	"ENOBUFS"},
		{ENODEV,	"ENODEV"},
		{ENOENT,	"ENOENT"},
		{ENOEXEC,	"ENOEXEC"},
		{ENOLCK,	"ENOLCK"},
		{ENOMEM,	"ENOMEM"},
		{ENOMSG,	"ENOMSG"},
		{ENOPROTOOPT,	"ENOPROTOOPT"},
		{ENOSPC,	"ENOSPC"},
		{ENOTBLK,	"ENOTBLK"},
		{ENOTCONN,	"ENOTCONN"},
		{ENOTDIR,	"ENOTDIR"},
		{ENOTEMPTY,	"ENOTEMPTY"},
		{ENOTSOCK,	"ENOTSOCK"},
		{ENOTSUP,	"ENOTSUP"},
		{ENOTTY,	"ENOTTY"},
		{ENXIO,	"ENXIO"},
		{EOPNOTSUPP,	"EOPNOTSUPP"},
		{EOVERFLOW,	"EOVERFLOW"},
		{EPERM,	"EPERM"},
		{EPFNOSUPPORT,	"EPFNOSUPPORT"},
		{EPIPE,	"EPIPE"},
		{EPROTONOSUPPORT,	"EPROTONOSUPPORT"},
		{EPROTOTYPE,	"EPROTOTYPE"},
		{ERANGE,	"ERANGE"},
		{EREMOTE,	"EREMOTE"},
		{EROFS,	"EROFS"},
		{ESHUTDOWN,	"ESHUTDOWN"},
		{ESOCKTNOSUPPORT,	"ESOCKTNOSUPPORT"},
		{ESPIPE,	"ESPIPE"},
		{ESRCH,	"ESRCH"},
		{ESTALE,	"ESTALE"},
		{ETIMEDOUT,	"ETIMEDOUT"},
		{ETXTBSY,	"ETXTBSY"},
		{EUSERS,	"EUSERS"},
		{EWOULDBLOCK,	"EWOULDBLOCK"},
		{EXDEV,	"EXDEV"},
#if defined __APPLE__ || defined __OpenBSD__
		{EAUTH,	"EAUTH"},
		{EBADRPC,	"EBADRPC"},
		{EFTYPE,	"EFTYPE"},
		{ENEEDAUTH,	"ENEEDAUTH"},
		{ENOATTR,	"ENOATTR"},
		{EPROCLIM,	"EPROCLIM"},
		{EPROCUNAVAIL,	"EPROCUNAVAIL"},
		{EPROGMISMATCH,	"EPROGMISMATCH"},
		{EPROGUNAVAIL,	"EPROGUNAVAIL"},
		{ERPCMISMATCH,	"ERPCMISMATCH"},
#endif
#if defined __APPLE__ || defined __linux__
		{EBADMSG,	"EBADMSG"},
		{EMULTIHOP,	"EMULTIHOP"},
		{ENODATA,	"ENODATA"},
		{ENOLINK,	"ENOLINK"},
		{ENOSR,	"ENOSR"},
		{ENOSTR,	"ENOSTR"},
		{EPROTO,	"EPROTO"},
		{ETIME,	"ETIME"},
#endif
#if defined __OpenBSD__ || defined __linux__
		{EMEDIUMTYPE,	"EMEDIUMTYPE"},
		{ENOMEDIUM,	"ENOMEDIUM"},
#endif
#ifdef __OpenBSD__
		{EIPSEC,	"EIPSEC"},
#endif
#ifdef __linux__
		{EBADE,	"EBADE"},
		{EBADFD,	"EBADFD"},
		{EBADR,	"EBADR"},
		{EBADRQC,	"EBADRQC"},
		{EBADSLT,	"EBADSLT"},
		{ECHRNG,	"ECHRNG"},
		{ECOMM,	"ECOMM"},
		{EDEADLOCK,	"EDEADLOCK"},
		{EISNAM,	"EISNAM"},
		{EKEYEXPIRED,	"EKEYEXPIRED"},
		{EKEYREJECTED,	"EKEYREJECTED"},
		{EKEYREVOKED,	"EKEYREVOKED"},
		{EL2HLT,	"EL2HLT"},
		{EL2NSYNC,	"EL2NSYNC"},
		{EL3HLT,	"EL3HLT"},
		{EL3RST,	"EL3RST"},
		{ELIBACC,	"ELIBACC"},
		{ELIBBAD,	"ELIBBAD"},
		{ELIBMAX,	"ELIBMAX"},
		{ELIBSCN,	"ELIBSCN"},
		{ELIBEXEC,	"ELIBEXEC"},
		{ENOKEY,	"ENOKEY"},
		{ENONET,	"ENONET"},
		{ENOPKG,	"ENOPKG"},
		{ENOTUNIQ,	"ENOTUNIQ"},
		{EREMCHG,	"EREMCHG"},
		{EREMOTEIO,	"EREMOTEIO"},
		{ERESTART,	"ERESTART"},
		{ESTRPIPE,	"ESTRPIPE"},
		{EUCLEAN,	"EUCLEAN"},
		{EUNATCH,	"EUNATCH"},
		{EXFULL,	"EXFULL"},
#endif
#ifdef __APPLE__
		{EBADARCH,	"EBADARCH"},
		{EBADEXEC,	"EBADEXEC"},
		{EBADMACHO,	"EBADMACHO"},
		{EDEVERR,	"EDEVERR"},
		{ENOPOLICY,	"ENOPOLICY"},
		{ENOTRECOVERABLE,	"ENOTRECOVERABLE"},
		{EOWNERDEAD,	"EOWNERDEAD"},
		{EPWROFF,	"EPWROFF"},
		{EQFULL,	"EQFULL"},
		{ESHLIBVERS,	"ESHLIBVERS"},
		{ETOOMANYREFS,	"ETOOMANYREFS"},
#endif
		{0,	NULL} };
	struct flag_name *f;

	for (f = flag_names; f->name; ++f) {
		if (f->flag == _errno) {
			printf(" [%s:%d])", f->name, _errno);
			return;
		}
	}
	printf(" [errno:%d]", _errno);
}

void
display_domain(int domain)
{
	static struct flag_name flag_names[] = {
#ifdef __linux__
		{AF_NETLINK,	"AF_NETLINK"},
		{AF_X25,	"AF_X25"},
		{AF_AX25,	"AF_AX25"},
		{AF_ATMPVC,	"AF_ATMPVC"},
		{AF_PACKET,	"AF_PACKET"},
		{AF_ALG,	"AF_ALG"},
#endif
#if defined __linux__ || defined __OpenBSD__
		{AF_UNIX,	"AF_UNIX"},
		{AF_LOCAL,	"AF_LOCAL"},
		{AF_INET,	"AF_INET"},
		{AF_INET6,	"AF_INET6"},
		{AF_IPX,	"AF_IPX"},
		{AF_APPLETALK,	"AF_APPLETALK"},
#endif
#ifdef __APPLE__
		{PF_LOCAL,	"PF_LOCAL"},
		{PF_UNIX,	"PF_UNIX"},
		{PF_INET,	"PF_INET"},
		{PF_ROUTE,	"PF_ROUTE"},
		{PF_KEY,	"PF_KEY"},
		{PF_INET6,	"PF_INET6"},
		{PF_SYSTEM,	"PF_SYSTEM"},
		{PF_NDRV,	"PF_NDRV"},
#endif
		{0,	NULL} };
	struct flag_name *f;

	for (f = flag_names; f->name; ++f) {
		if (f->flag == domain) {
			printf("%s", f->name);
			return;
		}
	}
	printf("unknown:%d", domain);
}

void
display_protocol(int protocol)
{
	/* from /etc/protocols */
	/* TODO: read from file at startup */
	static struct flag_name flag_names[] = {
		{0,	"IP"},
		{0,	"HOPOPT"},
		{1,	"ICMP"},
		{2,	"IGMP"},
		{3,	"GGP"},
		{4,	"IP-ENCAP"},
		{5,	"ST"},
		{6,	"TCP"},
		{8,	"EGP"},
		{9,	"IGP"},
		{12,	"PUP"},
		{17,	"UDP"},
		{20,	"HMP"},
		{22,	"XNS-IDP"},
		{27,	"RDP"},
		{29,	"ISO-TP4"},
		{33,	"DCCP"},
		{36,	"XTP"},
		{37,	"DDP"},
		{38,	"IDPR-CMTP"},
		{41,	"IPv6"},
		{43,	"IPv6-Route"},
		{44,	"IPv6-Frag"},
		{45,	"IDRP"},
		{46,	"RSVP"},
		{47,	"GRE"},
		{0,	NULL} };
	struct flag_name *f;

	for (f = flag_names; f->name; ++f) {
		if (f->flag == protocol) {
			printf("%s", f->name);
			return;
		}
	}
	printf("unknown:%d", protocol);
}

void
display_socktype(int socktype)
{
	static struct flag_name flag_names[] = {
		{SOCK_STREAM,	"SOCK_STREAM"},
		{SOCK_DGRAM,	"SOCK_DGRAM"},
		{SOCK_SEQPACKET,	"SOCK_SEQPACKET"},
		{SOCK_RAW,	"SOCK_RAW"},
		{SOCK_RDM,	"SOCK_RDM"},
#ifdef __linux__
		{SOCK_PACKET,	"SOCK_PACKET"},
#endif
#if defined __linix__ || defined __OpenBSD__
		{SOCK_NONBLOCK,	"SOCK_NONBLOCK"},
		{SOCK_CLOEXEC,	"SOCK_CLOEXEC"},
#endif
		{0,	NULL} };
	struct flag_name *f;

	for (f = flag_names; f->name; ++f) {
		if (f->flag == socktype) {
			printf("%s", f->name);
			return;
		}
	}
	printf("unknown:%d", socktype);
}

void
display_sockaddr(struct retrace_endpoint *ep, const struct sockaddr *addr, socklen_t len)
{
	struct handler_info *hi = ep->handle->user_data;
	struct sockaddr_storage raddr;
	int x;
	char buf[256];

	assert(sizeof(struct sockaddr_un) <= sizeof(raddr));

	if (!hi->expand_structs || addr == NULL) {
		DISPLAY_pvoid(ep, addr);
		return;
	}

	if (len > sizeof(raddr))
		len = sizeof(raddr);

	x = retrace_fetch_memory(ep->fd, addr, &raddr, len);
	if (x == 0) {
		DISPLAY_pvoid(ep, addr);
		return;
	}

	if (raddr.ss_family == AF_UNIX) {
		struct sockaddr_un *sa = (struct sockaddr_un *)&raddr;

		printf("{AF_UNIX, %*s}", (int)sizeof(sa->sun_path),
		    sa->sun_path);
	} else if (raddr.ss_family == AF_INET) {
		struct sockaddr_in *sa = (struct sockaddr_in *)&raddr;

		printf("{AF_INET, %d, %s}", ntohs(sa->sin_port),
		    inet_ntop(AF_INET, &sa->sin_addr, buf, sizeof(buf)));
	} else if (raddr.ss_family == AF_INET6) {
		struct sockaddr_in6 *sa = (struct sockaddr_in6 *)&raddr;

		printf("{AF_INET6, %d, %s}", ntohs(sa->sin6_port),
		    inet_ntop(AF_INET6, &sa->sin6_addr, buf, sizeof(buf)));
	}
}

void
display_msg(struct retrace_endpoint *ep, const struct msghdr *msg, size_t msglen)
{
	struct handler_info *hi = ep->handle->user_data;
	struct msghdr rmsg;
	int x;

	if (!hi->expand_structs || msg == NULL) {
		DISPLAY_pvoid(ep, msg);
		return;
	}

	x = retrace_fetch_memory(ep->fd, msg, &rmsg, sizeof(rmsg));
	if (x == 0) {
		DISPLAY_pvoid(ep, msg);
		return;
	}

	printf("{");
	if (rmsg.msg_name != NULL)
		display_sockaddr(ep, rmsg.msg_name, rmsg.msg_namelen);
	else
		printf("NULL");
	printf(", %d", rmsg.msg_namelen);

	printf(", ");
	display_iovec(ep, rmsg.msg_iov, rmsg.msg_iovlen, msglen);
#ifdef __linux__
	printf(", %lu}", rmsg.msg_iovlen);
#else
	printf(", %d}", rmsg.msg_iovlen);
#endif
}

void
display_iovec(struct retrace_endpoint *ep, const struct iovec *iov, size_t iovlen, size_t msglen)
{
	struct handler_info *hi = ep->handle->user_data;
	struct iovec riov[iovlen];
	int i, x;
	size_t len;

	if (!hi->expand_structs || iov == NULL) {
		DISPLAY_pvoid(ep, iov);
		return;
	}

	x = retrace_fetch_memory(ep->fd, iov, riov, sizeof(riov));
	if (x == 0) {
		DISPLAY_pvoid(ep, iov);
		return;
	}

	printf("{");
	for (i = 0; i < iovlen; i++) {
		printf("{");
		len = riov[i].iov_len;
		if (len > msglen)
			len = msglen;
		msglen -= len;
		display_buffer(ep, riov[i].iov_base, len);
		printf(", %lu}", riov[i].iov_len);
	}
}

void
display_buffer(struct retrace_endpoint *ep, const void *addr, size_t len)
{
	static const char hex[] = "0123456789abcdef";
	struct handler_info *hi = ep->handle->user_data;
	char buf[hi->expand_buffers];
	int x, i, truncated = 0;
	const char *mask;

	if (!hi->expand_buffers || addr == NULL) {
		DISPLAY_pvoid(ep, addr);
		return;
	}

	if (len > hi->expand_buffers) {
		len = hi->expand_buffers;
		truncated = 1;
	}

	x = retrace_fetch_memory(ep->fd, addr, buf, len);
	if (x == 0) {
		DISPLAY_pvoid(ep, addr);
		return;
	}

	mask = "[%c%c";
	for (i = 0; i < len; i++) {
		printf(mask, hex[buf[i] >> 4 & 0x0f], hex[buf[i] & 0x0f]);
		mask = " %c%c";
	}
	printf(truncated ? "]+" : "]");
}

struct log_info {
	STAILQ_ENTRY(log_info) next;
	const char *info;
};

STAILQ_HEAD(log_infos, log_info);

static void
free_info(void *user_data)
{
	struct log_infos *infos = user_data;
	struct log_info *pinfo;

	while (!STAILQ_EMPTY(infos)) {
		pinfo = STAILQ_FIRST(infos);
		STAILQ_REMOVE_HEAD(infos, next);
		free(pinfo);
	}
	free(infos);
}

void
add_info(struct retrace_call_context *ctx, const char *fmt, ...)
{
	char buf[1024];
	va_list ap;
	struct log_infos *infos;
	struct log_info *pinfo;
	size_t n;

	infos = ctx->user_data;
	if (infos == NULL) {
		infos = malloc(sizeof(struct log_infos));
		if (infos == NULL)
			return;
		STAILQ_INIT(infos);
		ctx->user_data = infos;
		ctx->free_user_data = free_info;
	}

	va_start(ap, fmt);

	n = vsnprintf(buf, sizeof(buf), fmt, ap);
	if (n >= sizeof(buf))
		n = sizeof(buf) - 1;

	pinfo = malloc(sizeof(struct log_info) + n + 1);
	if (pinfo != NULL) {
		pinfo->info = (char *)(pinfo + 1);
		strcpy((char *)pinfo->info, buf);
	}
	STAILQ_INSERT_TAIL(infos, pinfo, next);
}

void
display_info(struct retrace_call_context *ctx, unsigned int depth)
{
	struct log_info *pinfo;

	if (ctx->user_data == NULL)
		return;
	STAILQ_FOREACH(pinfo, (struct log_infos *)ctx->user_data, next)
		printf("\t%.*s%s\n", depth, "\t\t\t\t\t", pinfo->info);
}
