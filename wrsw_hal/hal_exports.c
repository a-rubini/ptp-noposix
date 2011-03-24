#include <stdio.h>
#include <stdlib.h>

#include <hw/trace.h>
#include <hw/dmpll.h>

#include "wrsw_hal.h"
#include "hal_exports.h"

#include <wr_ipc.h>


#define WRSW_HAL_SERVER_ADDR "wrsw_hal"

static wripc_handle_t hal_ipc;

int halexp_check_running()
{
	return 1;
}

int halexp_reset_port(const char *port_name)
{
  TRACE(TRACE_INFO, "resetting port %s\n", port_name);
  return 0;
}



int halexp_lock_cmd(const char *port_name, int command, int priority)
{
	int rval;
	
	//	TRACE(TRACE_INFO,"Command %d", command);
	
	switch(command)
	{
		case HEXP_LOCK_CMD_START:
			return hal_port_start_lock(port_name, priority);

		case HEXP_LOCK_CMD_CHECK:
			rval = hal_port_check_lock(port_name);
			
			if(rval > 0)
				return HEXP_LOCK_STATUS_LOCKED;
			else if (!rval)
				return HEXP_LOCK_STATUS_BUSY;
			else
				return HEXP_LOCK_STATUS_NONE;
			break;
	}
	
	return -100;
}

int halexp_query_port(char *port_name, int id)
{

}

int halexp_pps_cmd(int cmd, hexp_pps_params_t *params)
{


  switch(cmd)
    {
    case HEXP_PPSG_CMD_ADJUST_PHASE:
      shw_dmpll_phase_shift(params->port_name, params->adjust_phase_shift);
      return 0;

    case HEXP_PPSG_CMD_ADJUST_NSEC:
      shw_pps_gen_adjust_nsec(params->adjust_nsec);
      return 0;

    case HEXP_PPSG_CMD_ADJUST_UTC:
      shw_pps_gen_adjust_utc(params->adjust_utc);
      return 0;

    case HEXP_PPSG_CMD_POLL:
      return shw_dmpll_shifter_busy(params->port_name) || shw_pps_gen_busy();
    }
}


static void hal_cleanup_wripc()
{
	wripc_close(hal_ipc);
}

int hal_init_wripc()
{
	hal_ipc = wripc_create_server(WRSW_HAL_SERVER_ADDR);


	if(hal_ipc < 0) 
		return -1;

	wripc_export(hal_ipc, T_INT32, "halexp_pps_cmd", halexp_pps_cmd, 2, T_INT32, T_STRUCT(hexp_pps_params_t));
	wripc_export(hal_ipc, T_INT32, "halexp_check_running", halexp_check_running, 0);
	wripc_export(hal_ipc, T_STRUCT(hexp_port_state_t), "halexp_get_port_state", halexp_get_port_state, 1, T_STRING);
	wripc_export(hal_ipc, T_INT32, "halexp_calibration_cmd", halexp_calibration_cmd, 3, T_STRING, T_INT32, T_INT32);
	wripc_export(hal_ipc, T_INT32, "halexp_lock_cmd", halexp_lock_cmd, 3, T_STRING, T_INT32, T_INT32);
	wripc_export(hal_ipc, T_STRUCT(hexp_port_list_t), "halexp_query_ports", halexp_query_ports, 0);


	hal_add_cleanup_callback(hal_cleanup_wripc);
	
	TRACE(TRACE_INFO, "Started WRIPC server '%s'", WRSW_HAL_SERVER_ADDR);
	
	return 0;
}

int hal_update_wripc()
{
	return wripc_process(hal_ipc);
}


int hal_check_running()
{
	wripc_handle_t fd;

	fd = wripc_connect(WRSW_HAL_SERVER_ADDR);
	
	if(fd >= 0)
	{
		wripc_close(fd);
		return 1;
	}
	return 0;
	
}
