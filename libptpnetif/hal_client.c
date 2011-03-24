#include <stdio.h>
#include <stdlib.h>

#include "hal_exports.h"

#include <wr_ipc.h>

static wripc_handle_t hal_cli;

int halexp_check_running()
{
	//int res_int;
	//return wripc_call(hal_ipc, "halexp_check_running", ;
	return 0;
}

int halexp_reset_port(const char *port_name)
{
//  TRACE(TRACE_INFO, "resetting port %s\n", port_name);
  return 0;
}

int halexp_calibration_cmd(const char *port_name, int command, int on_off)
{
	int rval;
	wripc_call(hal_cli, "halexp_calibration_cmd", &rval,3, A_STRING(port_name), A_INT32(command), A_INT32(on_off));
	return rval;
}

int halexp_lock_cmd(const char *port_name, int command, int priority)
{
	int rval;
	wripc_call(hal_cli, "halexp_lock_cmd", &rval, 3, A_STRING(port_name), A_INT32(command), A_INT32(priority));
	return rval;
}

int halexp_query_ports(hexp_port_list_t *list)
{
	wripc_call(hal_cli, "halexp_query_ports", list, 0);
	return 0;
}

int halexp_get_port_state(hexp_port_state_t *state, const char *port_name)
{
	wripc_call(hal_cli, "halexp_get_port_state", state, 1, A_STRING(port_name));
	return 0;
}

int halexp_pps_cmd(int cmd, hexp_pps_params_t *params)
{
  int rval;
  wripc_call(hal_cli, "halexp_pps_cmd", &rval, 2, A_INT32(cmd), A_STRUCT(*params));
	return rval;
}

#if 0
int halexp_pll_cmd(int cmd, hexp_pll_cmd_t *params)
{
  int rval;
  wripc_call(hal_cli, "halexp_pll_cmd", &rval, 2, A_INT32(cmd), A_STRUCT(*params));
	return rval;

}
#endif

int halexp_client_init()
{
	hal_cli = wripc_connect(WRSW_HAL_SERVER_ADDR);
	if(hal_cli < 0)
		return -1;
	else
	  return 0;
}
