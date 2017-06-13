#ifndef __RETRACE_STR_H__
#define __RETRACE_STR_H__

typedef char *(*rtr_strcpy_t)(char *s1, const char *s2);
typedef char *(*rtr_strncpy_t)(char *s1, const char *s2, size_t n);
typedef char *(*rtr_strcat_t)(char *s1, const char *s2);
typedef char *(*rtr_strncat_t)(char *s1, const char *s2, size_t n);
typedef int (*rtr_strcmp_t)(const char *s1, const char *s2);
typedef int (*rtr_strncmp_t)(const char *s1, const char *s2, size_t n);
typedef char *(*rtr_strstr_t)(const char *s1, const char *s2);
typedef size_t (*rtr_strlen_t)(const char *s);
typedef char *(*rtr_strchr_t)(const char *s, int c);

RETRACE_DECL(strcpy);
RETRACE_DECL(strncpy);
RETRACE_DECL(strcat);
RETRACE_DECL(strncat);
RETRACE_DECL(strcmp);
RETRACE_DECL(strncmp);
RETRACE_DECL(strstr);
RETRACE_DECL(strlen);
RETRACE_DECL(strchr);

#endif /* __RETRACE_STR_H__ */
