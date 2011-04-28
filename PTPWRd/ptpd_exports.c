//#include <stdio.h>
//#include <stdlib.h>
#include <string.h>

//#include <math.h>


//#include <inttypes.h>
//#include <sys/time.h>

#include <wr_ipc.h>

#include "ptpd.h"
#include "ptpd_exports.h"

extern int servo_state_valid;
extern ptpdexp_sync_state_t cur_servo_state;

void ptpdexp_get_sync_state(ptpdexp_sync_state_t *state)
{
	if(servo_state_valid)
	{
		memcpy(state, &cur_servo_state, sizeof(ptpdexp_sync_state_t));
		state->valid = 1;
	} else
		state->valid = 0;
}

void ptpdexp_cmd(int cmd, int value)
{

	//DBG("GotCMd: %d value %d\n", cmd, value);
	if(cmd == PTPDEXP_COMMAND_TRACKING)
		wr_servo_enable_tracking(value);

	if(cmd == PTPDEXP_COMMAND_MAN_ADJUST_PHASE)
		wr_servo_man_adjust_phase(value);

}

static wripc_handle_t wripc_srv;

void ptpd_init_exports()
{
	wripc_srv = wripc_create_server("ptpd");

	wripc_export(wripc_srv, T_STRUCT(ptpdexp_sync_state_t), "ptpdexp_get_sync_state", ptpdexp_get_sync_state, 0);
	wripc_export(wripc_srv, T_VOID, "ptpdexp_cmd", ptpdexp_cmd, 2, T_INT32, T_INT32);
}

void ptpd_handle_wripc()
{
	wripc_process(wripc_srv);
}
