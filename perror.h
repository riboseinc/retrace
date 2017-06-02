#ifndef __RETRACE_PERROR_H__
#define __RETRACE_PERROR_H__

typedef void (*rtr_perror_t)(const char *s);
rtr_perror_t real_perror;

#endif /* __RETRACE_PERROR_H__ */
