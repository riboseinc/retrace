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

#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__)
#include <sys/ucred.h>
#endif

#include "peer_gate.h"

#if defined(__linux__)

int retraced_peer_uid(int fd, long *uid_out)
{
	struct ucred cr;
	socklen_t len = sizeof(cr);

	memset(&cr, 0, sizeof(cr));
	if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cr, &len) != 0)
		return -1;
	*uid_out = (long)cr.uid;
	return 0;
}

#elif defined(SO_PEERCRED)

int retraced_peer_uid(int fd, long *uid_out)
{
	uid_t uid = 0;
	socklen_t len = sizeof(uid);

	/* the BSDs that grew SO_PEERCRED (NetBSD) */
	if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &uid, &len) != 0)
		return -1;
	*uid_out = (long)uid;
	return 0;
}

#elif defined(LOCAL_PEERCRED)

int retraced_peer_uid(int fd, long *uid_out)
{
	struct xucred cr;
	socklen_t len = sizeof(cr);

	memset(&cr, 0, sizeof(cr));
#ifdef SOL_LOCAL
	if (getsockopt(fd, SOL_LOCAL, LOCAL_PEERCRED, &cr, &len) != 0)
		return -1;
#else
	/* FreeBSD has no SOL_LOCAL; level 0 is the local domain */
	if (getsockopt(fd, 0, LOCAL_PEERCRED, &cr, &len) != 0)
		return -1;
#endif
	if (len < sizeof(cr) || cr.cr_version != XUCRED_VERSION)
		return -1;
	*uid_out = (long)cr.cr_uid;
	return 0;
}

#else

/*
 * OpenBSD: no credential query on local sockets. Fail OPEN for
 * the agent plane (liveness doctrine) -- the 0660 socket mode
 * still gates who can even reach connect(2); the daemon says so
 * in the journal at startup.
 */
int retraced_peer_uid(int fd, long *uid_out)
{
	(void)fd;
	(void)uid_out;
	return -1;
}

#endif

int retraced_peer_query_supported(void)
{
#if defined(__linux__) || defined(SO_PEERCRED) || \
	defined(LOCAL_PEERCRED)
	return 1;
#else
	return 0;
#endif
}

int retraced_peer_allowed(long peer_uid, long my_euid)
{
	return peer_uid == 0 || peer_uid == my_euid;
}

int retraced_nonce_matches(const char *presented, const char *expected)
{
	size_t i, n = strlen(expected);

	volatile unsigned char diff = 0;

	if (presented == NULL || strlen(presented) != n)
		return 0;
	for (i = 0; i < n; i++)
		diff |= (unsigned char)presented[i] ^
			(unsigned char)expected[i];
	return diff == 0;
}
