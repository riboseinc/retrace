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

/*
 * Syscall tables. The numbers below are the stable kernel ABI for
 * x86_64 and aarch64 (see Linux <asm-generic/unistd.h>,
 * arch/x86/entry/syscalls/syscall_64.tbl,
 * arch/arm64/include/uapi/asm/unistd.h). They are stable across kernel
 * versions; the only maintenance is appending new syscalls, never
 * renumbering.
 *
 * The names here are the canonical libc names the engine's prototype
 * table understands (write, read, open, ...). Syscalls without a
 * libc-level interposition target are intentionally omitted (the
 * ptrace loop will pass them through untraced).
 */

#include "syscall_table.h"

#include <string.h>

/*
 * x86_64 syscall table. Subset that maps cleanly to libc functions
 * the engine already prototypes. Numbers from syscall_64.tbl.
 */
static const struct retrace_ptrace_syscall_entry x86_64_table[] = {
	{0, "read"},
	{1, "write"},
	{2, "open"},
	{3, "close"},
	{4, "stat"},
	{5, "fstat"},
	{6, "lstat"},
	{7, "poll"},
	{8, "lseek"},
	{9, "mmap"},
	{10, "mprotect"},
	{11, "munmap"},
	{12, "brk"},
	{13, "rt_sigaction"},
	{14, "rt_sigprocmask"},
	{17, "pread64"},
	{18, "pwrite64"},
	{19, "readv"},
	{20, "writev"},
	{21, "access"},
	{22, "pipe"},
	{23, "select"},
	{24, "sched_yield"},
	{25, "mremap"},
	{28, "madvise"},
	{32, "dup"},
	{33, "dup2"},
	{34, "pause"},
	{35, "nanosleep"},
	{39, "getpid"},
	{40, "socket"},
	{41, "connect"},
	{42, "accept"},
	{43, "sendto"},
	{44, "recvfrom"},
	{45, "sendmsg"},
	{46, "recvmsg"},
	{47, "shutdown"},
	{48, "bind"},
	{49, "listen"},
	{50, "getsockname"},
	{51, "getpeername"},
	{52, "socketpair"},
	{56, "clone"},
	{57, "fork"},
	{58, "vfork"},
	{59, "execve"},
	{60, "exit"},
	{61, "wait4"},
	{62, "kill"},
	{72, "fcntl"},
	{74, "fsync"},
	{75, "fdatasync"},
	{76, "truncate"},
	{77, "ftruncate"},
	{78, "getdents"},
	{79, "getcwd"},
	{80, "chdir"},
	{81, "fchdir"},
	{82, "rename"},
	{83, "mkdir"},
	{84, "rmdir"},
	{85, "creat"},
	{86, "link"},
	{87, "unlink"},
	{88, "symlink"},
	{89, "readlink"},
	{90, "chmod"},
	{91, "fchmod"},
	{92, "chown"},
	{93, "fchown"},
	{94, "lchown"},
	{95, "umask"},
	{96, "gettimeofday"},
	{97, "getrlimit"},
	{98, "getrusage"},
	{102, "getuid"},
	{104, "getgid"},
	{105, "setuid"},
	{106, "setgid"},
	{110, "getppid"},
	{157, "prctl"},
	{202, "futex"},
	{217, "getdents64"},
	{218, "set_tid_address"},
	{231, "exit_group"},
	{232, "epoll_wait"},
	{233, "epoll_ctl"},
	{257, "openat"},
	{258, "mkdirat"},
	{262, "newfstatat"},
	{263, "unlinkat"},
	{264, "renameat"},
	{265, "linkat"},
	{266, "symlinkat"},
	{267, "readlinkat"},
	{268, "fchmodat"},
	{269, "faccessat"},
	{270, "pselect6"},
	{271, "ppoll"},
	{280, "utimensat"},
	{281, "epoll_pwait"},
	{282, "signalfd"},
	{283, "timerfd_create"},
	{284, "eventfd"},
	{287, "epoll_create1"},
	{288, "dup3"},
	{289, "pipe2"},
	{290, "inotify_init1"},
	{291, "preadv"},
	{292, "pwritev"},
	{293, "rt_tgsigqueueinfo"},
	{294, "perf_event_open"},
	{295, "recvmmsg"},
	{296, "fanotify_init"},
	{297, "fanotify_mark"},
	{298, "prlimit64"},
	{300, "name_to_handle_at"},
	{307, "sendmmsg"},
};

/*
 * aarch64 (arm64) syscall table. Uses the generic asm-generic/unistd.h
 * numbering, which differs from x86_64. Numbers from
 * arch/arm64/include/uapi/asm/unistd.h.
 */
