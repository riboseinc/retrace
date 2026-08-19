/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#ifndef RETRACE_CORE_POSIX_COMPAT_H_
#define RETRACE_CORE_POSIX_COMPAT_H_

/*
 * Platform shim for the v2 core (TODO.windows/04): threads,
 * thread-specific storage, and symbol/module lookup behind one
 * rc_* namespace. POSIX systems map straight onto pthreads and
 * dlfcn; Windows maps onto SRWLOCK, fiber-local storage,
 * CreateThread, and the loader/PE APIs. This header is the ONLY
 * place in src/core that touches platform thread/symbol APIs
 * directly -- everything else goes through retrace_real_impls
 * (fields typed with the rc_* names below), so the reentrancy
 * guard is preserved on both platforms.
 */

#if defined(_WIN32)

/* NOGDI: wingdi.h #defines ERROR/DELETE which collide with the
 * core's severity enum (logger.h).
 */
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include <windows.h>
#include <process.h>
#include <time.h>

/*
 * windows.h macros that collide with core identifiers
 * (logger.h's Severity enum). Every Windows TU gets windows.h
 * through this header, so the undefs are global policy.
 */
#undef ERROR
#undef SEVERITY_ERROR
#undef SEVERITY_SUCCESS
#undef DELETE

/*
 * Mutex: SRWLOCK is the right weight (no recursion, no timed
 * waits -- the core never recurses into its own locks). It has
 * no init/destroy calls: static SRWLOCK_INIT or zeroed memory
 * is a valid lock, and destroy is a no-op.
 */
typedef SRWLOCK rc_mutex_t;
#define RC_MUTEX_STATIC_INIT SRWLOCK_INIT

static inline int rc_mutex_init_win(rc_mutex_t *m)
{
	InitializeSRWLock(m);
	return 0;
}

static inline int rc_mutex_lock_win(rc_mutex_t *m)
{
	AcquireSRWLockExclusive(m);
	return 0;
}

static inline int rc_mutex_unlock_win(rc_mutex_t *m)
{
	ReleaseSRWLockExclusive(m);
	return 0;
}

static inline int rc_mutex_destroy_win(rc_mutex_t *m)
{
	(void)m;
	return 0;
}

/*
 * Thread-specific storage with a destructor: FlsAlloc is the
 * exact pthread_key_create analogue (destructor runs at thread
 * exit; on process exit it runs for FLS values still set).
 */
typedef DWORD rc_tss_t;

static inline int rc_tss_create_win(rc_tss_t *key,
	void (*dtor)(void *))
{
	*key = FlsAlloc((PFLS_CALLBACK_FUNCTION)dtor);
	return (*key == FLS_OUT_OF_INDEXES) ? -1 : 0;
}

static inline void *rc_tss_get_win(rc_tss_t key)
{
	return FlsGetValue(key);
}

static inline int rc_tss_set_win(rc_tss_t key, const void *val)
{
	return FlsSetValue(key, (LPVOID)val) ? 0 : -1;
}

static inline int rc_tss_delete_win(rc_tss_t key)
{
	return FlsFree(key) ? 0 : -1;
}

static inline unsigned long rc_thread_self_tid_win(void)
{
	return GetCurrentThreadId();
}

/*
 * Threads. rc_thread_t is the thread id; creation also yields
 * a waitable handle for join.
 */
typedef unsigned long rc_thread_t;

typedef struct rc_thread_h {
	HANDLE handle;
	void *(*fn)(void *arg);
	void *arg;
} rc_thread_h;

static inline DWORD WINAPI rc_thread_thunk(LPVOID p)
{
	rc_thread_h *t = (rc_thread_h *)p;
	void *ret = t->fn(t->arg);

	/* The joiner frees the descriptor after WaitForSingleObject
	 * observes the exit; passing the result via the (unused)
	 * exit code would truncate pointers. Callers that need the
	 * return value store it themselves.
	 */
	(void)ret;
	return 0;
}

