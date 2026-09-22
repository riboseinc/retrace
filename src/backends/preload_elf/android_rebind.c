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

#include <dlfcn.h>
#include <elf.h>
#include <fcntl.h>
#include <link.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

#include "android_rebind.h"
#include "logger.h"

#ifdef __ANDROID__

/*
 * Android v2 tracing (TODO.impl/24): after the engine boots,
 * rebind the TARGET's PLT/GOT slots for every symbol we
 * trampoline, so the target's own libc calls are captured.
 *
 * The trampolines are weak+hidden at load (the bionic
 * weak-override law); the __retrace_wrap_<func> aliases ride
 * at the same addresses and ARE exported -- nothing references
 * them at load, the rebind resolves them by name afterwards.
 *
 * OPT-IN (RETRACE_ANDROID_REBIND=1): with the rebind active
 * the loader's own boot-time libc calls also enter the engine
 * (a deterministic boot-time dispatch flood) and the process
 * exits before main. The remaining work is characterizing
 * which boot-phase callers must be exempted.
 */

struct rebind_ctx {
	uintptr_t exec_base;
	ElfW(Dyn) *exec_dyn;
	uintptr_t self_base;
	uintptr_t self_end;
	int rebound;
};

/* find a module's base + extent from /proc/self/maps: the
 * executable is the first named executable mapping that is
 * neither ours nor a system lib/bin; our library is whichever
 * mapping contains this function's own address. dl_iterate_phdr
 * under qemu+bionic never reaches the callback -- the maps walk
 * is the proven path (real_linkmap.c).
 */
static void maps_walk(struct rebind_ctx *ctx)
{
	static char buf[16384];
	uintptr_t here = (uintptr_t) &maps_walk;
	int fd;
	ssize_t n;
	char *p;

	fd = open("/proc/self/maps", O_RDONLY);
	if (fd < 0)
		return;
	n = read(fd, buf, sizeof(buf) - 1);
	close(fd);
	if (n <= 0)
		return;
	buf[n] = '\0';

	p = buf;
	while (*p != '\0') {
		uintptr_t from = 0, to = 0;
		char perms[5];
		char *line_end;
		char *path = NULL;
		char *q;

		while (*p != ' ' && *p != '-') {
			char c = *p++;

			if (c == '\0')
				return;
			from <<= 4;
			from |= (c <= '9') ? c - '0' :
				(c | 0x20) - 'a' + 10;
		}
		while (*p == '-')
			p++;
		while (*p != ' ') {
			char c = *p++;

			if (c == '\0')
				return;
			to <<= 4;
			to |= (c <= '9') ? c - '0' :
				(c | 0x20) - 'a' + 10;
		}
		line_end = p;
		while (*line_end != '\0' && *line_end != '\n')
			line_end++;
		q = p;
		while (q < line_end) {
			if (*q == '/' && q[1] != '\0')
				path = q;
			q++;
		}
		p++;	/* the space before the perms field */
		perms[0] = *p++; perms[1] = *p++;
		perms[2] = *p++; perms[3] = *p++;
		perms[4] = '\0';
		while (*p != '\0' && *p != '\n')
			p++;
		if (*p == '\n')
			p++;

		if (perms[2] != 'x' || from == 0)
			continue;
		if (here >= from && here < to) {
			ctx->self_base = from;
			ctx->self_end = to;
			continue;
		}
		if (ctx->exec_base == 0 && path != NULL &&
				strstr(path, "libretrace") == NULL &&
				strstr(path, "/bin/") == NULL &&
				strstr(path, "/lib") == NULL) {
			Elf64_Ehdr *eh = (Elf64_Ehdr *) from;

			if (eh->e_ident[0] == 0x7f &&
					eh->e_ident[1] == 'E' &&
					eh->e_machine == EM_AARCH64)
				ctx->exec_base = from;
		}
	}
	if (ctx->exec_base == ctx->self_base)
		ctx->exec_base = 0;
}

/* the GOT stays WRITABLE after the write: bionic resolves
 * lazily, and sibling slots on the same page may still be
 * pending -- restoring read-only would fault their first call */
