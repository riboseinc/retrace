#define _GNU_SOURCE
#include <stddef.h>

#include <dlfcn.h>
#include <link.h>

#include "real_linkmap.h"
#include "real_impls.h"

#ifdef __ANDROID__
#include <elf.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

/*
 * Bionic has no dlinfo/RTLD_DI_LINKMAP. Worse: during this
 * library's own load the linker calls interposed functions
 * (the bionic weak-override law: a weak preload definition
 * beats libc's strong one), so the pre-init engine lane must
 * resolve real implementations WITHOUT dlsym -- dlsym inside
 * a live linker lock produced NULL resolutions and the
 * blr-x8=0 crashes. This resolver touches no linker state:
 * it parses /proc/self/maps and the already-mapped ELF
 * dynamic sections directly (the classic Android hooking
 * technique).
 */
#define ANDROID_MAX_MODS 64

struct android_mod {
	uintptr_t base;
	uintptr_t strtab;
	Elf64_Sym *syms;
	uint32_t *buckets;
	uint32_t nbuckets;
	uint32_t *chain;
	uint32_t symoffset;
};

static uint32_t android_djb2(const char *s)
{
	uint32_t h = 5381;

	while (*s != '\0')
		h = h * 33 + (uint8_t) *s++;
	return h;
}

static void android_mod_init(struct android_mod *m, uintptr_t base)
{
	Elf64_Ehdr *eh = (Elf64_Ehdr *) base;
	Elf64_Phdr *ph = (Elf64_Phdr *) (base + eh->e_phoff);
	uintptr_t dyn_vaddr = 0;
	Elf64_Dyn *d;
	int i;

	for (i = 0; i < eh->e_phnum; i++) {
		if (ph[i].p_type == PT_DYNAMIC)
			dyn_vaddr = ph[i].p_vaddr;
	}
	if (dyn_vaddr == 0)
		return;

	m->base = base;
	d = (Elf64_Dyn *) (base + dyn_vaddr);
	for (; d->d_tag != DT_NULL; d++) {
		switch (d->d_tag) {
		case DT_SYMTAB:
			m->syms = (Elf64_Sym *) d->d_un.d_ptr;
			break;
		case DT_STRTAB:
			m->strtab = d->d_un.d_ptr;
			break;
		case DT_GNU_HASH:
			m->buckets = (uint32_t *) d->d_un.d_ptr;
			break;
		default:
			break;
		}
	}
	if (m->buckets != NULL) {
		uint32_t *gh = m->buckets;

		/* nbuckets, symoffset, bloom_size,
		 * bloom_shift
		 */
		m->symoffset = gh[1];
		m->nbuckets = gh[2];
		m->buckets = gh + 4 + gh[3];
		m->chain = m->buckets + m->nbuckets;
	}
}

static int android_mod_valid(const struct android_mod *m)
{
	return m->syms != NULL && m->strtab != 0 &&
		m->buckets != NULL;
}

static void *android_mod_lookup(struct android_mod *m,
	const char *name)
{
	uint32_t h;
	uint32_t i;
	const char *strtab = (const char *) m->strtab;

	if (!android_mod_valid(m))
		return NULL;

	h = android_djb2(name);
	i = m->buckets[h % m->nbuckets];
	while (i != 0 && i >= m->symoffset) {
		Elf64_Sym *s = &m->syms[i];
		const char *sn = strtab + s->st_name;

		if (strcmp(sn, name) == 0 && s->st_value != 0 &&
		    ELF64_ST_TYPE(s->st_info) == STT_FUNC)
			return (void *) (m->base + s->st_value);
		if (m->chain[i - m->symoffset] & 1)
			break;
		i = m->chain[i - m->symoffset];
	}
	return NULL;
}

/*
 * One-shot scan of /proc/self/maps: every executable mapping
 * that is a page-aligned ELF library becomes a module.
 */