static const struct retrace_ptrace_syscall_entry aarch64_table[] = {
	{17, "getcwd"},
	{18, "eventfd2"},
	{19, "epoll_create1"},
	{20, "epoll_ctl"},
	{21, "epoll_pwait"},
	{22, "dup"},
	{23, "dup3"},
	{24, "fcntl"},
	{25, "inotify_init1"},
	{26, "inotify_add_watch"},
	{27, "inotify_rm_watch"},
	{28, "ioctl"},
	{29, "ioprio_set"},
	{30, "ioprio_get"},
	{34, "mknodat"},
	{35, "mkdirat"},
	{36, "unlinkat"},
	{37, "symlinkat"},
	{38, "linkat"},
	{39, "renameat"},
	{40, "umount2"},
	{43, "statfs"},
	{44, "fstatfs"},
	{45, "truncate"},
	{46, "ftruncate"},
	{47, "fallocate"},
	{48, "faccessat"},
	{49, "chdir"},
	{50, "fchdir"},
	{51, "chroot"},
	{52, "fchmod"},
	{53, "fchmodat"},
	{54, "fchownat"},
	{55, "fchown"},
	{56, "openat"},
	{57, "close"},
	{58, "vhangup"},
	{59, "pipe2"},
	{61, "getdents64"},
	{62, "lseek"},
	{63, "read"},
	{64, "write"},
	{65, "readv"},
	{66, "writev"},
	{67, "pread64"},
	{68, "pwrite64"},
	{69, "preadv"},
	{70, "pwritev"},
	{71, "sendfile"},
	{72, "pselect6"},
	{73, "ppoll"},
	{74, "signalfd4"},
	{75, "vmsplice"},
	{76, "splice"},
	{77, "tee"},
	{78, "readlinkat"},
	{79, "newfstatat"},
	{80, "fstat"},
	{81, "sync"},
	{82, "fsync"},
	{83, "fdatasync"},
	{84, "sync_file_range"},
	{85, "timerfd_create"},
	{86, "timerfd_settime"},
	{87, "timerfd_gettime"},
	{88, "utimensat"},
	{89, "acct"},
	{95, "exit"},
	{96, "exit_group"},
	{97, "waitid"},
	{98, "set_tid_address"},
	{99, "unshare"},
	{100, "futex"},
	{101, "set_robust_list"},
	{102, "get_robust_list"},
	{103, "nanosleep"},
	{104, "getitimer"},
	{105, "setitimer"},
	{106, "kexec_load"},
	{107, "init_module"},
	{108, "finit_module"},
	{109, "delete_module"},
	{110, "timer_create"},
	{113, "timer_settime"},
	{116, "timer_getoverrun"},
	{121, "socket"},
	{122, "socketpair"},
	{123, "bind"},
	{124, "listen"},
	{125, "accept"},
	{126, "connect"},
	{127, "getsockname"},
	{128, "getpeername"},
	{129, "sendto"},
	{130, "recvfrom"},
	{131, "setsockopt"},
	{132, "getsockopt"},
	{133, "shutdown"},
	{134, "sendmsg"},
	{135, "recvmsg"},
	{136, "readahead"},
	{137, "brk"},
	{138, "munmap"},
	{139, "mremap"},
	{140, "add_key"},
	{141, "request_key"},
	{142, "keyctl"},
	{143, "clone"},
	{144, "execve"},
	{145, "mmap"},
	{172, "getpid"},
	{173, "getppid"},
	{174, "getuid"},
	{175, "geteuid"},
	{176, "getgid"},
	{177, "getegid"},
	{178, "gettid"},
	{198, "socket"},
	{200, "bind"},
	{203, "connect"},
	{206, "sendmsg"},
	{220, "clone"},
	{221, "execve"},
	{222, "mmap"},
	{223, "fadvise64"},
	{224, "swapcontext"},
};

/* Forward lookup. */
const char *
retrace_ptrace_syscall_name(retrace_ptrace_arch_t arch, long number)
{
	const struct retrace_ptrace_syscall_entry *t;
	size_t					   n, i;

	n = retrace_ptrace_syscall_table(arch, &t);
	if (t == NULL)
		return NULL;

	for (i = 0; i < n; i++) {
		if (t[i].number == number)
			return t[i].name;
	}
	return NULL;
}

/* Reverse lookup (used for engine -> syscall mapping if ever needed). */
long
retrace_ptrace_syscall_number(retrace_ptrace_arch_t arch, const char *name)
{
	const struct retrace_ptrace_syscall_entry *t;
	size_t					   n, i;

	if (name == NULL)
		return -1;

	n = retrace_ptrace_syscall_table(arch, &t);
	if (t == NULL)
		return -1;

	for (i = 0; i < n; i++) {
		if (strcmp(t[i].name, name) == 0)
			return t[i].number;
	}
	return -1;
}

size_t
retrace_ptrace_syscall_table(retrace_ptrace_arch_t			 arch,
			     const struct retrace_ptrace_syscall_entry **out)
{
	switch (arch) {
	case RETRACE_PTRACE_ARCH_X86_64:
		if (out != NULL)
			*out = x86_64_table;
		return sizeof(x86_64_table) / sizeof(x86_64_table[0]);
	case RETRACE_PTRACE_ARCH_AARCH64:
		if (out != NULL)
			*out = aarch64_table;
		return sizeof(aarch64_table) / sizeof(aarch64_table[0]);
	default:
		if (out != NULL)
			*out = NULL;
		return 0;
	}
}