static int page_write(void *slot, unsigned long value)
{
	unsigned long page = (unsigned long) slot & ~4095UL;

	if (mprotect((void *) page, 4096,
			PROT_READ | PROT_WRITE) != 0)
		return -1;
	*(unsigned long *) slot = value;
	return 0;
}

static void rebind_table(struct rebind_ctx *ctx,
	ElfW(Rela) *rela, size_t relsz,
	ElfW(Sym) *dynsym, const char *strtab)
{
	size_t cnt, i;

	if (rela == NULL || relsz < sizeof(ElfW(Rela)) ||
			dynsym == NULL || strtab == NULL)
		return;
	cnt = relsz / sizeof(ElfW(Rela));
	for (i = 0; i < cnt; i++) {
		unsigned int symidx, type;
		ElfW(Sym) *sym;
		const char *name;
		void *ours;
		void **slot;

		type = ELF64_R_TYPE(rela[i].r_info);
		if (type != R_AARCH64_JUMP_SLOT &&
				type != R_AARCH64_GLOB_DAT)
			continue;
		symidx = ELF64_R_SYM(rela[i].r_info);
		sym = &dynsym[symidx];
		if (sym->st_name == 0)
			continue;
		name = strtab + sym->st_name;
		if (name[0] == '\0')
			continue;
		{
			/* the visible alias: __retrace_wrap_<func> */
			char alias[96];
			const char *suffix = "__retrace_wrap_";
			size_t k = 0, n = 0;

			while (suffix[n] != '\0' && n < sizeof(alias) - 1)
				alias[n++] = suffix[n++];
			while (name[k] != '\0' && n < sizeof(alias) - 1)
				alias[n++] = name[k++];
			alias[n] = '\0';
			ours = dlsym(RTLD_DEFAULT, alias);
		}
		if (ours == NULL)
			continue;
		if ((uintptr_t) ours < ctx->self_base ||
				(uintptr_t) ours >= ctx->self_end)
			continue;
		/* bionic's relocator biases r_offset in place: the
		 * stored value is already absolute.
	 */
		if (rela[i].r_offset < 0x1000UL)
			continue;
		slot = (void **) rela[i].r_offset;
		if (*slot == ours)
			continue;
		if (page_write(slot, (unsigned long) ours) == 0)
			ctx->rebound++;
	}
}

/* Android packed relocations (DT_ANDROID_RELA, "APA1"): the
 * API-26 NDK packs the executable's .rela.dyn -- the GLOB_DAT
 * entries carrying the target's own calls live inside it.
 * Unpack into a caller-held buffer; returns the entry count. */
struct sleb {
	const unsigned char *p;
	const unsigned char *end;
};

static long sleb_get(struct sleb *s)
{
	long result = 0;
	int shift = 0;
	unsigned char b;

	do {
		if (s->p >= s->end)
			return -1;
		b = *s->p++;
		result |= (long) (b & 0x7f) << shift;
		shift += 7;
	} while (b & 0x80);
	if (shift < 64 && (b & 0x40))
		result |= -(1L << shift);
	return result;
}

static long unpack_packed(const unsigned char *blob, size_t blobsz,
	ElfW(Rela) *out, size_t out_max)
{
	struct sleb s = { blob + 4, blob + blobsz };
	unsigned long offset = 0, info = 0, addend = 0;
	size_t cnt = 0;

	if (blobsz < 4 || memcmp(blob, "APA1", 4) != 0)
		return -1;
	while (s.p < s.end) {
		long flags = sleb_get(&s);
		long gcount = sleb_get(&s);
		long gdelta = (flags & 2) ? sleb_get(&s) : 0;
		long i;

		if (flags < 0 || gcount < 0 || gdelta < 0)
			break;
		if ((size_t) (cnt + gcount) > out_max)
			gcount = (long) (out_max - cnt);
		for (i = 0; i < gcount; i++) {
			long odelta = (flags & 2) ? gdelta :
				sleb_get(&s);
			long idelta = 0, adelta = 0;

			if (odelta < 0)
				return cnt;
			offset += (unsigned long) odelta;
			if (flags & 1) {
				idelta = sleb_get(&s);
				if (idelta < 0)
					return cnt;
				info += (unsigned long) idelta;
			}
			if (flags & 4) {
				if (i == 0 || (flags & 1)) {
					adelta = sleb_get(&s);
					if (adelta < 0)
						return cnt;
					addend += (unsigned long) adelta;
				}
			} else {
				addend = offset;
			}
			out[cnt].r_offset = offset;
			out[cnt].r_info = info;
			out[cnt].r_addend = (ElfW(Sxword)) addend;
			cnt++;
		}
	}
	return (long) cnt;
}

