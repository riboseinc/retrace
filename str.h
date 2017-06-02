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

rtr_strcpy_t real_strcpy;
rtr_strncpy_t real_strncpy;
rtr_strcat_t real_strcat;
rtr_strncat_t real_strncat;
rtr_strcmp_t real_strcmp;
rtr_strncmp_t real_strncmp;
rtr_strstr_t real_strstr;
rtr_strlen_t real_strlen;

#endif /* __RETRACE_STR_H__ */
