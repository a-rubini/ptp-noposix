#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <minipc.h>

#define HAL_EXPORT_STRUCTURES
#include "hal_exports.h"

#define DEFAULT_TO 200000 /* ms */

static struct minipc_ch *hal_ch;

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
	int ret, rval;
	ret = minipc_call(hal_ch, DEFAULT_TO, &__rpcdef_calibration_cmd,
			  &rval, port_name, command, on_off);
	if (ret < 0)
		return ret;
	return rval;
}

int halexp_lock_cmd(const char *port_name, int command, int priority)
{
	int ret, rval;
	ret = minipc_call(hal_ch, DEFAULT_TO, &__rpcdef_lock_cmd,
			  &rval, port_name, command, priority);
	if (ret < 0)
		return ret;
	return rval;
}

int halexp_query_ports(hexp_port_list_t *list)
{
	int ret;
	ret = minipc_call(hal_ch, DEFAULT_TO, &__rpcdef_query_ports,
			 list /* return val */);
	return ret;
}

int halexp_get_port_state(hexp_port_state_t *state, const char *port_name)
{
	int ret;
	ret = minipc_call(hal_ch, DEFAULT_TO, &__rpcdef_get_port_state,
			 state /* retval */, port_name);

	return ret;
}

int halexp_pps_cmd(int cmd, hexp_pps_params_t *params)
{
	int ret, rval;
	ret = minipc_call(hal_ch, DEFAULT_TO, &__rpcdef_pps_cmd,
			 &rval, cmd, params);
	if (ret < 0)
		return ret;
	return rval;
}

int halexp_get_timing_state(hexp_timing_state_t *tstate)
{
	int ret;
	ret = minipc_call(hal_ch, DEFAULT_TO, &__rpcdef_get_timing_state,
			 tstate);
	if (ret < 0)
		return ret;
	return 0;
}



int halexp_client_try_connect(int retries, int timeout)
{
	
	for(;;) {
		hal_ch = minipc_client_create(WRSW_HAL_SERVER_ADDR, MINIPC_FLAG_VERBOSE);
		if (hal_ch == 0)
			retries--;
		else 
			return 0;
		
		if(!retries)
			return -1;
			
		usleep(timeout);
	}

	return -1;
}


int halexp_client_init()
{
	return halexp_client_try_connect(0, 0);
}