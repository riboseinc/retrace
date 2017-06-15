#ifndef __RETRACE_CHAR_H__
#define __RETRACE_CHAR_H__

typedef int (*rtr_tolower_t)(int c);
typedef int (*rtr_toupper_t)(int c);
typedef int (*rtr_putc_t)(int c, FILE *stream);
typedef int (*rtr__IO_putc_t)(int c, FILE *stream);

RETRACE_DECL(tolower);
RETRACE_DECL(toupper);
RETRACE_DECL(putc);
#ifndef __APPLE__
RETRACE_DECL(_IO_putc);
#endif

#endif /* __RETRACE_CHAR_H__ */
