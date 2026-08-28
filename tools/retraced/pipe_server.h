/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#ifndef RETRACE_TOOLS_RETRACED_PIPE_SERVER_H_
#define RETRACE_TOOLS_RETRACED_PIPE_SERVER_H_

/*
 * The Windows daemon (TODO.supervisor/12 P0): the same RTRD
 * framing and state machine over \\.\pipe\retraced-agent (byte
 * mode). One accept loop; one service thread per client; the
 * registry/journal/ctl modules are platform-neutral and shared
 * with the POSIX daemon. The pipe's default ACL (creator-owner
 * only) is the PEERCRED equivalent; the nonce discipline and
 * the spectator role are identical.
 */

int retraced_pipe_main(int argc, char **argv);

#endif /* RETRACE_TOOLS_RETRACED_PIPE_SERVER_H_ */
