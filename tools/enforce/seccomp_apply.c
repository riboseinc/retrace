/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * The seccomp floor (TODO.beyond-libc/01 P0): classic BPF,
 * hand-assembled -- no libseccomp dependency. Deny-list style:
 * each listed syscall returns EPERM; everything else passes.
 * The deny set comes from the spec (the generator drops the
 * unsafe classes the profile proves unused). Sits UNDER the
 * Landlock plane as the coarse, arch-stable floor.
 */

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "seccomp_apply.h"

#if defined(__linux__) && (defined(__x86_64__) || \
	defined(__aarch64__))

#include <unistd.h>

#include <sys/prctl.h>

#include <linux/audit.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <sys/syscall.h>

#ifndef SECCOMP_RET_ERRNO
#define SECCOMP_RET_ERRNO 0x00050000U
#endif
#ifndef SECCOMP_RET_ALLOW
#define SECCOMP_RET_ALLOW 0x7fff0000U
#endif

/*
 * Name -> number from the TOOLCHAIN's own unistd (per-arch
 * truth; no hand-maintained tables to rot). Entries vanish on
 * arches that lack the syscall.
 */
struct sc_name {
	const char *name;
	long nr;
};

#define SC(name) { #name, (long)__NR_##name }

static const struct sc_name sc_names[] = {
#ifdef __NR_socket
	SC(socket),
#endif
#ifdef __NR_socketpair
	SC(socketpair),
#endif
#ifdef __NR_connect
	SC(connect),
#endif
#ifdef __NR_accept
	SC(accept),
#endif
#ifdef __NR_accept4
	SC(accept4),
#endif
#ifdef __NR_sendto
	SC(sendto),
#endif
#ifdef __NR_recvfrom
	SC(recvfrom),
#endif
#ifdef __NR_bind
	SC(bind),
#endif
#ifdef __NR_listen
	SC(listen),
#endif
#ifdef __NR_shutdown
	SC(shutdown),
#endif
#ifdef __NR_execve
	SC(execve),
#endif
#ifdef __NR_execveat
	SC(execveat),
#endif
#ifdef __NR_ptrace
	SC(ptrace),
#endif
#ifdef __NR_mount
	SC(mount),
#endif
#ifdef __NR_umount2
	SC(umount2),
#endif
#ifdef __NR_reboot
	SC(reboot),
#endif
#ifdef __NR_swapon
	SC(swapon),
#endif
#ifdef __NR_swapoff
	SC(swapoff),
#endif
#ifdef __NR_setuid
	SC(setuid),
#endif
#ifdef __NR_setgid
	SC(setgid),
#endif
#ifdef __NR_clone
	SC(clone),
#endif
#ifdef __NR_unshare
	SC(unshare),
#endif
#ifdef __NR_setns
	SC(setns),
#endif
#ifdef __NR_bpf
	SC(bpf),
#endif
#ifdef __NR_keyctl
	SC(keyctl),
#endif
#ifdef __NR_request_key
	SC(request_key),
#endif
#ifdef __NR_add_key
	SC(add_key),
#endif
#ifdef __NR_pivot_root
	SC(pivot_root),
#endif
#ifdef __NR_kexec_load
	SC(kexec_load),
#endif
#ifdef __NR_kexec_file_load
	SC(kexec_file_load),
#endif
#ifdef __NR_perf_event_open
	SC(perf_event_open),
#endif
#ifdef __NR_fanotify_init
	SC(fanotify_init),
#endif
#ifdef __NR_userfaultfd
	SC(userfaultfd),
#endif
#ifdef __NR_init_module
	SC(init_module),
#endif
#ifdef __NR_finit_module
	SC(finit_module),
#endif
#ifdef __NR_delete_module
	SC(delete_module),
#endif
#ifdef __NR_iopl
	SC(iopl),
#endif
#ifdef __NR_ioperm
	SC(ioperm),
#endif
	{NULL, -1},
};

static long sc_nr(const char *name)
{
	const struct sc_name *s;

	for (s = sc_names; s->name != NULL; s++) {
		if (strcmp(s->name, name) == 0)
			return s->nr;
	}
	return -1;
}

#define FILTER_MAX (4 + ENFORCE_SYSCALLS_MAX * 2 + 2)

int enforce_seccomp_apply(const struct enforce_spec *spec)
{
	struct sock_filter f[FILTER_MAX];
	struct sock_fprog prog = {.len = 0, .filter = f};
	size_t i;
	int n = 0;

	if (spec->deny_n == 0)
		return 0;
	if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0)
		return errno == EPERM ? 1 : -1;

	/* load arch (audit arch is the first 32 bits of the nr) */
	f[n].code = 0x20;	/* BPF_LD | BPF_W | BPF_ABS */
	f[n].jt = 0;
	f[n].jf = 0;
	f[n].k = 4;		/* offsetof(seccomp_data, arch) */
	n++;
	f[n].code = 0x15;	/* BPF_JMP | BPF_JEQ | BPF_K */
	f[n].jt = 0;
	f[n].jf = 0;
#ifdef __x86_64__
	f[n].k = AUDIT_ARCH_X86_64;
#else
	f[n].k = AUDIT_ARCH_AARCH64;
#endif
	n++;
	/* load syscall nr */
	f[n].code = 0x20;
	f[n].k = 0;		/* offsetof(seccomp_data, nr) */
	n++;

	for (i = 0; i < spec->deny_n; i++) {
		long nr = sc_nr(spec->deny[i].name);

		if (nr < 0)
			continue;	/* unknown on this arch: skip */
		f[n].code = 0x15;	/* JEQ nr -> EPERM */
		f[n].jt = 0;
		f[n].jf = 1;
		f[n].k = (uint32_t)nr;
		n++;
		f[n].code = 0x06;	/* RET */
		f[n].jt = 0;
		f[n].jf = 0;
		f[n].k = SECCOMP_RET_ERRNO | (EPERM & 0xffff);
		n++;
	}
	/* default allow */
	f[n].code = 0x06;
	f[n].k = SECCOMP_RET_ALLOW;
	n++;
	prog.len = (uint16_t)n;

	if (syscall(__NR_seccomp, SECCOMP_SET_MODE_FILTER, 0, &prog) != 0)
		return errno == EINVAL || errno == ENOSYS ? 1 : -1;
	return 0;
}

#else

int enforce_seccomp_apply(const struct enforce_spec *spec)
{
	(void)spec;
	return 1;	/* floor unsupported off Linux-64 */
}

#endif
