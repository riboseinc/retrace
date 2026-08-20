/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * PE-section registry lookup (TODO.windows/05). See section_walk.h
 * for the design. The walk reads the calling module's own PE
 * headers via GetModuleHandleEx(FROM_ADDRESS): registry arrays and
 * this code live in the same final image (the retrace DLL, or a
 * test executable that links the object libraries directly), so
 * "own module" is always the right one.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "section_walk.h"

#include <stdio.h>
#include <string.h>

/*
 * Element sizes for the compacting lookup (the registry structs
 * themselves; same final image, no layering violation).
 */
#include "actions.h"
#include "data_types.h"
#include "funcs.h"

struct role_map {
	const char *key;   /* POSIX walker spelling (seg+sec) */
	const char *sec;   /* PE section name (<= 8 chars) */
};

static const struct role_map g_roles[] = {
	{ "__DATA__retrace_acts", ".rtrA" },
	{ "__DATA__retrace_funcs", ".rtrF" },
	{ "__DATA__retrace_dt", ".rtrD" },
	{ NULL, NULL }
};

/* diagnostics: list the module's PE sections (CI-visible) */
void retrace_win_dump_sections(void)
{
	HMODULE module = NULL;
	const unsigned char *base;
	const void *dos;
	const void *nt;
	const void *sh;
	unsigned int i;

	if (!GetModuleHandleExA(
		    GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
			    GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		    (LPCSTR)(uintptr_t)&retrace_win_dump_sections,
		    &module))
		return;
	base = (const unsigned char *)module;
	dos = base;
	if (((const IMAGE_DOS_HEADER *)dos)->e_magic !=
	    IMAGE_DOS_SIGNATURE)
		return;
	nt = base + ((const IMAGE_DOS_HEADER *)dos)->e_lfanew;
	if (((const IMAGE_NT_HEADERS *)nt)->Signature !=
	    IMAGE_NT_SIGNATURE)
		return;
	sh = IMAGE_FIRST_SECTION((const IMAGE_NT_HEADERS *)nt);
	for (i = 0;
	     i < ((const IMAGE_NT_HEADERS *)nt)->FileHeader
			 .NumberOfSections;
	     i++) {
		const IMAGE_SECTION_HEADER *sect =
			&((const IMAGE_SECTION_HEADER *)sh)[i];
		char name[9];

		memcpy(name, sect->Name, 8);
		name[8] = '\0';
		printf("pe-section %2u: %-8s vsize=%lu\n", i, name,
			(unsigned long)sect->Misc.VirtualSize);
	}
}

int retrace_win_section_lookup(const char *key,
			       void **addr,
			       unsigned long *size)
{
	const struct role_map *role;
	HMODULE module = NULL;
	const unsigned char *base;
	const void *dos;
	const void *nt;
	unsigned int i;
	unsigned int nsects;

	if (key == NULL || addr == NULL || size == NULL)
		return 0;

	for (role = g_roles; role->key != NULL; role++) {
		if (strcmp(role->key, key) == 0)
			break;
	}
	if (role->key == NULL)
		return 0;

	if (!GetModuleHandleExA(
		    GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
			    GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		    (LPCSTR)(uintptr_t)&retrace_win_section_lookup,
		    &module))
		return 0;

	base = (const unsigned char *)module;
	dos = base;
	if (((const IMAGE_DOS_HEADER *)dos)->e_magic !=
	    IMAGE_DOS_SIGNATURE)
		return 0;
	nt = base + ((const IMAGE_DOS_HEADER *)dos)->e_lfanew;
	if (((const IMAGE_NT_HEADERS *)nt)->Signature !=
	    IMAGE_NT_SIGNATURE)
		return 0;

	nsects = ((const IMAGE_NT_HEADERS *)nt)->FileHeader
			 .NumberOfSections;
	{
		const IMAGE_SECTION_HEADER *sh =
			IMAGE_FIRST_SECTION((const IMAGE_NT_HEADERS *)nt);

		for (i = 0; i < nsects; i++) {
			char name[9];

			memcpy(name, sh[i].Name, 8);
			name[8] = '\0';
			if (strcmp(name, role->sec) != 0)
				continue;
			if (sh[i].Misc.VirtualSize == 0)
				return 0;
			*addr = (void *)(base + sh[i].VirtualAddress);
			*size = (unsigned long)sh[i].Misc.VirtualSize;
			return 1;
		}
	}
	return 0;
}

