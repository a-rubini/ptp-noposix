#ifdef __STDC_HOSTED__

#include <string.h>
#include <minipc.h>

#include "ptpd.h"

#define PTP_EXPORT_STRUCTURES
#include "ptpd_exports.h"

extern int servo_state_valid;
extern ptpdexp_sync_state_t cur_servo_state;

int ptpdexp_get_sync_state(ptpdexp_sync_state_t *state)
{
	
	fprintf(stderr," GSS: valid %d\n", servo_state_valid);
	if(servo_state_valid)
	{
		memcpy(state, &cur_servo_state, sizeof(ptpdexp_sync_state_t));
		state->valid = 1;
	} else
		state->valid = 0;
	return 0;
}

int ptpdexp_cmd(int cmd, int value)
{

	if(cmd == PTPDEXP_COMMAND_TRACKING)
		wr_servo_enable_tracking(value);

	if(cmd == PTPDEXP_COMMAND_MAN_ADJUST_PHASE)
		wr_servo_man_adjust_phase(value);
	return 0;

}

/* Two functions to manage packet/args conversions */
static int export_get_sync_state(const struct minipc_pd *pd,
				 uint32_t *args, void *ret)
{
	ptpdexp_sync_state_t state;

	ptpdexp_get_sync_state(&state);
	
	*(ptpdexp_sync_state_t *)ret = state;
	return 0;

}

static int export_cmd(const struct minipc_pd *pd,
				 uint32_t *args, void *ret)
{
	int i;

        i = ptpdexp_cmd(args[0], args[1]);
        *(int *)ret = i;
        return 0;
}

static struct minipc_ch *ptp_ch;

void ptpd_init_exports(void)
{
	ptp_ch = minipc_server_create("ptpd", 0);

	__rpcdef_get_sync_state.f = export_get_sync_state;
	__rpcdef_cmd.f = export_cmd;

	minipc_export(ptp_ch, &__rpcdef_get_sync_state);
	minipc_export(ptp_ch, &__rpcdef_cmd);
}

void ptpd_handle_wripc()
{
		minipc_server_action(ptp_ch, 10 /* ms */);
}

#endif
