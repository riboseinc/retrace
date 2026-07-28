/*
 * Compatibility shim for glibc's printf.h.
 *
 * glibc's printf.h provides the PA_* enum, the PA_FLAG_* family, and
 * parse_printf_format().
 *
 * Used as integer markers by data_types.c (lookup table from
 * printf-arg-type to DataType), preload_elf arch_spec_bottom.c
 * (varargs walk: parse fmt, read the right number of args from the
 * captured frame, pass them through dispatch to the real libc impl),
 * and preload_macho backends (custom printf domain registration;
 * register_printf_domain_function is only available on glibc/Darwin).
 *
 * musl (OHOS, Alpine) does not ship printf.h and does not provide
 * register_printf_*. The constants are still needed as integer markers,
 * so we define them locally when the system header is absent.
 *
 * Values mirror glibc exactly so any argtype produced by
 * parse_printf_format on a glibc host still indexes correctly.
 */
#ifndef RETRACE_PRINTF_COMPAT_H
#define RETRACE_PRINTF_COMPAT_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if HAVE_PRINTF_H
#include <printf.h>
#else
enum {
	PA_INT      = 0,
	PA_CHAR     = 1,
	PA_WCHAR    = 2,
	PA_STRING   = 3,
	PA_WSTRING  = 4,
	PA_POINTER  = 5,
	PA_FLOAT    = 6,
	PA_DOUBLE   = 7,
	PA_LAST     = 8
};
#define PA_FLAG_SHORT       0x04
#define PA_FLAG_LONG        0x08
#define PA_FLAG_LONG_LONG   0x10
#define PA_FLAG_LONG_DOUBLE 0x20
#define PA_FLAG_PTR         0x100
#define PA_FLAG_MASK        (PA_FLAG_PTR | PA_FLAG_LONG_DOUBLE | \
			     PA_FLAG_LONG_LONG | PA_FLAG_LONG | \
			     PA_FLAG_SHORT)

/*
 * musl does not provide parse_printf_format. We need a real
 * implementation: callers (setup_params in arch_spec_bottom.c) use
 * the return value to decide how many varargs slots to read from the
 * captured frame and pass through dispatch. Returning 0 means
 * printf-family symbols get called with only their named args --
 * which on aarch64 AAPCS64 leaves x1..x7 unpopulated, so va_arg
 * reads garbage and segfaults.
 *
 * The implementation here walks the format string and produces one
 * argtype entry per conversion specifier, plus one PA_INT entry per
 * '*' width/precision. Handles the common conversion specifiers
 * (%d %i %u %x %X %o %c %s %p %f %g %e %%) and length modifiers
 * (h, hh, l, ll, L, j, z, t). Not a complete ISO C11 printf parser
 * -- no positional args (%1$d), no glibc custom conversion
 * callbacks. Sufficient for libc usage.
 *
 * Two-call protocol (same as glibc):
 *   count = parse_printf_format(fmt, 0, NULL);
 *   argtypes = malloc(count * sizeof(int));
 *   parse_printf_format(fmt, count, argtypes);
 */
static inline int parse_printf_format(const char *fmt, int n, int *argtypes)
{
	int count = 0;
	const char *p = fmt;
	int i = 0;
	int has_fp = 0;

	while (*p != '\0') {
		if (*p != '%') {
			p++;
			continue;
		}
		p++;

		/* literal %% */
		if (*p == '%') {
			p++;
			continue;
		}

		/* flags */
		while (*p == '-' || *p == '+' || *p == ' ' ||
		       *p == '#' || *p == '0' || *p == '\'') {
			p++;
		}

		/* width: '*' consumes an int arg */
		if (*p == '*') {
			if (n > 0 && i < n && argtypes != NULL)
				argtypes[i] = PA_INT;
			count++; i++;
			p++;
		} else {
			while (*p >= '0' && *p <= '9')
				p++;
		}

		/* precision: .[digits | *] */
		if (*p == '.') {
			p++;
			if (*p == '*') {
				if (n > 0 && i < n && argtypes != NULL)
					argtypes[i] = PA_INT;
				count++; i++;
				p++;
			} else {
				while (*p >= '0' && *p <= '9')
					p++;
			}
		}

		/* length modifiers */
		int flag = 0;

		if (p[0] == 'h' && p[1] == 'h') {
			flag = PA_FLAG_SHORT; p += 2;
		} else if (p[0] == 'l' && p[1] == 'l') {
			flag = PA_FLAG_LONG_LONG; p += 2;
		} else if (*p == 'h') {
			flag = PA_FLAG_SHORT; p++;
		} else if (*p == 'l') {
			flag = PA_FLAG_LONG; p++;
		} else if (*p == 'L') {
			flag = PA_FLAG_LONG_DOUBLE; p++;
		} else if (*p == 'j' || *p == 'z' || *p == 't' || *p == 'q') {
			flag = PA_FLAG_LONG_LONG; p++;
		}

		/* conversion specifier */
		int base = PA_INT;

		switch (*p) {
		case 'd': case 'i': case 'u':
		case 'x': case 'X': case 'o':
			base = PA_INT;
			p++;
			break;
		case 'c':
			base = PA_CHAR;
			p++;
			break;
		case 'C':
			base = PA_WCHAR;
			p++;
			break;
		case 's':
			base = PA_STRING;
			p++;
			break;
		case 'S':
			base = PA_WSTRING;
			p++;
			break;
		case 'p':
			base = PA_POINTER;
			p++;
			break;
		case 'f': case 'F':
		case 'e': case 'E':
		case 'g': case 'G':
		case 'a': case 'A':
			/* default argument promotion lifts float to double
			 * in variadic calls, so %f is PA_DOUBLE. The L
			 * modifier promotes to long double (still PA_DOUBLE
			 * base, with PA_FLAG_LONG_DOUBLE set).
			 */
			base = PA_DOUBLE;
			has_fp = 1;
			break;
		case 'n':
			/* %n writes to an int* argument; consumes a slot */
			base = PA_POINTER | PA_FLAG_PTR;
			p++;
			break;
		case '\0':
			/* malformed; stop */
			return count;
		default:
			/* unknown specifier; assume int */
			p++;
			break;
		}

		if (n > 0 && i < n && argtypes != NULL)
			argtypes[i] = base | flag;
		count++; i++;
	}

	/* FP varargs can't be dispatched correctly on aarch64 (trampoline
	 * captures x0..x7 only, not v0..v7). Fall back to named-args-only
	 * so the callee's va_arg reads the original captured registers
	 * instead of our mis-dispatched ones. printf still produces
	 * correct output for fmts without %f; printf with %f gets
	 * garbage values but does not crash.
	 */
	if (has_fp)
		return 0;

	return count;
}
#endif

#endif /* RETRACE_PRINTF_COMPAT_H */
