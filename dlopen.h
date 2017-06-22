#ifndef __RETRACE_DLOPEN_H__
#define __RETRACE_DLOPEN_H__


typedef void *(*rtr_dlopen_t)(const char *filename, int flag);
typedef char *(*rtr_dlerror_t)(void);
typedef void *(*rtr_dlsym_t)(void *handle, const char *symbol);
typedef int (*rtr_dlclose_t)(void *handle);

RETRACE_DECL(dlopen);
RETRACE_DECL(dlerror);
#ifdef HAVE_ATOMIC_BUILTINS
RETRACE_DECL(dlsym);
#endif
RETRACE_DECL(dlclose);


#endif /* __RETRACE_DLOPEN_H__ */