static inline int rc_thread_create_win(rc_thread_h *t,
	void *(*fn)(void *), void *arg)
{
	t->fn = fn;
	t->arg = arg;
	t->handle = CreateThread(NULL, 0, rc_thread_thunk, t, 0,
		NULL);
	return (t->handle == NULL) ? -1 : 0;
}

static inline int rc_thread_join_win(rc_thread_h *t)
{
	int rc = (WaitForSingleObject(t->handle, INFINITE) ==
		  WAIT_OBJECT_0) ? 0 : -1;

	CloseHandle(t->handle);
	t->handle = NULL;
	return rc;
}

static inline unsigned long rc_thread_self_win(void)
{
	return GetCurrentThreadId();
}

static inline void rc_monotonic_win(struct timespec *ts)
{
	LARGE_INTEGER freq;
	LARGE_INTEGER cnt;
	double sec;

	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&cnt);
	sec = (double)cnt.QuadPart / (double)freq.QuadPart;
	ts->tv_sec = (time_t)sec;
	ts->tv_nsec = (long)((sec - (double)ts->tv_sec) * 1e9);
}

static inline int rc_cas_win(volatile int *ptr, int oldv,
	int newv)
{
	return InterlockedCompareExchange((volatile LONG *)ptr, newv,
		oldv) == oldv;
}

static inline void rc_nanosleep_win(const struct timespec *ts)
{
	DWORD ms = (DWORD)((ts->tv_sec * 1000LL) +
		(ts->tv_nsec / 1000000L));

	Sleep(ms);
}

static inline long rc_getpid_win(void)
{
	return (long)GetCurrentProcessId();
}

/*
 * Module lookup for caller identification (rc_dladdr). Windows
 * has no dladdr; RtlPcToFileHeader yields the module base and
 * GetModuleFileNameA the path. dli_sname stays NULL: caller
 * SYMBOL matching degrades to module matching on Windows
 * (documented limitation until dbghelp-driven symbolication).
 */
typedef struct rc_dl_info {
	const char *dli_fname;  /* module path */
	void *dli_fbase;        /* module base */
	const char *dli_sname;  /* NULL on Windows */
	void *dli_saddr;        /* NULL on Windows */
} rc_dl_info_t;

static inline int rc_dladdr_win(void *addr, rc_dl_info_t *info)
{
	void *base = NULL;

	if (!RtlPcToFileHeader(addr, &base) || base == NULL)
		return 0;
	{
		HMODULE hmod = (HMODULE)base;
		static char path[MAX_PATH];

		if (GetModuleFileNameA(hmod, path, MAX_PATH) == 0)
			return 0;
		info->dli_fname = path;
		info->dli_fbase = (void *)hmod;
		info->dli_sname = NULL;
		info->dli_saddr = NULL;
		return 1;
	}
}

static inline unsigned long rc_backtrace_win(void **buf,
	unsigned long max)
{
	return (unsigned long)CaptureStackBackTrace(0,
		(ULONG)max, buf, NULL);
}

#else /* POSIX */

#include <pthread.h>
#include <unistd.h>
#include <dlfcn.h>

typedef pthread_mutex_t rc_mutex_t;
#define RC_MUTEX_STATIC_INIT PTHREAD_MUTEX_INITIALIZER

static inline int rc_mutex_init_posix(rc_mutex_t *m)
{
	return pthread_mutex_init(m, NULL);
}

static inline int rc_mutex_lock_posix(rc_mutex_t *m)
{
	return pthread_mutex_lock(m);
}

static inline int rc_mutex_unlock_posix(rc_mutex_t *m)
{
	return pthread_mutex_unlock(m);
}

static inline int rc_mutex_destroy_posix(rc_mutex_t *m)
{
	return pthread_mutex_destroy(m);
}

typedef pthread_key_t rc_tss_t;

static inline int rc_tss_create_posix(rc_tss_t *key,
	void (*dtor)(void *))
{
	return pthread_key_create(key, dtor);
}

