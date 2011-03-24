#ifndef __HAL_EXPORTS_H
#define __HAL_EXPORTS_H

#include <inttypes.h>

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


typedef struct {

  char port_name[16];

 uint32_t current_phase_shift;
 int32_t adjust_phase_shift;

 int64_t adjust_utc;
 int32_t adjust_nsec;
 
 uint64_t current_utc;
 uint32_t current_nsec;
 
} hexp_pps_params_t;

#define HEXP_PORT_MODE_WR_MASTER 1
#define HEXP_PORT_MODE_WR_SLAVE 2
#define HEXP_PORT_MODE_NON_WR 3

#define HEXP_PORT_TSC_RISING 1
#define HEXP_PORT_TSC_FALLING 2

typedef struct {
  int valid;
  int mode;
	
  uint64_t delta_tx;
  uint64_t delta_rx;
  uint64_t phase_val;
  int phase_val_valid;

  int up;
  int tx_calibrated;
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
} hexp_port_state_t;

typedef struct {
  int num_ports;
  char port_names[HAL_MAX_PORTS][16];
} hexp_port_list_t;


int halexp_check_running();
int halexp_reset_port(const char *port_name);
int halexp_calibration_cmd(const char *port_name, int command, int on_off);
int halexp_lock_cmd(const char *port_name, int command, int priority);
int halexp_query_ports(hexp_port_list_t *list);
int halexp_get_port_state(hexp_port_state_t *state, const char *port_name);
int halexp_pps_cmd(int cmd, hexp_pps_params_t *params);

#endif
