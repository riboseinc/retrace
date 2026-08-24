/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Atomic operations compatibility shim.
 *
 * Wraps the small subset of C11 <stdatomic.h> we use:
 *   - atomic_load / atomic_store / atomic_compare_exchange_weak on uint64_t
 *   - atomic_load / atomic_store / atomic_fetch_add on int
 *
 * On GCC/Clang this is a thin pass-through to <stdatomic.h> so we
 * get optimal code generation and the standard memory model.
 *
 * On MSVC, we use compiler intrinsics (_InterlockedCompareExchange64,
 * _InterlockedExchange64, _InterlockedExchangeAdd). These have full
 * barriers by default, which is slightly stronger than the C11
 * orders we ask for, but correct — the strong order is a no-op for
 * our lock-free queue and counter invariants.
 *
 * Scope: uint64_t and int only. The library uses no other atomic
 * types. Adding more is straightforward (copy the pattern).
 */
#ifndef OTLP_C_ATOMIC_COMPAT_H
#define OTLP_C_ATOMIC_COMPAT_H

#include <stdint.h>

#if defined(_MSC_VER)

#include <intrin.h>

typedef volatile uint64_t otlp_atomic_u64;
typedef volatile int      otlp_atomic_int;

#define OTLP_MEMORY_ORDER_RELAXED 0
#define OTLP_MEMORY_ORDER_ACQUIRE 1
#define OTLP_MEMORY_ORDER_RELEASE 2

/* ── uint64_t ── */

static inline uint64_t
otlp_atomic_load_u64(otlp_atomic_u64 *a, int mo)
{
	(void) mo;
	/* Aligned volatile read of 64 bits is atomic on x64/ARM64. */
	return *a;
}

static inline void
otlp_atomic_store_u64(otlp_atomic_u64 *a, uint64_t v, int mo)
{
	(void) mo;
	_InterlockedExchange64((long long *) a, (long long) v);
}

static inline int
otlp_atomic_cas_u64(otlp_atomic_u64 *a,
		    uint64_t	       *expected,
		    uint64_t		desired,
		    int			succ,
		    int			fail)
{
	uint64_t old;

	(void) succ;
	(void) fail;
	old = (uint64_t) _InterlockedCompareExchange64(
	    (long long *) a, (long long) desired, (long long) *expected);
	if (old == *expected)
		return 1;
	*expected = old;
	return 0;
}

static inline uint64_t
otlp_atomic_fetch_add_u64(otlp_atomic_u64 *a, uint64_t delta, int mo)
{
	(void) mo;
	return (uint64_t) _InterlockedExchangeAdd64(
	    (long long *) a, (long long) delta);
}

/* ── int ── */

static inline int
otlp_atomic_load_int(otlp_atomic_int *a, int mo)
{
	(void) mo;
	return *a;
}

static inline void
otlp_atomic_store_int(otlp_atomic_int *a, int v, int mo)
{
	(void) mo;
	_InterlockedExchange((long volatile *) a, (long) v);
}

static inline int
otlp_atomic_fetch_add_int(otlp_atomic_int *a, int delta, int mo)
{
	(void) mo;
	return (int) _InterlockedExchangeAdd((long volatile *) a, (long) delta);
}

#else

#include <stdatomic.h>

typedef _Atomic uint64_t otlp_atomic_u64;
typedef _Atomic int      otlp_atomic_int;

#define OTLP_MEMORY_ORDER_RELAXED memory_order_relaxed
#define OTLP_MEMORY_ORDER_ACQUIRE memory_order_acquire
#define OTLP_MEMORY_ORDER_RELEASE memory_order_release

static inline uint64_t
otlp_atomic_load_u64(otlp_atomic_u64 *a, int mo)
{
	return atomic_load_explicit(a, mo);
}

static inline void
otlp_atomic_store_u64(otlp_atomic_u64 *a, uint64_t v, int mo)
{
	atomic_store_explicit(a, v, mo);
}

static inline int
otlp_atomic_cas_u64(otlp_atomic_u64 *a,
		    uint64_t	       *expected,
		    uint64_t		desired,
		    int			succ,
		    int			fail)
{
	return atomic_compare_exchange_weak_explicit(
	    a, expected, desired, succ, fail);
}

static inline uint64_t
otlp_atomic_fetch_add_u64(otlp_atomic_u64 *a, uint64_t delta, int mo)
{
	return atomic_fetch_add_explicit(a, delta, mo);
}

static inline int
otlp_atomic_load_int(otlp_atomic_int *a, int mo)
{
	return atomic_load_explicit(a, mo);
}

static inline void
otlp_atomic_store_int(otlp_atomic_int *a, int v, int mo)
{
	atomic_store_explicit(a, v, mo);
}

static inline int
otlp_atomic_fetch_add_int(otlp_atomic_int *a, int delta, int mo)
{
	return atomic_fetch_add_explicit(a, delta, mo);
}

#endif

#endif
