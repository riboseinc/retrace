/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Cross-platform clock helpers. Shared POSIX + Win32 code.
 *
 * Real platform.c (and platform_unix.c / platform_win.c) lands in
 * Phase 3 alongside http_client.c.
 */

/* Linux glibc needs _POSIX_C_SOURCE >= 199309L for clock_gettime
 * and CLOCK_REALTIME/MONOTONIC. macOS declares them by default.
 * Define before any system header.
 */
#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "platform.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <time.h>
#endif

otlp_status_t
otlp_platform_now_unix_nano(uint64_t *out)
{
	if (!out)
		return OTLP_ERR_NULL;

#if defined(_WIN32)
	FILETIME ft;
	ULARGE_INTEGER li;

	GetSystemTimeAsFileTime(&ft);
	li.LowPart = ft.dwLowDateTime;
	li.HighPart = ft.dwHighDateTime;
	/* FILETIME is 100ns intervals since 1601-01-01. Unix epoch
	 * is 11644473600 seconds before that. */
	*out = (li.QuadPart - 116444736000000000ULL) * 100;
	return OTLP_OK;
#else
	struct timespec ts;

	if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
		return OTLP_ERR_NETWORK;
	*out = (uint64_t) ts.tv_sec * 1000000000ULL + (uint64_t) ts.tv_nsec;
	return OTLP_OK;
#endif
}

uint64_t
otlp_platform_now_mono_ms(void)
{
	uint64_t n;

	return (otlp_platform_now_mono_nano(&n) == OTLP_OK) ? n / 1000000ULL
							    : 0;
}

otlp_status_t
otlp_platform_now_mono_nano(uint64_t *out)
{
	if (!out)
		return OTLP_ERR_NULL;

#if defined(_WIN32)
	LARGE_INTEGER freq;
	LARGE_INTEGER counter;

	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&counter);
	if (freq.QuadPart == 0)
		return OTLP_ERR_NETWORK;
	*out = (uint64_t)(counter.QuadPart * 1000000000ULL / freq.QuadPart);
	return OTLP_OK;
#else
	struct timespec ts;

	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
		return OTLP_ERR_NETWORK;
	*out = (uint64_t) ts.tv_sec * 1000000000ULL + (uint64_t) ts.tv_nsec;
	return OTLP_OK;
#endif
}