/*
 * MSVC pads each object's section CONTRIBUTION up to the input
 * section's alignment (observed: 8 bytes per odd-count file --
 * sizeof(FuncPrototype) % 16 == 8). The POSIX walkers expect ONE
 * dense array, so hand them one: copy the elements into a heap
 * block, skipping the zero-filled gaps the linker inserted.
 *
 * A valid element starts with a NUL-terminated printable name
 * (every registry entry has one by construction); gaps are all
 * zero. Compacted once per role, cached for later lookups.
 */

struct compacted {
	void *addr;
	unsigned long size;
	int done;
};

static struct compacted g_compacted[3];

static int plausible_name(const unsigned char *p, size_t max)
{
	size_t i;

	if (p[0] == '\0')
		return 0;
	for (i = 0; i < max && p[i] != '\0'; i++) {
		if (p[i] < 0x20 || p[i] > 0x7e)
			return 0;
	}
	return i < max;
}

static int compact_role(const unsigned char *raw, unsigned long raw_size,
			size_t elem_size, struct compacted *out)
{
	unsigned char *dense;
	unsigned long pos = 0;
	unsigned long count = 0;

	/* worst case: every element survives */
	dense = (unsigned char *)malloc(raw_size);
	if (dense == NULL)
		return 0;

	while (pos + elem_size <= raw_size) {
		const unsigned char *el = raw + pos;

		if (plausible_name(el, elem_size > 65 ? 65 : elem_size)) {
			memcpy(dense + count * elem_size, el, elem_size);
			count++;
			pos += (unsigned long)elem_size;
			continue;
		}

		/* skip a zero-filled gap to the next candidate */
		if (el[0] == 0) {
			pos += 8; /* contributions align to >= 8 */
			continue;
		}
		break; /* non-name garbage: stop here */
	}

	if (count == 0) {
		free(dense);
		return 0;
	}
	out->addr = dense;
	out->size = count * (unsigned long)elem_size;
	out->done = 1;
	return 1;
}

/*
 * Walker entry point: the portable 4-arg get_section_info maps
 * here on Windows; the role's element size is looked up from the
 * registry structs so the callers stay untouched.
 */
int retrace_win_get_registry(const char *key, void **addr,
			     unsigned long *size)
{
	static const struct {
		const char *key;
		size_t elem_size;
	} elem_sizes[] = {
		{ "__DATA__retrace_acts", sizeof(struct Action) },
		{ "__DATA__retrace_funcs",
		  sizeof(struct FuncPrototype) },
		{ "__DATA__retrace_dt", sizeof(struct DataType) },
		{ NULL, 0 }
	};
	int i;

	for (i = 0; elem_sizes[i].key != NULL; i++) {
		if (strcmp(elem_sizes[i].key, key) == 0)
			return retrace_win_registry_compact(key, addr,
				size, elem_sizes[i].elem_size);
	}
	return retrace_win_section_lookup(key, addr, size);
}

int retrace_win_registry_compact(const char *key, void **addr,
				 unsigned long *size, size_t elem_size)
{
	const struct role_map *role;
	int idx;

	for (role = g_roles; role->key != NULL; role++) {
		if (strcmp(role->key, key) == 0)
			break;
	}
	if (role->key == NULL)
		return 0;
	idx = (int)(role - g_roles);

	if (!g_compacted[idx].done) {
		void *raw_addr = NULL;
		unsigned long raw_size = 0;

		if (!retrace_win_section_lookup(key, &raw_addr,
						&raw_size))
			return 0;
		if (!compact_role((const unsigned char *)raw_addr,
				  raw_size, elem_size,
				  &g_compacted[idx]))
			return 0;
	}
	*addr = g_compacted[idx].addr;
	*size = g_compacted[idx].size;
	return 1;
}
