#ifndef __RETRACE_STR_H__
#define __RETRACE_STR_H__

char *(*real_strcpy)(char *s1, const char *s2);
char *(*real_strncpy)(char *s1, const char *s2, size_t n);
char *(*real_strcat)(char *s1, const char *s2);
char *(*real_strncat)(char *s1, const char *s2, size_t n);
int (*real_strcmp)(const char *s1, const char *s2);
int (*real_strncmp)(const char *s1, const char *s2, size_t n);
char *(*real_strstr)(const char *s1, const char *s2);
size_t (*real_strlen)(const char *s);

#endif /* __RETRACE_STR_H__ */