static inline void *rc_tss_get_posix(rc_tss_t key)
{
	return pthread_getspecific(key);
}

static inline int rc_tss_set_posix(rc_tss_t key, const void *val)
{
	return pthread_setspecific(key, val);
}

static inline int rc_tss_delete_posix(rc_tss_t key)
{
	return pthread_key_delete(key);
}

typedef struct rc_thread_h {
	pthread_t tid;
} rc_thread_h;

static inline int rc_thread_create_posix(rc_thread_h *t,
	void *(*fn)(void *), void *arg)
{
	return pthread_create(&t->tid, NULL, fn, arg);
}

static inline int rc_thread_join_posix(rc_thread_h *t)
{
	return pthread_join(t->tid, NULL);
}

static inline unsigned long rc_thread_self_posix(void)
{
	return (unsigned long)pthread_self();
}

static inline void rc_monotonic_posix(struct timespec *ts)
{
	clock_gettime(CLOCK_MONOTONIC, ts);
}

static inline int rc_cas_posix(volatile int *ptr, int oldv,
	int newv)
{
	return __sync_bool_compare_and_swap(ptr, oldv, newv);
}

static inline void rc_nanosleep_posix(const struct timespec *ts)
{
	nanosleep(ts, NULL);
}

static inline long rc_getpid_posix(void)
{
	return (long)getpid();
}

typedef Dl_info rc_dl_info_t;

static inline int rc_dladdr_posix(void *addr, rc_dl_info_t *info)
{
	return dladdr(addr, info);
}

#include <sys/syscall.h>

#if defined(__linux__)
#define _GNU_SOURCE
#endif
static inline unsigned long rc_thread_self_tid_posix(void)
{
#if defined(__linux__)
	return (unsigned long)syscall(SYS_gettid);
#elif defined(__APPLE__)
	return (unsigned long)pthread_mach_thread_np(pthread_self());
#elif defined(__FreeBSD__)
	return (unsigned long)pthread_getthreadid_np();
#else
	return 0;
#endif
}

/*
 * musl has no execinfo.h; the caller cache does not use
 * rc_backtrace today, so it degrades to "no frames" there.
 */
#if defined(__GLIBC__) || defined(__APPLE__) || \
	defined(__FreeBSD__) || defined(__NetBSD__) || \
	defined(__OpenBSD__)
#include <execinfo.h>

static inline unsigned long rc_backtrace_posix(void **buf,
	unsigned long max)
{
	return backtrace(buf, (int)max);
}
#else
static inline unsigned long rc_backtrace_posix(void **buf,
	unsigned long max)
{
	(void)buf;
	(void)max;
	return 0;
}
#endif

#endif /* _WIN32 / POSIX */

/*
 * Neutral spellings: one name per capability, mapped to the
 * platform inline above. Consumers use these.
 */
#if defined(_WIN32)
#define rc_getpid() rc_getpid_win()
#define rc_thread_self_tid() rc_thread_self_tid_win()
#define rc_monotonic(ts) rc_monotonic_win(ts)
#define rc_cas(p, o, n) rc_cas_win(p, o, n)
#define rc_nanosleep(ts) rc_nanosleep_win(ts)
#define rc_dladdr(a, i) rc_dladdr_win(a, i)
#define rc_backtrace(b, n) rc_backtrace_win(b, n)
#define rc_thread_self() rc_thread_self_win()
#else
#define rc_getpid() rc_getpid_posix()
#define rc_thread_self_tid() rc_thread_self_tid_posix()
#define rc_monotonic(ts) rc_monotonic_posix(ts)
#define rc_cas(p, o, n) rc_cas_posix(p, o, n)
#define rc_nanosleep(ts) rc_nanosleep_posix(ts)
#define rc_dladdr(a, i) rc_dladdr_posix(a, i)
#define rc_backtrace(b, n) rc_backtrace_posix(b, n)
#define rc_thread_self() rc_thread_self_posix()
#endif

#endif /* RETRACE_CORE_POSIX_COMPAT_H_ */
