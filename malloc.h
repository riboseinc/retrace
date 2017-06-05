#ifndef __RETRACE_MALLOC_H__
#define __RETRACE_MALLOC_H__

typedef void (*rtr_free_t)(void *mem);
typedef void *(*rtr_malloc_t)(size_t bytes);

rtr_free_t   real_free;
rtr_malloc_t real_malloc;

#endif /* __RETRACE_MALLOC_H__  */
