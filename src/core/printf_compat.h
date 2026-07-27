/*
 * Compatibility shim for glibc's printf.h.
 *
 * glibc's printf.h provides:
 *   - the PA_* enum (PA_INT, PA_CHAR, ..., PA_LAST)
 *   - the PA_FLAG_* family (SHORT, LONG, LONG_LONG, LONG_DOUBLE, PTR, MASK)
 *
 * These are used as integer markers by:
 *   - src/core/data_types.c    (lookup table from printf-arg-type to DataType)
 *   - src/backends/preload_elf/aarch64/arch_spec_bottom.c (varargs walk)
 *   - src/backends/preload_macho/* (custom printf domain registration;
 *     register_printf_domain_function is only available on glibc.)
 *
 * musl (OHOS, Alpine) does not ship printf.h and does not provide
 * register_printf_*. The constants are still needed as integer markers,
 * so we define them locally when the system header is absent.
 *
 * Values mirror glibc exactly so any argtype produced by parse_printf_format
 * on a glibc host still indexes correctly.
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

/* musl does not provide parse_printf_format. Callers use the return value
 * as "how many varargs slots should I prepare?" -- returning 0 makes the
 * caller fall through its no-varargs fast path, so printf-family symbols
 * are still intercepted for log_params but their format-string argument
 * types are not parsed on musl hosts.
 */
static inline int parse_printf_format(const char *fmt, int n, int *argtypes)
{
	(void)fmt; (void)n; (void)argtypes;
	return 0;
}
#endif

#endif /* RETRACE_PRINTF_COMPAT_H */
