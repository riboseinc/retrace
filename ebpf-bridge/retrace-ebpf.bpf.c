/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * retrace-ebpf: eBPF backend for retrace (TODO.complete/29 MVP).
 *
 * BPF program that hooks the sys_enter_openat and sys_enter_close
 * tracepoints and emits one event per call via bpf_perf_event_output.
 * A Python loader attaches this program and writes the events to
 * stdout in retrace JSON format.
 *
 * Limitations (MVP scope):
 *   - Linux x86-64 only (eBPF kernel surface).
 *   - Two syscalls (openat, close). Add more by following the pattern.
 *   - Args are raw integers. Dereferencing user pointers (e.g. the
 *     `char *pathname` in openat) needs bpf_probe_read_user_str and
 *     per-syscall customization -- left as TODO.
 *   - No filtering, no per-return-address routing. eBPF runs in
 *     kernel context and cannot modify arguments or skip calls;
 *     retrace's mutate-and-redirect model doesn't apply. eBPF is
 *     for OBSERVATION ONLY, not intervention. (See TODO.complete/29
 *     open questions.)
 *
 * Pairs with: ebpf-bridge/retrace-ebpf-loader (Python).
 *
 * Compile with: clang -target bpf -O2 -g -c retrace-ebpf.bpf.c -o retrace-ebpf.bpf.o
 * Run with:     python3 retrace-ebpf-loader ./your-binary
 */

#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

/* One event matches one entry in the retrace JSON array. The
 * Python loader converts to JSON: {"time":..., "func":"openat",
 * "args":[...], "ret_val":...}.
 */
struct event_t {
	__u32 pid;
	__u32 syscall_id;     /* 1=openat, 2=close (matches loader) */
	__u64 args[4];        /* raw integer args */
	__u64 ret_val;
	__u64 ts_ns;          /* ktime_ns */
};

/* Perf event map: BPF -> user-space. */
struct {
	__uint(type, BPF_MAP_TYPE_PERF_EVENT_ARRAY);
	__uint(key_size, sizeof(__u32));
	__uint(value_size, sizeof(__u32));
} events SEC(".maps");

/* Hook sys_enter_openat (syscall #257 on x86-64, but we use the
 * tracepoint by name to be portable across syscall numbers).
 */
SEC("tracepoint/syscalls/sys_enter_openat")
int trace_openat(struct trace_event_raw_openat *ctx)
{
	struct event_t e = {};

	e.pid = bpf_get_current_pid_tgid() >> 32;
	e.syscall_id = 1;
	e.args[0] = (__u64)ctx->dfd;
	e.args[1] = (__u64)ctx->filename;  /* user pointer; loader derefs */
	e.args[2] = (__u64)ctx->flags;
	e.args[3] = (__u64)ctx->mode;
	e.ts_ns = bpf_ktime_get_ns();

	bpf_perf_event_output(ctx, &events, BPF_F_CURRENT_CPU,
	    &e, sizeof(e));
	return 0;
}

SEC("tracepoint/syscalls/sys_enter_close")
int trace_close(struct trace_event_raw_close *ctx)
{
	struct event_t e = {};

	e.pid = bpf_get_current_pid_tgid() >> 32;
	e.syscall_id = 2;
	e.args[0] = (__u64)ctx->fd;
	e.ts_ns = bpf_ktime_get_ns();

	bpf_perf_event_output(ctx, &events, BPF_F_CURRENT_CPU,
	    &e, sizeof(e));
	return 0;
}

char LICENSE[] SEC("license") = "BSD-2-Clause";