void retrace_android_rebind(void)
{
	struct rebind_ctx ctx;
	ElfW(Dyn) *dyn = NULL;
	ElfW(Rela) *jmprel = NULL, *rela = NULL;
	size_t jmprelsz = 0, relasz = 0;
	ElfW(Sym) *dynsym = NULL;
	const char *strtab = NULL;
	const unsigned char *packed = NULL;
	size_t packedsz = 0;
	static ElfW(Rela) unpacked[4096];
	int i;

	{
		/* OPT-IN until the boot-phase interposition
		 * semantics are characterized (TODO.impl/24) */
		const char *env = getenv("RETRACE_ANDROID_REBIND");

		if (env == NULL || env[0] != '1')
			return;
	}

	memset(&ctx, 0, sizeof(ctx));
	maps_walk(&ctx);
	if (ctx.exec_base == 0 || ctx.self_base == 0)
		return;
	{
		Elf64_Ehdr *eh = (Elf64_Ehdr *) ctx.exec_base;
		ElfW(Phdr) *ph = (ElfW(Phdr) *) (ctx.exec_base +
			eh->e_phoff);

		for (i = 0; i < eh->e_phnum; i++) {
			if (ph[i].p_type == PT_DYNAMIC) {
				dyn = (ElfW(Dyn) *) (ctx.exec_base +
					ph[i].p_vaddr);
				break;
			}
		}
	}
	if (dyn == NULL)
		return;
	for (i = 0; dyn[i].d_tag != DT_NULL; i++) {
		switch (dyn[i].d_tag) {
		case DT_JMPREL:
			jmprel = (ElfW(Rela) *) (ctx.exec_base +
				dyn[i].d_un.d_ptr);
			break;
		case DT_PLTRELSZ:
			jmprelsz = dyn[i].d_un.d_val;
			break;
		case DT_RELA:
			rela = (ElfW(Rela) *) (ctx.exec_base +
				dyn[i].d_un.d_ptr);
			break;
		case DT_RELASZ:
			relasz = dyn[i].d_un.d_val;
			break;
		case DT_SYMTAB:
			dynsym = (ElfW(Sym) *) (ctx.exec_base +
				dyn[i].d_un.d_ptr);
			break;
		case DT_STRTAB:
			strtab = (const char *) (ctx.exec_base +
				dyn[i].d_un.d_ptr);
			break;
		case 0x6000000f:	/* DT_ANDROID_RELA */
			packed = (const unsigned char *) (ctx.exec_base +
				dyn[i].d_un.d_ptr);
			break;
		case 0x60000010:	/* DT_ANDROID_RELASZ */
			packedsz = dyn[i].d_un.d_val;
			break;
		default:
			break;
		}
	}
	if (dynsym == NULL || strtab == NULL)
		return;

	rebind_table(&ctx, jmprel, jmprelsz, dynsym, strtab);
	if (packed != NULL && packedsz != 0) {
		long n = unpack_packed(packed, packedsz,
				unpacked, 4096);

		if (n > 0)
			rebind_table(&ctx, unpacked, (size_t) n *
				sizeof(ElfW(Rela)), dynsym, strtab);
	} else {
		rebind_table(&ctx, rela, relasz, dynsym, strtab);
	}

	log_info("android rebind: %d PLT/GOT slots now enter the engine",
		ctx.rebound);
}

#endif /* __ANDROID__ */
