#ifndef __PTPD_EXPORTS_H
#define __PTPD_EXPORTS_H

#include <stdio.h>
#include <stdlib.h>

#include "ptpd.h"

typedef struct{
	int valid;
	char slave_servo_state[128];
	char sync_source[128];
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
}  ptpdexp_sync_state_t ;

#define PTPDEXP_COMMAND_TRACKING 1
#define PTPDEXP_COMMAND_MAN_ADJUST_PHASE 2

void ptpdexp_get_sync_state(ptpdexp_sync_state_t *state);
void ptpdexp_cmd(int cmd, int value);

#endif
