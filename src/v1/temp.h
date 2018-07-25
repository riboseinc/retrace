#ifndef __RETRACE_TEMP_H__
#define __RETRACE_TEMP_H__

typedef char *(*rtr_mktemp_t)(char *template);
typedef char *(*rtr_mkdtemp_t)(char *template);
typedef int (*rtr_mkstemp_t)(char *template);
typedef int (*rtr_mkostemp_t)(char *template, int flags);
typedef int (*rtr_mkstemps_t)(char *template, int suffixlen);
typedef int (*rtr_mkostemps_t)(char *template, int suffixlen, int flags);
typedef char *(*rtr_tempnam_t)(const char *dir, const char *pfx);
typedef FILE *(*rtr_tmpfile_t)(void);
typedef char *(*rtr_tmpnam_t)(char *s);


RETRACE_DECL(mktemp);
RETRACE_DECL(mkdtemp);
RETRACE_DECL(mkstemp);
RETRACE_DECL(mkostemp);
RETRACE_DECL(mkstemps);
RETRACE_DECL(mkostemps);
RETRACE_DECL(tempnam);
RETRACE_DECL(tmpfile);
RETRACE_DECL(tmpnam);


#endif /* __RETRACE_TEMP_H__ */
