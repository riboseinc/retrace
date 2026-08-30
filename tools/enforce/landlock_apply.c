/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Landlock application, raw-syscall edition (TODO.beyond-libc/01).
 *
 * No liblandlock dependency: the UAPI structs are declared here
 * (the layout is stable since 5.13; handled_access_fs is 64-bit
 * since 6.10's FS_IOCTL_DEV -- the mask we use fits both) and
 * the three syscalls are called by number so old toolchains
 * without linux/landlock.h build unchanged. Landlock syscalls
 * are 444/445/446 on ALL 64-bit architectures by allocation.
 */

#include <stdint.h>

#include "landlock_apply.h"

#if defined(__linux__)

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

#ifndef __NR_landlock_create_ruleset
#define __NR_landlock_create_ruleset 444
#endif
#ifndef __NR_landlock_add_rule
#define __NR_landlock_add_rule 445
#endif
#ifndef __NR_landlock_restrict_self
#define __NR_landlock_restrict_self 446
#endif

/* UAPI (linux/landlock.h), trimmed to what we set */
#define LANDLOCK_ACCESS_FS_EXECUTE		(1ULL << 0)
#define LANDLOCK_ACCESS_FS_WRITE_FILE		(1ULL << 1)
#define LANDLOCK_ACCESS_FS_READ_FILE		(1ULL << 2)
#define LANDLOCK_ACCESS_FS_READ_DIR		(1ULL << 3)
#define LANDLOCK_ACCESS_FS_REMOVE_DIR		(1ULL << 4)
#define LANDLOCK_ACCESS_FS_REMOVE_FILE		(1ULL << 5)
#define LANDLOCK_ACCESS_FS_MAKE_CHAR		(1ULL << 6)
#define LANDLOCK_ACCESS_FS_MAKE_DIR		(1ULL << 7)
#define LANDLOCK_ACCESS_FS_MAKE_REG		(1ULL << 8)
#define LANDLOCK_ACCESS_FS_MAKE_SOCK		(1ULL << 9)
#define LANDLOCK_ACCESS_FS_MAKE_FIFO		(1ULL << 10)
#define LANDLOCK_ACCESS_FS_MAKE_BLOCK		(1ULL << 11)
#define LANDLOCK_ACCESS_FS_MAKE_SYM		(1ULL << 12)
#define LANDLOCK_ACCESS_FS_REFER			(1ULL << 13)

struct ll_ruleset_attr {
	uint64_t handled_access_fs;
};

struct ll_path_beneath {
	uint64_t allowed_access;
	int32_t parent_fd;
};

static int ll_create_ruleset(const struct ll_ruleset_attr *attr,
	size_t size, uint32_t flags)
{
	return (int)syscall(__NR_landlock_create_ruleset, attr, size,
		flags);
}

static int ll_add_rule(int ruleset_fd, int rule_type,
	const void *rule, uint32_t flags)
{
	return (int)syscall(__NR_landlock_add_rule, ruleset_fd,
		rule_type, rule, flags);
}

static int ll_restrict_self(int ruleset_fd, uint32_t flags)
{
	return (int)syscall(__NR_landlock_restrict_self, ruleset_fd,
		flags);
}

static uint64_t mask_for(unsigned int access, int is_dir)
{
	uint64_t m = 0;

	if (access & ENF_READ)
		m |= LANDLOCK_ACCESS_FS_READ_FILE;
	if (access & ENF_WRITE)
		m |= LANDLOCK_ACCESS_FS_WRITE_FILE |
			LANDLOCK_ACCESS_FS_READ_FILE;
	if (access & ENF_EXECUTE)
		m |= LANDLOCK_ACCESS_FS_EXECUTE;
	if (is_dir) {
		if (access & ENF_READ)
			m |= LANDLOCK_ACCESS_FS_READ_DIR;
		if (access & ENF_WRITE)
			m |= LANDLOCK_ACCESS_FS_REMOVE_DIR |
				LANDLOCK_ACCESS_FS_REMOVE_FILE |
				LANDLOCK_ACCESS_FS_MAKE_DIR |
				LANDLOCK_ACCESS_FS_MAKE_REG |
				LANDLOCK_ACCESS_FS_MAKE_SYM |
				LANDLOCK_ACCESS_FS_MAKE_SOCK |
				LANDLOCK_ACCESS_FS_MAKE_FIFO |
				LANDLOCK_ACCESS_FS_MAKE_CHAR |
				LANDLOCK_ACCESS_FS_REFER;
	}
	return m;
}

int enforce_landlock_apply(const struct enforce_spec *spec)
{
	const uint64_t handled =
		LANDLOCK_ACCESS_FS_EXECUTE |
		LANDLOCK_ACCESS_FS_WRITE_FILE |
		LANDLOCK_ACCESS_FS_READ_FILE |
		LANDLOCK_ACCESS_FS_READ_DIR |
		LANDLOCK_ACCESS_FS_REMOVE_DIR |
		LANDLOCK_ACCESS_FS_REMOVE_FILE |
		LANDLOCK_ACCESS_FS_MAKE_CHAR |
		LANDLOCK_ACCESS_FS_MAKE_DIR |
		LANDLOCK_ACCESS_FS_MAKE_REG |
		LANDLOCK_ACCESS_FS_MAKE_SOCK |
		LANDLOCK_ACCESS_FS_MAKE_FIFO |
		LANDLOCK_ACCESS_FS_MAKE_BLOCK |
		LANDLOCK_ACCESS_FS_MAKE_SYM;
	struct ll_ruleset_attr attr = {.handled_access_fs = handled};
	int fd;
	size_t i;

	if (spec->rules_n == 0)
		return 0;
	if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0)
		return errno == EPERM ? 1 : -1;
	fd = ll_create_ruleset(&attr, sizeof(attr), 0);
	if (fd < 0) {
		/* an unimplemented landlock syscall raises ENOSYS
		 * (the number is an invalid syscall there); older
		 * kernels raise EINVAL/EOPNOTSUPP -- all mean
		 * "no landlock on this kernel"
		 */
		if (errno == ENOSYS || errno == EOPNOTSUPP ||
		    errno == EINVAL)
			return 1;
		return -1;
	}
	for (i = 0; i < spec->rules_n; i++) {
		const struct enforce_rule *r = &spec->rules[i];
		int path_fd = open(r->path, O_PATH | O_CLOEXEC);
		struct ll_path_beneath rule;
		struct stat st;

		if (path_fd < 0) {
			if (errno == ENOENT || errno == ENOTDIR) {
				/* a declared path may legitimately be
				 * absent on this host (e.g.
				 * ld.so.preload): skip the rule, keep
				 * the rest -- fail-closed stays, the
				 * absent path simply grants nothing
				 */
				fprintf(stderr,
					"retrace-enforce: rule path absent, skipped: %s\n",
					r->path);
				continue;
			}
			close(fd);
			return -1;
		}
		if (fstat(path_fd, &st) != 0 || !S_ISDIR(st.st_mode)) {
			/* file rule: the allowed mask applies to the
			 * file itself; make-rights need the parent,
			 * which the generator emits separately
			 */
			rule.parent_fd = path_fd;
			rule.allowed_access =
				mask_for(r->access, 0) & handled;
		} else {
			rule.parent_fd = path_fd;
			rule.allowed_access =
				mask_for(r->access, 1) & handled;
		}
		if (rule.allowed_access == 0) {
			close(path_fd);
			continue;
		}
		if (ll_add_rule(fd, 1 /* LANDLOCK_RULE_PATH_BENEATH */,
			    &rule, 0) != 0) {
			close(path_fd);
			close(fd);
			return -1;
		}
		close(path_fd);
	}
	if (ll_restrict_self(fd, 0) != 0) {
		close(fd);
		return errno == E2BIG ? -1 : 1;
	}
	close(fd);
	return 0;
}

#else /* !__linux__ */

int enforce_landlock_apply(const struct enforce_spec *spec)
{
	(void)spec;
	return 1;	/* no landlock off Linux */
}

#endif /* __linux__ */
