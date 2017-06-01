#ifndef __RETRACE_CHAR_H__
#define __RETRACE_CHAR_H__

static int (*real_tolower)(int c);
static int (*real_toupper)(int c);
static int (*real_putc)(int c, FILE *stream);

#endif /* __RETRACE_CHAR_H__ */
