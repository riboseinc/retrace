#ifndef __RETRACE_MALLOC_H__
#define __RETRACE_MALLOC_H__

typedef void *(*rtr_malloc_t)(size_t bytes);
typedef void (*rtr_free_t)(void *mem);

RETRACE_DECL(malloc);
RETRACE_DECL(free);

#if 0
typedef void *(*rtr_calloc_t)(size_t nmemb, size_t size);
typedef void *(*rtr_realloc_t)();
RETRACE_DECL(calloc);
RETRACE_DECL(realloc);
#endif

#endif /* __RETRACE_MALLOC_H__  */