static int android_scan_mods(struct android_mod *mods, int max)
{
	static char buf[16384];
	int fd;
	ssize_t n;
	int cnt = 0;

	fd = open("/proc/self/maps", O_RDONLY);
	if (fd < 0)
		return 0;
	n = read(fd, buf, sizeof(buf) - 1);
	close(fd);
	if (n <= 0)
		return 0;
	buf[n] = '\0';

	{
		char *p = buf;

		while (*p != '\0' && cnt < max) {
			uintptr_t from = 0;
			char perms[5];
			char *line_end;
			char *path = NULL;
			char *q;
			int i;

			while (*p != ' ' && *p != '-') {
				char c = *p++;

				if (c == '\0')
					return cnt;
				from <<= 4;
				from |= (c <= '9') ? c - '0' :
					(c | 0x20) - 'a' + 10;
			}
			line_end = p;
			while (line_end > buf && *line_end != '\n')
				line_end--;
			q = line_end + 1;
			while (*q != '\0' && *q != '\n') {
				if (*q == '/' && q[1] != '\0')
					path = q;
				q++;
			}
			while (*p != ' ')
				p++;	/* skip to end of range */
			p++;
			perms[0] = *p++; perms[1] = *p++;
			perms[2] = *p++; perms[3] = *p++;
			perms[4] = '\0';
			while (*p != '\0' && *p != '\n')
				p++;
			if (*p == '\n')
				p++;

			if (perms[2] != 'x' || path == NULL ||
			    (from & 0xfff) != 0)
				continue;
			if (from < 0x10000)
				continue;

			for (i = 0; i < cnt; i++) {
				if (mods[i].base == from)
					break;
			}
			if (i < cnt)
				continue;

			{
				Elf64_Ehdr *eh = (Elf64_Ehdr *) from;

				if (eh->e_ident[0] != 0x7f ||
				    eh->e_ident[1] != 'E' ||
				    eh->e_ident[2] != 'L' ||
				    eh->e_ident[3] != 'F' ||
				    eh->e_machine != EM_AARCH64)
					continue;
			}

			memset(&mods[cnt], 0, sizeof(mods[cnt]));
			android_mod_init(&mods[cnt], from);
			cnt++;
		}
	}
	return cnt;
}

void *retrace_as_real_from_linkmap(const char *name)
{
	static struct android_mod mods[ANDROID_MAX_MODS];
	static int modcnt = -1;
	uintptr_t self_base = 0;
	int i;

	if (modcnt < 0)
		modcnt = android_scan_mods(mods, ANDROID_MAX_MODS);

	/* never resolve from ourselves: our export IS the
	 * wrapper. Identify our module by a symbol only we
	 * define.
	 */
	for (i = 0; i < modcnt; i++) {
		if (android_mod_lookup(&mods[i],
				"retrace_engine_wrapper") != NULL) {
			self_base = mods[i].base;
			break;
		}
	}

	for (i = 0; i < modcnt; i++) {
		if (mods[i].base == self_base)
			continue;
		/* prefer libc: it is the first big module */
		{
			void *p = android_mod_lookup(&mods[i], name);

			if (p != NULL && (unsigned long) p >=
					0x100000000UL)
				return p;
		}
	}
	return NULL;
}

#else /* glibc / musl link_map chain */
void *retrace_as_real_from_linkmap(const char *name)
{
	struct link_map *lm;
	void *self_base;
	Dl_info di;

	if (dladdr((void *)&retrace_as_real_from_linkmap, &di) == 0)
		return NULL;
	self_base = di.dli_fbase;

	if (dlinfo(dlopen(NULL, RTLD_LAZY), RTLD_DI_LINKMAP, &lm) != 0)
		return NULL;

	for (; lm != NULL; lm = lm->l_next) {
		void *h;
		void *p;

		if (lm->l_addr == 0 || lm->l_name[0] == '\0')
			continue;
		if ((void *)lm->l_addr == self_base)
			continue;	/* ourselves: our export IS the wrapper */
		/* the vdso: its (qemu-provided) symtab is not a
		 * full symbol table -- under qemu-user gdb itself
		 * reports "corrupt string table index" for it, and
		 * dlsym against it returned garbage offsets that the
		 * call_real tail then jumped to (the 0x22325c ghost)
		 */
		if (retrace_real_impls.strncmp != NULL &&
			retrace_real_impls.strncmp(lm->l_name,
				"linux-vdso", 10) == 0)
			continue;
		h = dlopen(lm->l_name, RTLD_LAZY | RTLD_NOLOAD);
		if (h == NULL)
			continue;
		p = dlsym(h, name);
		/* same sanity floor as get_real_safe: the loader's
		 * lookup paths have been observed returning low
		 * garbage under qemu-ppc64le preloads
		 */
		if (p != NULL && (unsigned long) p >= 0x100000000UL)
			return p;
	}
	return NULL;
}

#endif /* __ANDROID__ */
