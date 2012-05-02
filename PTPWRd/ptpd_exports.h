#ifndef __PTPD_EXPORTS_H
#define __PTPD_EXPORTS_H

#include <stdio.h>
#include <stdlib.h>

#include "ptpd.h"

typedef struct{
	int valid;
	char slave_servo_state[32];
	char sync_source[32];
	int tracking_enabled;
	int64_t mu;
	int64_t delay_ms;
	int64_t delta_tx_m;
	int64_t delta_rx_m;
	int64_t delta_tx_s;
	int64_t delta_rx_s;
	int64_t fiber_asymmetry;
	int64_t total_asymmetry;
	int64_t cur_offset;
	int64_t cur_setpoint;
	int64_t cur_skew;
	int64_t update_count;
}  ptpdexp_sync_state_t ;

#define PTPDEXP_COMMAND_TRACKING 1
#define PTPDEXP_COMMAND_MAN_ADJUST_PHASE 2

extern int ptpdexp_get_sync_state(ptpdexp_sync_state_t *state);
extern int ptpdexp_cmd(int cmd, int value);

/* Export structures, shared by server and client for argument matching */
#ifdef PTP_EXPORT_STRUCTURES

//int ptpdexp_get_sync_state(ptpdexp_sync_state_t *state);
struct minipc_pd __rpcdef_get_sync_state = {
	.name = "get_sync_state",
	.retval = MINIPC_ARG_ENCODE(MINIPC_ATYPE_STRUCT, ptpdexp_sync_state_t),
	.args = {
		MINIPC_ARG_END,
	},
};

//int ptpdexp_cmd(int cmd, int value);
struct minipc_pd __rpcdef_cmd = {
	.name = "cmd",
	.retval = MINIPC_ARG_ENCODE(MINIPC_ATYPE_INT, int),
	.args = {
		MINIPC_ARG_ENCODE(MINIPC_ATYPE_INT, int),
		MINIPC_ARG_ENCODE(MINIPC_ATYPE_INT, int),
		MINIPC_ARG_END,
	},
};

#endif /* PTP_EXPORT_STRUCTURES */

#endif
