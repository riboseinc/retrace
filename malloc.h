#ifndef __RETRACE_MALLOC_H__
#define __RETRACE_MALLOC_H__

typedef void (*rtr_free_t)(void *mem);
typedef void *(*rtr_malloc_t)(size_t bytes);
typedef void *(*rtr_realloc_t)(void *ptr, size_t size);

RETRACE_DECL(free);
RETRACE_DECL(malloc);
RETRACE_DECL(realloc);

#endif /* __RETRACE_MALLOC_H__  */
