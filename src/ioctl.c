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


#include "common.h"
#include "ioctl.h"

#include <sys/ioctl.h>

#ifdef __linux__
#include <asm-generic/ioctls.h>
#endif


int ioctl_v(int fd, unsigned long request, va_list ap)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_FILE_DESCRIPTOR, PARAMETER_TYPE_INT | PARAMETER_FLAG_STRING_NEXT, PARAMETER_TYPE_POINTER, PARAMETER_TYPE_END};
	char *arg;
	char *request_str;
	void *parameter_values[] = {&fd, &request, &request_str, &arg};
	int ret;

	arg = va_arg(ap, char *);

	switch (request) {
#ifdef TCGETS
	case TCGETS:
		request_str = "TCGETS";
		break;
#endif
#ifdef TCSETS
	case TCSETS:
		request_str = "TCSETS";
		break;
#endif
#ifdef TCSETSW
	case TCSETSW:
		request_str = "TCSETSW";
		break;
#endif
#ifdef TCSETSF
	case TCSETSF:
		request_str = "TCSETSF";
		break;
#endif
#ifdef TCGETA
	case TCGETA:
		request_str = "TCGETA";
		break;
#endif
#ifdef TCSETA
	case TCSETA:
		request_str = "TCSETA";
		break;
#endif
#ifdef TCSETAW
	case TCSETAW:
		request_str = "TCSETAW";
		break;
#endif
#ifdef TCSETAF
	case TCSETAF:
		request_str = "TCSETAF";
		break;
#endif
#ifdef TCSBRK
	case TCSBRK:
		request_str = "TCSBRK";
		break;
#endif
#ifdef TCXONC
	case TCXONC:
		request_str = "TCXONC";
		break;
#endif
#ifdef TCFLSH
	case TCFLSH:
		request_str = "TCFLSH";
		break;
#endif
#ifdef TIOCEXCL
	case TIOCEXCL:
		request_str = "TIOCEXCL";
		break;
#endif
#ifdef TIOCNXCL
	case TIOCNXCL:
		request_str = "TIOCNXCL";
		break;
#endif
#ifdef TIOCSCTTY
	case TIOCSCTTY:
		request_str = "TIOCSCTTY";
		break;
#endif
#ifdef TIOCGPGRP
	case TIOCGPGRP:
		request_str = "TIOCGPGRP";
		break;
#endif
#ifdef TIOCSPGRP
	case TIOCSPGRP:
		request_str = "TIOCSPGRP";
		break;
#endif
#ifdef TIOCOUTQ
	case TIOCOUTQ:
		request_str = "TIOCOUTQ";
		break;
#endif
#ifdef TIOCSTI
	case TIOCSTI:
		request_str = "TIOCSTI";
		break;
#endif
#ifdef TIOCGWINSZ
	case TIOCGWINSZ:
		request_str = "TIOCGWINSZ";
		break;
#endif
#ifdef TIOCSWINSZ
	case TIOCSWINSZ:
		request_str = "TIOCSWINSZ";
		break;
#endif
#ifdef TIOCMGET
	case TIOCMGET:
		request_str = "TIOCMGET";
		break;
#endif
#ifdef TIOCMBIS
	case TIOCMBIS:
		request_str = "TIOCMBIS";
		break;
#endif
#ifdef TIOCMBIC
	case TIOCMBIC:
		request_str = "TIOCMBIC";
		break;
#endif
#ifdef TIOCMSET
	case TIOCMSET:
		request_str = "TIOCMSET";
		break;
#endif
#ifdef TIOCGSOFTCAR
	case TIOCGSOFTCAR:
		request_str = "TIOCGSOFTCAR";
		break;
#endif
#ifdef TIOCSSOFTCAR
	case TIOCSSOFTCAR:
		request_str = "TIOCSSOFTCAR";
		break;
#endif
#ifdef TIOCINQ
	case TIOCINQ:
		request_str = "TIOCINQ";
		break;
#endif
#ifdef TIOCLINUX
	case TIOCLINUX:
		request_str = "TIOCLINUX";
		break;
#endif
#ifdef TIOCCONS
	case TIOCCONS:
		request_str = "TIOCCONS";
		break;
#endif
#ifdef TIOCGSERIAL
	case TIOCGSERIAL:
		request_str = "TIOCGSERIAL";
		break;
#endif
#ifdef TIOCSSERIAL
	case TIOCSSERIAL:
		request_str = "TIOCSSERIAL";
		break;
#endif
#ifdef TIOCPKT
	case TIOCPKT:
		request_str = "TIOCPKT";
		break;
#endif
#ifdef FIONBIO
	case FIONBIO:
		request_str = "FIONBIO";
		break;
#endif
#ifdef TIOCNOTTY
	case TIOCNOTTY:
		request_str = "TIOCNOTTY";
		break;
#endif
#ifdef TIOCSETD
	case TIOCSETD:
		request_str = "TIOCSETD";
		break;
#endif
#ifdef TIOCGETD
	case TIOCGETD:
		request_str = "TIOCGETD";
		break;
#endif
#ifdef TCSBRKP
	case TCSBRKP:
		request_str = "TCSBRKP";
		break;
#endif
#ifdef TIOCSBRK
	case TIOCSBRK:
		request_str = "TIOCSBRK";
		break;
#endif
#ifdef TIOCCBRK
	case TIOCCBRK:
		request_str = "TIOCCBRK";
		break;
#endif
#ifdef TIOCGSID
	case TIOCGSID:
		request_str = "TIOCGSID";
		break;
#endif
#ifdef TIOCGRS485
	case TIOCGRS485:
		request_str = "TIOCGRS485";
		break;
#endif
#ifdef TIOCSRS485
	case TIOCSRS485:
		request_str = "TIOCSRS485";
		break;
#endif
#ifdef TCGETX
	case TCGETX:
		request_str = "TCGETX";
		break;
#endif
#ifdef TCSETX
	case TCSETX:
		request_str = "TCSETX";
		break;
#endif
#ifdef TCSETXF
	case TCSETXF:
		request_str = "TCSETXF";
		break;
#endif
#ifdef TCSETXW
	case TCSETXW:
		request_str = "TCSETXW";
		break;
#endif
#ifdef TIOCSIG
	case TIOCSIG:
		request_str = "TIOCSIG";
		break;
#endif
#ifdef TIOCVHANGUP
	case TIOCVHANGUP:
		request_str = "TIOCVHANGUP";
		break;
#endif
#ifdef TIOCGPKT
	case TIOCGPKT:
		request_str = "TIOCGPKT";
		break;
#endif
#ifdef TIOCGPTLCK
	case TIOCGPTLCK:
		request_str = "TIOCGPTLCK";
		break;
#endif
#ifdef TIOCGEXCL
	case TIOCGEXCL:
		request_str = "TIOCGEXCL";
		break;
#endif
#ifdef FIONCLEX
	case FIONCLEX:
		request_str = "FIONCLEX";
		break;
#endif
#ifdef FIOCLEX
	case FIOCLEX:
		request_str = "FIOCLEX";
		break;
#endif
#ifdef FIOASYNC
	case FIOASYNC:
		request_str = "FIOASYNC";
		break;
#endif
#ifdef TIOCSERCONFIG
	case TIOCSERCONFIG:
		request_str = "TIOCSERCONFIG";
		break;
#endif
#ifdef TIOCSERGWILD
	case TIOCSERGWILD:
		request_str = "TIOCSERGWILD";
		break;
#endif
#ifdef TIOCSERSWILD
	case TIOCSERSWILD:
		request_str = "TIOCSERSWILD";
		break;
#endif
#ifdef TIOCGLCKTRMIOS
	case TIOCGLCKTRMIOS:
		request_str = "TIOCGLCKTRMIOS";
		break;
#endif
#ifdef TIOCSLCKTRMIOS
	case TIOCSLCKTRMIOS:
		request_str = "TIOCSLCKTRMIOS";
		break;
#endif
#ifdef TIOCSERGSTRUCT
	case TIOCSERGSTRUCT:
		request_str = "TIOCSERGSTRUCT";
		break;
#endif
#ifdef TIOCSERGETLSR
	case TIOCSERGETLSR:
		request_str = "TIOCSERGETLSR";
		break;
#endif
#ifdef TIOCSERGETMULTI
	case TIOCSERGETMULTI:
		request_str = "TIOCSERGETMULTI";
		break;
#endif
#ifdef TIOCSERSETMULTI
	case TIOCSERSETMULTI:
		request_str = "TIOCSERSETMULTI";
		break;
#endif
#ifdef TIOCMIWAIT
	case TIOCMIWAIT:
		request_str = "TIOCMIWAIT";
		break;
#endif
#ifdef TIOCGICOUNT
	case TIOCGICOUNT:
		request_str = "TIOCGICOUNT";
		break;
#endif
#ifdef FIOQSIZE
	case FIOQSIZE:
		request_str = "FIOQSIZE";
		break;
#endif
	default:
		request_str = "UNKNOWN";
		break;
	}


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "ioctl";
	event_info.function_group = RTR_FUNC_GRP_PROC;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &ret;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);


	ret = real_ioctl(fd, request, arg);

	retrace_log_and_redirect_after(&event_info);

	return ret;
}

int RETRACE_IMPLEMENTATION(ioctl)(int fd, unsigned long request, ...)
{
	int r;
	va_list ap;

	va_start(ap, request);
	r = ioctl_v(fd, request, ap);
	va_end(ap);

	return (r);
}

RETRACE_REPLACE_V(ioctl, int, (int fd, long request, ...), request, ioctl_v, (fd, request, ap))
