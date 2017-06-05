#ifndef __RETRACE_CHAR_H__
#define __RETRACE_CHAR_H__

typedef int (*rtr_tolower_t)(int c);
typedef int (*rtr_toupper_t)(int c);
typedef int (*rtr_putc_t)(int c, FILE *stream);

rtr_tolower_t real_tolower;
rtr_toupper_t real_toupper;
rtr_putc_t    real_putc;

#endif /* __RETRACE_CHAR_H__ */
