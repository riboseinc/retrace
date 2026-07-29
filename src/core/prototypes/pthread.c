/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * pthread.h prototypes. Each entry mirrors a WRAPPER_ENTRY_SYSTEM_V
 * line in src/v2/funcs_symbols.S so the engine can parse the call's
 * args for log_params / modify_in_param_*.
 *
 * Conventions:
 *
 *   - Opaque pthread handles (pthread_mutex_t *, pthread_t, etc.) use
 *     {.modifiers = CDM_NOMOD}. log_params prints the address; no
 *     deref (the types are opaque to the user).
 *   - Output args (old value getters, join retval) use PDIR_OUT with
 *     CDM_POINTER so log_params dereferences to show the post-call
 *     value. The pre-call value may be uninitialized; that's fine.
 *   - pthread_t is `unsigned long` on glibc, `void *` on BSD. Use
 *     type_name="long" -- loses no bits, works everywhere.
 *
 * Skip on platforms where pthread cleanup_push / pop are macros that
 * expand to a brace-balance hack (glibc). Wrappers exist but using
 * them via the trampoline is brittle. We still register the
 * prototype for symmetry; users who intercept will see whatever the
 * macro expansion produces.
 */

#include "funcs.h"

retrace_func_define_prototypes(pthread) = {

/* ----------------------------------------------------------------------
 * Mutex
 * ----
 */
	{
		.name = "pthread_mutex_init",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 2,
		.params = {
			{.name = "mutex", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN},
			{.name = "attr", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN}
		}
	},
	{
		.name = "pthread_mutex_destroy",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 1,
		.params = {
			{.name = "mutex", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN}
		}
	},
	{
		.name = "pthread_mutex_lock",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 1,
		.params = {
			{.name = "mutex", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN}
		}
	},
	{
		.name = "pthread_mutex_trylock",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 1,
		.params = {
			{.name = "mutex", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN}
		}
	},
	{
		.name = "pthread_mutex_unlock",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 1,
		.params = {
			{.name = "mutex", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN}
		}
	},
	{
		.name = "pthread_mutex_getprioceiling",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 2,
		.params = {
			{.name = "mutex", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN},
			{.name = "prioceiling", .type_name = "ptr", .modifiers = CDM_POINTER, .ref_type_name = "int", .direction = PDIR_OUT}
		}
	},
	{
		.name = "pthread_mutex_setprioceiling",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 3,
		.params = {
			{.name = "mutex", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN},
			{.name = "prioceiling", .type_name = "int", .modifiers = CDM_NOMOD, .direction = PDIR_IN},
			{.name = "old_ceiling", .type_name = "ptr", .modifiers = CDM_POINTER, .ref_type_name = "int", .direction = PDIR_OUT}
		}
	},

/* ----------------------------------------------------------------------
 * Mutex attributes
 * ----
 */
	{
		.name = "pthread_mutexattr_init",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 1,
		.params = {
			{.name = "attr", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN}
		}
	},
	{
		.name = "pthread_mutexattr_destroy",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 1,
		.params = {
			{.name = "attr", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN}
		}
	},
#define ATTR_GET_INT(fname, field) \
	{ \
		.name = fname, .conv = CC_SYSTEM_V, .type_name = "int", \
		.params_cnt = 2, \
		.params = { \
			{.name = "attr", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN}, \
			{.name = field, .type_name = "ptr", .modifiers = CDM_POINTER, .ref_type_name = "int", .direction = PDIR_OUT} \
		} \
	}
#define ATTR_SET_INT(fname, field) \
	{ \
		.name = fname, .conv = CC_SYSTEM_V, .type_name = "int", \
		.params_cnt = 2, \
		.params = { \
			{.name = "attr", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN}, \
			{.name = field, .type_name = "int", .modifiers = CDM_NOMOD, .direction = PDIR_IN} \
		} \
	}
	ATTR_GET_INT("pthread_mutexattr_getprioceiling", "prioceiling"),
	ATTR_SET_INT("pthread_mutexattr_setprioceiling", "prioceiling"),
	ATTR_GET_INT("pthread_mutexattr_getprotocol",    "protocol"),
	ATTR_SET_INT("pthread_mutexattr_setprotocol",    "protocol"),
	ATTR_GET_INT("pthread_mutexattr_getpshared",     "pshared"),
	ATTR_SET_INT("pthread_mutexattr_setpshared",     "pshared"),
	ATTR_GET_INT("pthread_mutexattr_gettype",        "type"),
	ATTR_SET_INT("pthread_mutexattr_settype",        "type"),

/* ----------------------------------------------------------------------
 * Condition variables
 * ----
 */
	{
		.name = "pthread_cond_init",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 2,
		.params = {
			{.name = "cond", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN},
			{.name = "attr", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN}
		}
	},
	{
		.name = "pthread_cond_destroy",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 1,
		.params = {
			{.name = "cond", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN}
		}
	},
	{
		.name = "pthread_cond_signal",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 1,
		.params = {
			{.name = "cond", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN}
		}
	},
	{
		.name = "pthread_cond_broadcast",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 1,
		.params = {
			{.name = "cond", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN}
		}
	},
	{
		.name = "pthread_cond_wait",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 2,
		.params = {
			{.name = "cond", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN},
			{.name = "mutex", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN}
		}
	},
	{
		.name = "pthread_cond_timedwait",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 3,
		.params = {
			{.name = "cond", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN},
			{.name = "mutex", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN},
			{.name = "abstime", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN}
		}
	},

/* ----------------------------------------------------------------------
 * Condition variable attributes
 * ----
 */
	{
		.name = "pthread_condattr_init",
		.conv = CC_SYSTEM_V, .type_name = "int", .params_cnt = 1,
		.params = {{.name = "attr", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN}}
	},
	{
		.name = "pthread_condattr_destroy",
		.conv = CC_SYSTEM_V, .type_name = "int", .params_cnt = 1,
		.params = {{.name = "attr", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN}}
	},
	ATTR_GET_INT("pthread_condattr_getpshared", "pshared"),
	ATTR_SET_INT("pthread_condattr_setpshared", "pshared"),

/* ----------------------------------------------------------------------
 * Read/write locks
 * ----
 */
	{
		.name = "pthread_rwlock_init",
		.conv = CC_SYSTEM_V, .type_name = "int", .params_cnt = 2,
		.params = {
			{.name = "rwlock", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN},
			{.name = "attr", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN}
		}
	},
	{
		.name = "pthread_rwlock_destroy",
		.conv = CC_SYSTEM_V, .type_name = "int", .params_cnt = 1,
		.params = {{.name = "rwlock", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN}}
	},
	{
		.name = "pthread_rwlock_rdlock",
		.conv = CC_SYSTEM_V, .type_name = "int", .params_cnt = 1,
		.params = {{.name = "rwlock", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN}}
	},
	{
		.name = "pthread_rwlock_tryrdlock",
		.conv = CC_SYSTEM_V, .type_name = "int", .params_cnt = 1,
		.params = {{.name = "rwlock", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN}}
	},
	{
		.name = "pthread_rwlock_wrlock",
		.conv = CC_SYSTEM_V, .type_name = "int", .params_cnt = 1,
		.params = {{.name = "rwlock", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN}}
	},
	{
		.name = "pthread_rwlock_trywrlock",
		.conv = CC_SYSTEM_V, .type_name = "int", .params_cnt = 1,
		.params = {{.name = "rwlock", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN}}
	},
	{
		.name = "pthread_rwlock_unlock",
		.conv = CC_SYSTEM_V, .type_name = "int", .params_cnt = 1,
		.params = {{.name = "rwlock", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN}}
	},
	{
		.name = "pthread_rwlockattr_init",
		.conv = CC_SYSTEM_V, .type_name = "int", .params_cnt = 1,
		.params = {{.name = "attr", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN}}
	},
	{
		.name = "pthread_rwlockattr_destroy",
		.conv = CC_SYSTEM_V, .type_name = "int", .params_cnt = 1,
		.params = {{.name = "attr", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN}}
	},
	ATTR_GET_INT("pthread_rwlockattr_getpshared", "pshared"),
	ATTR_SET_INT("pthread_rwlockattr_setpshared", "pshared"),

/* ----------------------------------------------------------------------
 * Threads
 * ----
 */
	{
		.name = "pthread_create",
		.conv = CC_SYSTEM_V, .type_name = "int", .params_cnt = 4,
		.params = {
			{.name = "thread", .type_name = "ptr", .modifiers = CDM_POINTER, .ref_type_name = "long", .direction = PDIR_OUT},
			{.name = "attr", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN},
			{.name = "start_routine", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN},
			{.name = "arg", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN}
		}
	},
	{
		.name = "pthread_join",
		.conv = CC_SYSTEM_V, .type_name = "int", .params_cnt = 2,
		.params = {
			{.name = "thread", .type_name = "long", .modifiers = CDM_NOMOD, .direction = PDIR_IN},
			{.name = "retval", .type_name = "ptr", .modifiers = CDM_POINTER, .ref_type_name = "ptr", .direction = PDIR_OUT}
		}
	},
	{
		.name = "pthread_detach",
		.conv = CC_SYSTEM_V, .type_name = "int", .params_cnt = 1,
		.params = {{.name = "thread", .type_name = "long", .modifiers = CDM_NOMOD, .direction = PDIR_IN}}
	},
	{
		.name = "pthread_exit",
		.conv = CC_SYSTEM_V, .type_name = "void", .params_cnt = 1,
		.params = {{.name = "retval", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN}}
	},
	{
		.name = "pthread_self",
		.conv = CC_SYSTEM_V, .type_name = "long", .params_cnt = 0,
		.params = {{.name = "", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN}}
	},
	{
		.name = "pthread_equal",
		.conv = CC_SYSTEM_V, .type_name = "int", .params_cnt = 2,
		.params = {
			{.name = "t1", .type_name = "long", .modifiers = CDM_NOMOD, .direction = PDIR_IN},
			{.name = "t2", .type_name = "long", .modifiers = CDM_NOMOD, .direction = PDIR_IN}
		}
	},
	{
		.name = "pthread_cancel",
		.conv = CC_SYSTEM_V, .type_name = "int", .params_cnt = 1,
		.params = {{.name = "thread", .type_name = "long", .modifiers = CDM_NOMOD, .direction = PDIR_IN}}
	},
	{
		.name = "pthread_setcancelstate",
		.conv = CC_SYSTEM_V, .type_name = "int", .params_cnt = 2,
		.params = {
			{.name = "state", .type_name = "int", .modifiers = CDM_NOMOD, .direction = PDIR_IN},
			{.name = "oldstate", .type_name = "ptr", .modifiers = CDM_POINTER, .ref_type_name = "int", .direction = PDIR_OUT}
		}
	},
	{
		.name = "pthread_setcanceltype",
		.conv = CC_SYSTEM_V, .type_name = "int", .params_cnt = 2,
		.params = {
			{.name = "type", .type_name = "int", .modifiers = CDM_NOMOD, .direction = PDIR_IN},
			{.name = "oldtype", .type_name = "ptr", .modifiers = CDM_POINTER, .ref_type_name = "int", .direction = PDIR_OUT}
		}
	},
	{
		.name = "pthread_testcancel",
		.conv = CC_SYSTEM_V, .type_name = "void", .params_cnt = 0,
		.params = {{.name = "", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN}}
	},

/* ----------------------------------------------------------------------
 * Thread attributes
 * ----
 */
	{
		.name = "pthread_attr_init",
		.conv = CC_SYSTEM_V, .type_name = "int", .params_cnt = 1,
		.params = {{.name = "attr", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN}}
	},
	{
		.name = "pthread_attr_destroy",
		.conv = CC_SYSTEM_V, .type_name = "int", .params_cnt = 1,
		.params = {{.name = "attr", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN}}
	},
	ATTR_GET_INT("pthread_attr_getdetachstate",  "detachstate"),
	ATTR_SET_INT("pthread_attr_setdetachstate",  "detachstate"),
	{
		.name = "pthread_attr_getguardsize",
		.conv = CC_SYSTEM_V, .type_name = "int", .params_cnt = 2,
		.params = {
			{.name = "attr", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN},
			{.name = "guardsize", .type_name = "ptr", .modifiers = CDM_POINTER, .ref_type_name = "size_t", .direction = PDIR_OUT}
		}
	},
	{
		.name = "pthread_attr_setguardsize",
		.conv = CC_SYSTEM_V, .type_name = "int", .params_cnt = 2,
		.params = {
			{.name = "attr", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN},
			{.name = "guardsize", .type_name = "size_t", .modifiers = CDM_NOMOD, .direction = PDIR_IN}
		}
	},
	ATTR_GET_INT("pthread_attr_getinheritsched", "inheritsched"),
	ATTR_SET_INT("pthread_attr_setinheritsched", "inheritsched"),
	ATTR_GET_INT("pthread_attr_getschedpolicy",  "schedpolicy"),
	ATTR_SET_INT("pthread_attr_setschedpolicy",  "schedpolicy"),
	ATTR_GET_INT("pthread_attr_getscope",        "scope"),
	ATTR_SET_INT("pthread_attr_setscope",        "scope"),
	{
		.name = "pthread_attr_getstackaddr",
		.conv = CC_SYSTEM_V, .type_name = "int", .params_cnt = 2,
		.params = {
			{.name = "attr", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN},
			{.name = "stackaddr", .type_name = "ptr", .modifiers = CDM_POINTER, .ref_type_name = "ptr", .direction = PDIR_OUT}
		}
	},
	{
		.name = "pthread_attr_setstackaddr",
		.conv = CC_SYSTEM_V, .type_name = "int", .params_cnt = 2,
		.params = {
			{.name = "attr", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN},
			{.name = "stackaddr", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN}
		}
	},
	{
		.name = "pthread_attr_getstacksize",
		.conv = CC_SYSTEM_V, .type_name = "int", .params_cnt = 2,
		.params = {
			{.name = "attr", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN},
			{.name = "stacksize", .type_name = "ptr", .modifiers = CDM_POINTER, .ref_type_name = "size_t", .direction = PDIR_OUT}
		}
	},
	{
		.name = "pthread_attr_setstacksize",
		.conv = CC_SYSTEM_V, .type_name = "int", .params_cnt = 2,
		.params = {
			{.name = "attr", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN},
			{.name = "stacksize", .type_name = "size_t", .modifiers = CDM_NOMOD, .direction = PDIR_IN}
		}
	},

/* ----------------------------------------------------------------------
 * Once / scheduling / concurrency
 * ----
 */
	{
		.name = "pthread_once",
		.conv = CC_SYSTEM_V, .type_name = "int", .params_cnt = 2,
		.params = {
			{.name = "once", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN},
			{.name = "init_routine", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN}
		}
	},
	{
		.name = "pthread_getschedparam",
		.conv = CC_SYSTEM_V, .type_name = "int", .params_cnt = 3,
		.params = {
			{.name = "thread", .type_name = "long", .modifiers = CDM_NOMOD, .direction = PDIR_IN},
			{.name = "policy", .type_name = "ptr", .modifiers = CDM_POINTER, .ref_type_name = "int", .direction = PDIR_OUT},
			{.name = "param", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN}
		}
	},
	{
		.name = "pthread_setschedparam",
		.conv = CC_SYSTEM_V, .type_name = "int", .params_cnt = 3,
		.params = {
			{.name = "thread", .type_name = "long", .modifiers = CDM_NOMOD, .direction = PDIR_IN},
			{.name = "policy", .type_name = "int", .modifiers = CDM_NOMOD, .direction = PDIR_IN},
			{.name = "param", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN}
		}
	},
	{
		.name = "pthread_getconcurrency",
		.conv = CC_SYSTEM_V, .type_name = "int", .params_cnt = 0,
		.params = {{.name = "", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN}}
	},
	{
		.name = "pthread_setconcurrency",
		.conv = CC_SYSTEM_V, .type_name = "int", .params_cnt = 1,
		.params = {{.name = "level", .type_name = "int", .modifiers = CDM_NOMOD, .direction = PDIR_IN}}
	},

/* ----------------------------------------------------------------------
 * Thread-specific data (keys)
 * ----
 */
	{
		.name = "pthread_key_create",
		.conv = CC_SYSTEM_V, .type_name = "int", .params_cnt = 2,
		.params = {
			{.name = "key", .type_name = "ptr", .modifiers = CDM_POINTER, .ref_type_name = "int", .direction = PDIR_OUT},
			{.name = "destructor", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN}
		}
	},
	{
		.name = "pthread_key_delete",
		.conv = CC_SYSTEM_V, .type_name = "int", .params_cnt = 1,
		.params = {{.name = "key", .type_name = "int", .modifiers = CDM_NOMOD, .direction = PDIR_IN}}
	},
	{
		.name = "pthread_setspecific",
		.conv = CC_SYSTEM_V, .type_name = "int", .params_cnt = 2,
		.params = {
			{.name = "key", .type_name = "int", .modifiers = CDM_NOMOD, .direction = PDIR_IN},
			{.name = "value", .type_name = "ptr", .modifiers = CDM_NOMOD, .direction = PDIR_IN}
		}
	},
	{
		.name = "pthread_getspecific",
		.conv = CC_SYSTEM_V, .type_name = "ptr", .params_cnt = 1,
		.params = {{.name = "key", .type_name = "int", .modifiers = CDM_NOMOD, .direction = PDIR_IN}}
	}
};

#undef ATTR_GET_INT
#undef ATTR_SET_INT
