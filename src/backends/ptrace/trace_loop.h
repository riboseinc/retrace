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

#ifndef RETRACE_BACKENDS_PTRACE_TRACE_LOOP_H
#define RETRACE_BACKENDS_PTRACE_TRACE_LOOP_H

#include <sys/types.h>
#include <retrace/backend.h> /* for struct retrace_engine forward decl */

/*
 * Run the ptrace event loop against `child_pid` until the child exits.
 * Called by the ptrace backend's spawn()/attach() once the tracee is
 * set up. Returns the child's exit status (as waitpid would), or -1 on
 * ptrace error.
 *
 * `eng` is forwarded to retrace_engine_wrapper on each syscall entry.
 */
int retrace_ptrace_trace_loop(struct retrace_engine *eng, pid_t child_pid);

#endif /* RETRACE_BACKENDS_PTRACE_TRACE_LOOP_H */
