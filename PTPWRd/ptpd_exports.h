#ifndef __PTPD_EXPORTS_H
#define __PTPD_EXPORTS_H

#include <stdio.h>
#include <stdlib.h>

#include <inttypes.h>

#include "ptpd.h"

typedef struct{
	int valid;
	char slave_servo_state[128];
	char sync_source[128];
	int tracking_enabled;
	double mu;
	double delay_ms;
	double delta_tx_m;
	double delta_rx_m;
	double delta_tx_s;
	double delta_rx_s;
	double fiber_asymmetry;
	double total_asymmetry;
	double cur_offset;
	double cur_setpoint;
	double cur_skew;	
}  ptpdexp_sync_state_t ;

#define PTPDEXP_COMMAND_TRACKING 1
#define PTPDEXP_COMMAND_MAN_ADJUST_PHASE 2

void ptpdexp_get_sync_state(ptpdexp_sync_state_t *state);
void ptpdexp_cmd(int cmd, int value);

#endif