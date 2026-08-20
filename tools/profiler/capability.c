/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#include "capability.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void prof_capability_init(struct ProfCapability *c)
{
	memset(c, 0, sizeof(*c));
}

static unsigned int rd16(const unsigned char *p)
{
	return (unsigned int)p[0] | ((unsigned int)p[1] << 8);
}

static unsigned int rd32(const unsigned char *p)
{
	return (unsigned int)p[0] | ((unsigned int)p[1] << 8) |
	       ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

static size_t count_pattern(const unsigned char *buf, size_t n,
			    const unsigned char *pat, size_t patlen)
{
	size_t i, j, hits = 0;

	if (n < patlen)
		return 0;
	for (i = 0; i <= n - patlen; i++) {
		for (j = 0; j < patlen; j++)
			if (buf[i + j] != pat[j])
				break;
		if (j == patlen)
			hits++;
	}
	return hits;
}

/* ---- ELF (32/64, LE): scan PT_LOAD segments with X ---- */

static int scan_elf(const unsigned char *buf, size_t size,
		    struct ProfCapability *c)
{
	int is64 = (buf[4] == 2);
	int le = (buf[5] == 1);
	size_t phoff, phentsize, phnum;
	size_t i;
	int machine_x86 = 0, machine_arm = 0;

	if (!le)
		return 0; /* BE: rare; skip rather than misread */

	if (is64) {
		unsigned int e_machine = rd16(buf + 18);

		phoff = (size_t)rd32(buf + 32) |
			((size_t)rd32(buf + 36) << 32);
		phentsize = rd16(buf + 54);
		phnum = rd16(buf + 56);
		if (e_machine == 0x3e)
			machine_x86 = 1; /* EM_X86_64 */
		else if (e_machine == 0xb7 || e_machine == 0x28)
			machine_arm = 1; /* EM_AARCH64 / EM_ARM */
	} else {
		unsigned int e_machine = rd16(buf + 18);

		phoff = rd32(buf + 28);
		phentsize = rd16(buf + 42);
		phnum = rd16(buf + 44);
		if (e_machine == 3)
			machine_x86 = 1; /* EM_386 */
		else if (e_machine == 0x28 || e_machine == 0xb7)
			machine_arm = 1;
	}

	for (i = 0; i < phnum; i++) {
		const unsigned char *ph;
		unsigned int p_flags, p_type;
		size_t off, filesz;

		if (is64) {
			ph = buf + phoff + i * phentsize;
			if ((size_t)(ph - buf) + 56 > size)
				break;
			p_type = rd32(ph);
			p_flags = rd32(ph + 4);
			off = (size_t)rd32(ph + 8) |
			      ((size_t)rd32(ph + 12) << 32);
			filesz = (size_t)rd32(ph + 32) |
				 ((size_t)rd32(ph + 36) << 32);
		} else {
			ph = buf + phoff + i * phentsize;
			if ((size_t)(ph - buf) + 32 > size)
				break;
			p_type = rd32(ph);
			off = rd32(ph + 4);
			filesz = rd32(ph + 16);
			p_flags = rd32(ph + 24);
		}
		if (p_type != 1) /* PT_LOAD */
			continue;
		if (!(p_flags & 1)) /* PF_X */
			continue;
		if (off >= size || filesz == 0)
			continue;
		if (off + filesz > size)
			filesz = size - off;
		if (machine_x86) {
			static const unsigned char sys64[] = { 0x0f, 0x05 };
			static const unsigned char sys32[] = { 0xcd, 0x80 };

			c->syscall_gadgets += count_pattern(buf + off,
				filesz, sys64, 2);
			c->syscall_gadgets += count_pattern(buf + off,
				filesz, sys32, 2);
		} else if (machine_arm) {
			static const unsigned char svc[] = {
				0x01, 0x00, 0x00, 0xd4 };

			c->syscall_gadgets += count_pattern(buf + off,
				filesz, svc, 4);
		}
	}
	c->has_syscall_gadget = c->syscall_gadgets > 0;
	return 0;
}

/* ---- Mach-O (64 LE): scan __TEXT exec sections ---- */

static int scan_macho(const unsigned char *buf, size_t size,
		      struct ProfCapability *c)
{
	size_t ncmds, off;
	size_t i;
	unsigned int cputype;
	int is_x86 = 0, is_arm = 0;

	if (buf[6] != 0 || buf[7] != 0)
		return 0; /* 32-bit Mach-O: rare, skip */
	cputype = rd32(buf + 4);
	if (cputype == 0x01000007)
		is_x86 = 1; /* CPU_TYPE_X86_64 */
	else if (cputype == 0x0100000c)
		is_arm = 1; /* CPU_TYPE_ARM64 */

	ncmds = rd32(buf + 16);
	off = 32;
	for (i = 0; i < ncmds && off + 8 <= size; i++) {
		unsigned int cmd = rd32(buf + off);
		unsigned int cmdsize = rd32(buf + off + 4);

		if (cmdsize == 0)
			break;
		if (cmd == 0x19) { /* LC_SEGMENT_64 */
			const unsigned char *seg = buf + off;
			size_t nsects = rd32(seg + 64);
			size_t so = off + 72;
			size_t s;

			for (s = 0; s < nsects && so + 80 <= size; s++) {
				const unsigned char *sect = buf + so;
				unsigned int flags = rd32(sect + 64);
				size_t soff = (size_t)rd32(sect + 48) |
					((size_t)rd32(sect + 52) << 32);
				size_t ssize = (size_t)rd32(sect + 56) |
					((size_t)rd32(sect + 60) << 32);
				const char *segname = (const char *)sect;

				if (strncmp(segname, "__TEXT", 6) == 0 &&
				    (flags & 0x80000000u)) { /* S_ATTR_PURE_INSTRUCTIONS */
					if (soff < size && ssize > 0) {
						if (soff + ssize > size)
							ssize = size - soff;
						if (is_x86) {
							static const unsigned char sys64[] = { 0x0f, 0x05 };

							c->syscall_gadgets += count_pattern(buf + soff,
								ssize, sys64, 2);
						} else if (is_arm) {
							static const unsigned char svc[] = { 0x01, 0x00, 0x00, 0xd4 };

							c->syscall_gadgets += count_pattern(buf + soff,
								ssize, svc, 4);
						}
					}
				}
				so += 80;
			}
		}
		off += cmdsize;
	}
	c->has_syscall_gadget = c->syscall_gadgets > 0;
	return 0;
}

/* ---- PE: exec-section gadgets (x64/x86) + ntdll IAT ---- */

static int scan_pe(const unsigned char *buf, size_t size,
		   struct ProfCapability *c)
{
	size_t pe_off = rd32(buf + 0x3c);
	const unsigned char *coff;
	unsigned int nsects;
	size_t opt_size, sect_off;
	unsigned int opt_magic;
	unsigned int i;

	if (pe_off + 24 > size)
		return 0;
	if (rd32(buf + pe_off) != 0x00004550u) /* "PE\0\0" */
		return 0;
	coff = buf + pe_off + 4;
	nsects = rd16(coff + 2);
	opt_size = rd16(coff + 16);
	opt_magic = rd16(coff + 20);
	sect_off = pe_off + 4 + 20 + opt_size;

	/* exec-section syscall gadgets (PE x64 only; arm64 PE too) */
	for (i = 0; i < nsects; i++) {
		const unsigned char *sect = buf + sect_off + i * 40;
		unsigned int chars;
		size_t soff, ssize;

		if ((size_t)(sect - buf) + 40 > size)
			break;
		chars = rd32(sect + 36);
		soff = rd32(sect + 20);
		ssize = rd32(sect + 16);
		if (!(chars & 0x20000000u)) /* IMAGE_SCN_MEM_EXECUTE */
			continue;
		if (soff >= size || ssize == 0)
			continue;
		if (soff + ssize > size)
			ssize = size - soff;
		if (opt_magic == 0x20b) { /* PE32+ (x64) */
			static const unsigned char sys64[] = { 0x0f, 0x05 };

			c->syscall_gadgets += count_pattern(buf + soff,
				ssize, sys64, 2);
		}
		/* PE32 (i386): the syscall path on Windows goes
		 * through ntdll imports, not an int instruction --
		 * no gadget scan for 32-bit PE.
		 */
	}

	/* Import table: ntdll.dll names */
	{
		const unsigned char *opt = coff + 20;
		size_t impdir_rva, impdir_size;
		const unsigned char *idd;

		if (opt_magic == 0x20b) {
			impdir_rva = rd32(opt + 120);
			impdir_size = rd32(opt + 124);
		} else {
			impdir_rva = rd32(opt + 96);
			impdir_size = rd32(opt + 100);
		}
		(void)impdir_size;
		if (impdir_rva == 0)
			return 0;

		/* Translate import-directory RVA to file offset via
		 * the section table.
		 */
		for (i = 0; i < nsects; i++) {
			const unsigned char *sect = buf + sect_off + i * 40;
			size_t vaddr, rawoff, rawsz;
			size_t d;

			if ((size_t)(sect - buf) + 40 > size)
				break;
			vaddr = rd32(sect + 12);
			rawoff = rd32(sect + 20);
			rawsz = rd32(sect + 16);
			if (impdir_rva < vaddr ||
			    impdir_rva >= vaddr + rawsz)
				continue;
			idd = buf + rawoff + (impdir_rva - vaddr);
			for (d = 0; d < 32; d++) {
				const unsigned char *desc = idd + d * 20;
				size_t name_rva, name_off;
				const unsigned char *nbuf;

				if ((size_t)(desc - buf) + 20 > size)
					break;
				name_rva = rd32(desc + 12);
				if (name_rva == 0)
					break;
				if (name_rva < vaddr ||
				    name_rva >= vaddr + rawsz)
					continue;
				name_off = rawoff + (name_rva - vaddr);
				if (name_off >= size)
					continue;
				nbuf = buf + name_off;
				if (size - (size_t)(nbuf - buf) < 10)
					continue;
				if ((nbuf[0] | 0x20) == 'n' &&
				    (nbuf[1] | 0x20) == 't' &&
				    (nbuf[2] | 0x20) == 'd' &&
				    (nbuf[3] | 0x20) == 'l' &&
				    (nbuf[4] | 0x20) == 'l') {
					/* this descriptor imports ntdll;
					 * walk its thunk names
					 */
					size_t oft_rva = rd32(desc + 0);
					size_t ft_rva = rd32(desc + 16);
					size_t thunk_rva = oft_rva ?
						oft_rva : ft_rva;
					size_t t;

					for (t = 0; t < 64; t++) {
						const unsigned char *th;
						size_t hint_rva;

						if (thunk_rva < vaddr ||
						    thunk_rva >= vaddr + rawsz)
							break;
						th = buf + rawoff +
						     (thunk_rva - vaddr) +
						     t * (opt_magic == 0x20b ? 8 : 4);
						if ((size_t)(th - buf) + 8 > size)
							break;
						hint_rva = (opt_magic == 0x20b)
							? (size_t)rd32(th) | ((size_t)rd32(th + 4) << 32)
							: rd32(th);
						if (hint_rva == 0)
							break;
						/* ordinal import if high bit */
						if ((opt_magic == 0x20b &&
						     (hint_rva >> 63)) ||
						    (opt_magic != 0x20b &&
						     (hint_rva >> 31)))
							continue;
						if (hint_rva >= vaddr + rawsz ||
						    hint_rva < vaddr)
							continue;
						{
							const unsigned char *nm = buf + rawoff + (hint_rva - vaddr) + 2;

							if ((size_t)(nm - buf) + 24 < size &&
							    c->ntdll_imports < 16) {
								snprintf(c->ntdll_names[c->ntdll_imports],
									sizeof(c->ntdll_names[0]),
									"%s", (const char *)nm);
							}
							c->ntdll_imports++;
						}
					}
				}
			}
			break;
		}
	}
	c->has_syscall_gadget = c->syscall_gadgets > 0;
	return 0;
}

int prof_capability_scan(const char *path, struct ProfCapability *c)
{
	FILE *f = fopen(path, "rb");
	long sz;
	unsigned char *buf;
	int rc = 0;

	prof_capability_init(c);
	if (f == NULL)
		return -1;
	if (fseek(f, 0, SEEK_END) != 0) {
		fclose(f);
		return -1;
	}
	sz = ftell(f);
	if (sz < 0 || fseek(f, 0, SEEK_SET) != 0) {
		fclose(f);
		return -1;
	}
	buf = (unsigned char *)malloc((size_t)sz);
	if (buf == NULL || fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
		free(buf);
		fclose(f);
		return -1;
	}
	fclose(f);

	if (sz >= 4 && buf[0] == 0x7f && buf[1] == 'E' && buf[2] == 'L' &&
	    buf[3] == 'F')
		rc = scan_elf(buf, (size_t)sz, c);
	else if (sz >= 4 && buf[0] == 0xfe && buf[1] == 0xed &&
		 buf[2] == 0xfa && buf[3] == 0xcf)
		rc = scan_macho(buf, (size_t)sz, c);
	else if (sz >= 2 && buf[0] == 'M' && buf[1] == 'Z')
		rc = scan_pe(buf, (size_t)sz, c);
	/* else: unrecognized format -- nothing scanned, nothing
	 * claimed.
	 */

	free(buf);
	return rc;
}
