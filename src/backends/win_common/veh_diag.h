/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#ifndef RETRACE_WIN_COMMON_VEH_DIAG_H_
#define RETRACE_WIN_COMMON_VEH_DIAG_H_

/*
 * Install the RETRACE_WIN_DIAG-gated fault-site observer (0 when
 * the gate is off, 1 installed, -1 failed). Call EARLY -- before
 * hooks -- so it sees everything that happens after.
 */
int retrace_win_veh_init(void);

#endif /* RETRACE_WIN_COMMON_VEH_DIAG_H_ */
