#ifndef __RETRACE_MALLOC_H__
#define __RETRACE_MALLOC_H__

typedef void *(*rtr_malloc_t)(size_t bytes);
typedef void (*rtr_free_t)(void *mem);
typedef void *(*rtr_calloc_t)(size_t nmemb, size_t size);
typedef void *(*rtr_realloc_t)();

typedef void *(*rtr_memcpy_t)(void *dest, const void *src, size_t n);
typedef void *(*rtr_memmove_t)(void *dest, const void *src, size_t n);
typedef void (*rtr_bcopy_t)(const void *src, void *dest, size_t n);
typedef void *(*rtr_memccpy_t)(void *dest, const void *src, int c, size_t n);

typedef void *(*rtr_mmap_t)(void *addr, size_t length, int prot, int flags,
	int fd, off_t offset);
typedef int (*rtr_munmap_t)(void *addr, size_t length);

/* brk(), sbrk() system calls has been explicitly marked as deprecated on osx */
#ifndef __APPLE__

typedef int (*rtr_brk_t)(void *addr);
typedef void *(*rtr_sbrk_t)(intptr_t increment);

#endif

RETRACE_DECL(malloc);
RETRACE_DECL(free);
RETRACE_DECL(calloc);
RETRACE_DECL(realloc);

RETRACE_DECL(memcpy);
RETRACE_DECL(memmove);
RETRACE_DECL(bcopy);
RETRACE_DECL(memccpy);

RETRACE_DECL(mmap);
RETRACE_DECL(munmap);

#ifndef __APPLE__

RETRACE_DECL(brk);
RETRACE_DECL(sbrk);

#endif

#endif /* __RETRACE_MALLOC_H__  */
