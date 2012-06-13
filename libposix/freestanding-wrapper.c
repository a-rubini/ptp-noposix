#include<sys/time.h>                                                        \
#include<board.h>
#include<timer.h>
#include<endpoint.h>
#include<pps_gen.h>
#include<minic.h>
#include<softpll_ng.h>
#include<types.h>
#include<errno.h>
#include"../wrsw_hal/hal_exports.h"
#include"ptpd_netif.h"
#include"../PTPWRd/ptpd.h"
#include"../PTPWRd/datatypes.h"

#include "wrapper_private.h"

#define min(x,y) (x<y ? x : y)

//#define netif_mprintf(...)

//static hal_port_state_t port_state;
static struct my_socket wr_sockets[SOCKS_NUM];

/* lets save some memory, we are not threaded so ptpd_netif_recvfrom() and update_rx_queues()
 * can share this buffer*/
static uint8_t pkg[sizeof(uint8_t)+ETH_HEADER_SIZE+MAX_PAYLOAD+sizeof(struct hw_timestamp)];

int usleep(useconds_t useconds)
{
  while(useconds--) asm volatile("nop");

  return 0;
}

uint64_t ptpd_netif_get_msec_tics(void)
{
  return timer_get_tics();
}

int ptpd_netif_calibrating_disable(int txrx, const char *ifaceName)
{
  return PTPD_NETIF_OK;
}

int ptpd_netif_calibrating_enable(int txrx, const char *ifaceName)
{
  return PTPD_NETIF_OK;
}

int ptpd_netif_calibrating_poll(int txrx, const char *ifaceName,
	uint64_t *delta)
{
	uint64_t delta_rx, delta_tx;

	ptpd_netif_read_calibration_data(ifaceName, &delta_tx, &delta_rx, NULL, NULL);
	if(txrx == PTPD_NETIF_TX)
		*delta = delta_tx;
	else
		*delta = delta_rx;

	return PTPD_NETIF_READY;
}

int ptpd_netif_calibration_pattern_enable(const char *ifaceName,
                                          unsigned int calibrationPeriod,
                                          unsigned int calibrationPattern,
                                          unsigned int calibrationPatternLen)
{
  ep_cal_pattern_enable();
  return PTPD_NETIF_OK;
}

int ptpd_netif_calibration_pattern_disable(const char *ifaceName)
{
  ep_cal_pattern_disable();
  return PTPD_NETIF_OK;
}

static int read_phase_val(hexp_port_state_t *state)
{
  int32_t dmtd_phase;
  
  if(spll_read_ptracker(0, &dmtd_phase, NULL))
  {
    state->phase_val = dmtd_phase;
    state->phase_val_valid = 1;
  }
  else
  {
    state->phase_val = 0;
    state->phase_val_valid = 0;
  }

  return 0;
}

int halexp_get_port_state(hexp_port_state_t *state, const char *port_name)
{
  state->valid         = 1;
#ifdef WRPC_MASTER
  state->mode          = HEXP_PORT_MODE_WR_MASTER;
#else
  state->mode          = HEXP_PORT_MODE_WR_SLAVE;
#endif
  ep_get_deltas( &state->delta_tx, &state->delta_rx);
  read_phase_val(state);
  state->up            = ep_link_up(NULL);
  state->tx_calibrated = 1;
  state->rx_calibrated = 1;
  state->is_locked     = spll_check_lock(0);
  state->lock_priority = 0;
  spll_get_phase_shift(0, NULL, &state->phase_setpoint);
  state->clock_period  = 8000;
  state->t2_phase_transition = cal_phase_transition;
  state->t4_phase_transition = cal_phase_transition;
  get_mac_addr(state->hw_addr);
  state->hw_index      = 0;
  state->fiber_fix_alpha = sfp_alpha;
  
  return 0;
}

int ptpd_netif_get_ifName(char *ifname, int number)
{

  strcpy(ifname, "wr0");
  return PTPD_NETIF_OK;
}

int ptpd_netif_get_port_state(const char *ifaceName)
{
  return ep_link_up(NULL) ? PTPD_NETIF_OK : PTPD_NETIF_ERROR;
}

int ptpd_netif_locking_disable(int txrx, const char *ifaceName, int priority)
{
 //softpll_disable();
 return PTPD_NETIF_OK;
}

int ptpd_netif_locking_enable(int txrx, const char *ifaceName, int priority)
{
  spll_init(SPLL_MODE_SLAVE, 0, 1);
  spll_enable_ptracker(0, 1);
  return PTPD_NETIF_OK;
}

int ptpd_netif_locking_poll(int txrx, const char *ifaceName, int priority)
{
  return spll_check_lock(0) ? PTPD_NETIF_READY : PTPD_NETIF_ERROR;
}

int ptpd_netif_read_calibration_data(const char *ifaceName, uint64_t *deltaTx,
    uint64_t *deltaRx, int32_t *fix_alpha, int32_t *clock_period)
{
  hexp_port_state_t state;

  halexp_get_port_state(&state, ifaceName);

  // check if the data is available
  if(state.valid)
  {

    if(fix_alpha)
      *fix_alpha = state.fiber_fix_alpha;

    if(clock_period)
      *clock_period = state.clock_period;

    //check if tx is calibrated,
    // if so read data
    if(state.tx_calibrated)
    {
      if(deltaTx) *deltaTx = state.delta_tx;
    }
    else
      return PTPD_NETIF_NOT_FOUND;

    //check if rx is calibrated,
    // if so read data
    if(state.rx_calibrated)
    {
      if(deltaRx) *deltaRx = state.delta_rx;
    }
    else
      return PTPD_NETIF_NOT_FOUND;

  }
  return PTPD_NETIF_OK;

}

int ptpd_netif_enable_timing_output(int enable)
{
  pps_gen_enable_output(enable);
  return PTPD_NETIF_OK;
}

int ptpd_netif_adjust_in_progress()
{
  return pps_gen_busy() || spll_shifter_busy(0);
}

int ptpd_netif_adjust_counters(int64_t adjust_sec, int32_t adjust_nsec)
{
	if(adjust_sec)
	  pps_gen_adjust(PPSG_ADJUST_SEC, adjust_sec);
	if(adjust_nsec)
	  pps_gen_adjust(PPSG_ADJUST_NSEC, adjust_nsec);
	
	return 0;
}

int ptpd_netif_adjust_phase(int32_t phase_ps)
{
  spll_set_phase_shift(SPLL_ALL_CHANNELS, phase_ps);
}

/*not implemented yet*/
int ptpd_netif_extsrc_detection()
{
  return PTPD_NETIF_OK;
}

int ptpd_netif_get_dmtd_phase(wr_socket_t *sock, int32_t *phase)
{
	if(phase)
		spll_read_ptracker(0, phase, NULL);
}

char* format_wr_timestamp(wr_timestamp_t ts)
{
  static char buf[1];
  buf[0]='\0';
  return buf;
}

int ptpd_netif_enable_phase_tracking(const char *if_name) 
{
  spll_enable_ptracker(0, 1);
  return PTPD_NETIF_OK;
}
