#ifndef __HAL_EXPORTS_H
#define __HAL_EXPORTS_H

//#include <inttypes.h> 

#define HAL_MAX_PORTS 64

#define WRSW_HAL_SERVER_ADDR "wrsw_hal"

// checks if the calibration unit is idle
#define HEXP_CAL_CMD_CHECK_IDLE 1

// enables/disables transmission of calibration pattern
#define HEXP_CAL_CMD_TX_PATTERN 2

// requests a measurement of TX delta
#define HEXP_CAL_CMD_TX_MEASURE 4

// requests a measurement of RX delta
#define HEXP_CAL_CMD_RX_MEASURE 5



#define HEXP_CAL_RESP_BUSY 1
#define HEXP_CAL_RESP_OK 0
#define HEXP_CAL_RESP_ERROR -1

#define HEXP_LOCK_CMD_START 1
#define HEXP_LOCK_CMD_CHECK 2

#define HEXP_LOCK_STATUS_LOCKED 0
#define HEXP_LOCK_STATUS_BUSY 1
#define HEXP_LOCK_STATUS_NONE 2

#define HEXP_PPSG_CMD_GET 0
#define HEXP_PPSG_CMD_ADJUST_PHASE 1
#define HEXP_PPSG_CMD_ADJUST_UTC 2 
#define HEXP_PPSG_CMD_ADJUST_NSEC 3
#define HEXP_PPSG_CMD_POLL 4

#define HEXP_ON 1
#define HEXP_OFF 0

#define HEXP_HPLL 0
#define HEXP_DMPLL 1

#define HEXP_FREQ 0
#define HEXP_PHASE 1

/////////////////added by ML//////////
#define HEXP_EXTSRC_CMD_CHECK 0

#define HEXP_EXTSRC_STATUS_LOCKED 0 
#define HEXP_LOCK_STATUS_BUSY	  1
#define HEXP_EXTSRC_STATUS_NOSRC  2
/////////////////////////////////////

typedef struct {

  char port_name[16];

  uint32_t current_phase_shift;
  int32_t adjust_phase_shift;

  int64_t adjust_utc;
  int32_t adjust_nsec;
 
  uint64_t current_utc;
  uint32_t current_nsec;
 
} hexp_pps_params_t;

/* Port modes (hexp_port_state_t.mode) */
#define HEXP_PORT_MODE_WR_M_AND_S 4
#define HEXP_PORT_MODE_WR_MASTER 1
#define HEXP_PORT_MODE_WR_SLAVE 2
#define HEXP_PORT_MODE_NON_WR 3


#define FIX_ALPHA_FRACBITS 40
/*
#define HEXP_PORT_TSC_RISING 1
#define HEXP_PORT_TSC_FALLING 2
*/

typedef struct {
  /* When non-zero: port state is valid */
  int valid;

  /* WR-PTP role of the port (Master, Slave, etc.) */
  int mode;

  /* TX and RX delays (combined, big Deltas from the link model in the spec) */
  uint32_t delta_tx;
  uint32_t delta_rx;
  
  /* DDMTD raw phase value in picoseconds */
  uint32_t phase_val;

  /* When non-zero: phase_val contains a valid phase readout */
  int phase_val_valid;

  /* When non-zero: link is up */
  int up;

  /* When non-zero: TX path is calibrated (delta_tx contains valid value) */
  int tx_calibrated;

  /* When non-zero: RX path is calibrated (delta_rx contains valid value) */
  int rx_calibrated;
  int tx_tstamp_counter;
  int rx_tstamp_counter;
  int is_locked;
  int lock_priority;

  // timestamp linearization paramaters

  uint32_t phase_setpoint; // DMPLL phase setpoint (picoseconds)

  uint32_t clock_period; // reference lock period in picoseconds
  uint32_t t2_phase_transition; // approximate DMTD phase value (on slave port) at which RX timestamp (T2) counter transistion occurs (picoseconds)

  uint32_t t4_phase_transition; // approximate phase value (on master port) at which RX timestamp (T4) counter transistion occurs (picoseconds)

  uint8_t hw_addr[6];
  int hw_index;
  int32_t fiber_fix_alpha;
} hexp_port_state_t;

typedef struct {
  int num_ports;
  char port_names[HAL_MAX_PORTS][16];
} hexp_port_list_t;

typedef struct {
	int ki, kp;
	int pll;
	int branch;

} hexp_pll_cmd_t;

int halexp_check_running();
int halexp_reset_port(const char *port_name);
int halexp_calibration_cmd(const char *port_name, int command, int on_off);
int halexp_lock_cmd(const char *port_name, int command, int priority);
int halexp_query_ports(hexp_port_list_t *list);
int halexp_get_port_state(hexp_port_state_t *state, const char *port_name);
int halexp_pps_cmd(int cmd, hexp_pps_params_t *params);
int halexp_pll_set_gain(int pll, int branch, int kp, int ki);

int halexp_extsrc_cmd(int command); //added by ML

#endif

