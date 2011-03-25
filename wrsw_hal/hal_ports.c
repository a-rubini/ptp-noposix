#include <stdio.h>
#include <stdlib.h>
//#include <string.h>

#include <wr_ipc.h>

#include <inttypes.h>
#include <hw/trace.h>
#include <hw/hpll.h>
#include <hw/dmpll.h>
#include <hw/phy_calibration.h>
#include <hw/pio.h>
#include <hw/pio_pins.h>
#include <hw/fpga_regs.h>
#include <hw/endpoint_regs.h>
#include <hw/watchdog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>

#include "wrsw_hal.h"
#include "hal_exports.h"

#define MAX_PORTS 64

#define HAL_PORT_MODE_WR_UPLINK 1
#define HAL_PORT_MODE_WR_DOWNLINK 2
#define HAL_PORT_MODE_NON_WR 3

#define HAL_PORT_STATE_LINK_DOWN 1
#define HAL_PORT_STATE_UP 2
#define HAL_PORT_STATE_CALIBRATION 3
#define HAL_PORT_STATE_LOCKING 4

#define LOCK_STATE_NONE 0
#define LOCK_STATE_WAIT_HPLL 1
#define LOCK_STATE_WAIT_DMPLL 2
#define LOCK_STATE_LOCKED 3
#define LOCK_STATE_START 4

typedef struct {
  uint32_t phy_rx_bias;
  uint32_t phy_rx_min;
  uint32_t phy_rx_range;

  uint32_t phy_tx_bias;
  uint32_t phy_tx_min;
  uint32_t phy_tx_range;
	
  uint32_t delta_tx_phy;
  uint32_t delta_rx_phy;

  uint32_t delta_tx_sfp;
  uint32_t delta_rx_sfp;

  uint32_t delta_tx_board;
  uint32_t delta_rx_board;

	
  uint32_t delta_tx; // sum of the above
  uint32_t delta_rx; // sum of the above

} hal_port_calibration_t;

typedef struct {
  int in_use;
  
  char name[16];
  uint8_t hw_addr[6];
  int hw_index;
  
  int fd;
  int hw_addr_auto;
  int mode;
  int state;
  int index;
  int calibrated_rx;
  int calibrated_tx;
  int locked;
  
  hal_port_calibration_t calib;
  
  uint32_t phase_val;
  int phase_val_valid;
  
  int lock_state;
  int tx_cal_pending;
  int rx_cal_pending;
	
	int led_index;

	uint32_t ep_base;

} hal_port_state_t;

#define LED_DOWN_MASK 0x00
#define LED_UP_MASK 0x80

static hal_port_state_t ports[MAX_PORTS];

static int fd_raw;

// FIXME: MAC addresses should be kept in some EEPROM
static int get_mac_address(const char *if_name, uint8_t *mac_addr)
{
	struct ifreq ifr;
	int idx;
	int uniq_num;
	
	idx = (if_name[2] == 'u' ? 32 : 0) + (if_name[3] - '0');
	
	
	ptpd_wrap_strncpy(ifr.ifr_name, "eth0", sizeof(ifr.ifr_name));
	
	if(ioctl(fd_raw, SIOCGIFHWADDR, &ifr) < 0)
		return -1;	
	
	uniq_num = (int)ifr.ifr_hwaddr.sa_data[4] * 256 + (int)ifr.ifr_hwaddr.sa_data[5] + idx;
	
	mac_addr[0] = 0x2; // locally administered MAC
	mac_addr[1] = 0x4a;
	mac_addr[2] = 0xbb;
	mac_addr[3] = (uniq_num >> 16) & 0xff;
	mac_addr[4] = (uniq_num >> 8) & 0xff;
	mac_addr[5] = uniq_num & 0xff;

	return 0; 
}

static void reset_port_state(hal_port_state_t *p)
{
  
	p->calibrated_rx = 0;
	p->calibrated_tx = 0;
	p->locked = 0;
	p->state = HAL_PORT_STATE_LINK_DOWN;
	p->lock_state = LOCK_STATE_NONE;
	p->tx_cal_pending = 0;
	p->rx_cal_pending = 0;

}

uint32_t hal_port_get_param(const char *port_name, const char *param_name, uint32_t def_value)
{
  char str[1024];
  uint32_t val;
  snprintf(str, sizeof(str), "ports.%s.%s", port_name, param_name);
  
  if(!hal_config_get_int(str, &val))
    return val;
  
  return def_value;
}

static int check_port_presence(const char *if_name)
{
	struct ifreq ifr;

	ptpd_wrap_strncpy(ifr.ifr_name, if_name, sizeof(ifr.ifr_name));
	
	if(ioctl(fd_raw, SIOCGIFHWADDR, &ifr) < 0)
		return -1;	

	return 0;
}

int hal_init_port(const char *name, int index)
{
	char key_name[128];
	char val[128];
	char cmd[128];
	int fd;
	hal_port_state_t *p = &ports[index];

	uint8_t mac_addr[6];

	if(check_port_presence(name) < 0)
	{
		reset_port_state(p);
		p->in_use = 0;	
		return 0;
	}


	reset_port_state(p);
	p->in_use = 1;	
	
	get_mac_address(name, mac_addr);

	TRACE(TRACE_INFO,"Initializing port '%s' [%02x:%02x:%02x:%02x:%02x:%02x]", name, mac_addr[0],  mac_addr[1],  mac_addr[2],  mac_addr[3],  mac_addr[4],  mac_addr[5] );
	
	ptpd_wrap_strncpy(p->name, name, 16);
	memcpy(p->hw_addr, mac_addr, 6);
	
	snprintf(cmd, sizeof(cmd), "/sbin/ifconfig %s hw ether %02x:%02x:%02x:%02x:%02x:%02x", name, mac_addr[0],  mac_addr[1],  mac_addr[2],  mac_addr[3],  mac_addr[4],  mac_addr[5] );
	system(cmd);
	
	snprintf(cmd, sizeof(cmd), "/sbin/ifconfig %s up", name);
	system(cmd);
	
	p->calib.phy_rx_bias = hal_port_get_param(name, "phy_rx_bias", 7800);
	p->calib.phy_rx_min = hal_port_get_param(name, "phy_rx_min", 18*800);
	p->calib.phy_rx_range = hal_port_get_param(name, "phy_rx_range", 7*800);

	p->calib.phy_tx_bias = hal_port_get_param(name, "phy_tx_bias", 7800);
	p->calib.phy_tx_min = hal_port_get_param(name, "phy_tx_min", 18*800);
	p->calib.phy_tx_range = hal_port_get_param(name, "phy_tx_range", 7*800);

	p->calib.delta_tx_sfp = hal_port_get_param(name, "delta_tx_sfp", 0);
	p->calib.delta_rx_sfp = hal_port_get_param(name, "delta_rx_sfp", 0);

	p->calib.delta_tx_board = hal_port_get_param(name, "delta_tx_board", 0);
	p->calib.delta_rx_board = hal_port_get_param(name, "delta_rx_board", 0);
	
	p->led_index = (int) (p->name[3] - '0');
	if(p->name[2] == 'u') 
		p->led_index |= LED_UP_MASK;

	

	if(p->name[2] == 'd')
	{
		p->hw_index = 2+(p->name[3]-'0');
	} else {
		p->hw_index = 0+(p->name[3]-'0');
	}
	
	p->ep_base = FPGA_BASE_EP_UP0 + 0x10000  *  p->hw_index;
	
	TRACE(TRACE_INFO,"port %s regs 0x%08x\n", p->name, p->ep_base);

	snprintf(key_name, sizeof(key_name),  "ports.%s.mode", p->name);
	
	if(!hal_config_get_string(key_name, val, sizeof(val)))
	{
		if(!strcasecmp(val, "wr_master"))
			p->mode = HEXP_PORT_MODE_WR_MASTER;
		else if(!strcasecmp(val, "wr_slave"))
			p->mode = HEXP_PORT_MODE_WR_SLAVE;
		else if(!strcasecmp(val, "non_wr"))
			p->mode = HEXP_PORT_MODE_NON_WR;
		else {
			TRACE(TRACE_ERROR,"Invalid mode specified for port %s. Defaulting to Non-WR", p->name);
			p->mode = HEXP_PORT_MODE_NON_WR;
		}
			
		
		TRACE(TRACE_INFO,"port %s mode %s", p->name, val);
	} else {	
		p->mode = HEXP_PORT_MODE_NON_WR;
	}
	
	return 0;	
}

int hal_init_ports()
{

	int index = 0;
	char port_name[128];
	
	TRACE(TRACE_INFO, "Initializing switch ports");

	fd_raw = socket(AF_PACKET, SOCK_DGRAM, 0);
	if(fd_raw < 0) return -1;

	memset(ports, 0, sizeof(ports));
	
  for(;;)
  {
    if(!hal_config_iterate("ports", index++, port_name, sizeof(port_name))) break;
		hal_init_port(port_name, index);
	}
	return 0;
}

static int check_link_up(const char *if_name)
{
	struct ifreq ifr;
	
	ptpd_wrap_strncpy(ifr.ifr_name, if_name, sizeof(ifr.ifr_name));

  if(ioctl(fd_raw, SIOCGIFFLAGS, &ifr) > 0) return -1;
  
  return (ifr.ifr_flags & IFF_UP && ifr.ifr_flags & IFF_RUNNING);
}



static void port_locking_fsm(hal_port_state_t *p)
{
	switch(p->lock_state)
	{
		case LOCK_STATE_NONE:
			return;
			
		case LOCK_STATE_START:
			shw_hpll_switch_reference(p->name);
			p->lock_state = LOCK_STATE_WAIT_HPLL;
		  break;

		case LOCK_STATE_WAIT_HPLL:
			if(shw_hpll_check_lock())
			{
			  TRACE(TRACE_INFO, "HPLL locked to port: %s", p->name);
				shw_dmpll_switch_reference(p->name);
				p->lock_state = LOCK_STATE_WAIT_DMPLL;
			} 
			break;
		case LOCK_STATE_WAIT_DMPLL:
			
			
			if(!shw_hpll_check_lock())
			{
				p->lock_state = LOCK_STATE_NONE;
			} else if(shw_dmpll_check_lock())
			{
			  TRACE(TRACE_INFO, "DMPLL locked to port: %s", p->name);
				p->lock_state = LOCK_STATE_LOCKED;
			}
			
			break;
			
		case LOCK_STATE_LOCKED:
			if(!shw_hpll_check_lock() || !shw_dmpll_check_lock())
			{
				TRACE(TRACE_ERROR, "One of PLLs got de-locked");			
				p->lock_state = LOCK_STATE_NONE;
			}

			p->locked = 1;

			break;
	}
}

static void poll_dmtd(hal_port_state_t *p)
{
  uint32_t phase_val;
  
  if(shw_poll_dmtd(p->name, &phase_val) > 0)
    {
      p->phase_val = phase_val;
      p->phase_val_valid = 1;
    } else {
    p->phase_val_valid = 0;
  };
}

uint32_t fix_phy_delay(int32_t delay, int32_t bias, int32_t min_val, int32_t range)
{
      int32_t brange = bias - range;
      TRACE(TRACE_INFO,"dly %d bias %d range %d brange %d", delay, bias, range,brange);
      
    if(bias + range > 8000)
    {
      if(brange < 0) brange += 8000;

      if(delay > bias || (delay < bias && delay > brange))
	{
	  return delay - bias;// + min_val;
	}  {
	return delay + (8000-bias);// + min_val;
      }
    } else {
      return delay - bias;// + min_val;
    }
  return delay;
}


static void calibration_fsm(hal_port_state_t *p)
{
  if(p->name[2] == 'd')
  {
//  	TRACE(TRACE_INFO,"bypassing calibration for port %s", p->name);
  	p->calibrated_tx = 1;
  	p->calibrated_rx = 1;
	  p->calib.delta_rx_phy = 0;
	  p->calib.delta_tx_phy = 0;
	  p->tx_cal_pending = 0;
	  p->rx_cal_pending = 0;
	  
	  return;
  }
  
  if(p->tx_cal_pending || p->rx_cal_pending)
    {
      uint32_t phase;
      if(shw_cal_measure(&phase) > 0)
	{

	  if(p->tx_cal_pending) 
	    {
	      p->calibrated_tx =1;
	      p->calib.delta_tx_phy = fix_phy_delay(phase, p->calib.phy_tx_bias, p->calib.phy_tx_min, p->calib.phy_tx_range);
	      TRACE(TRACE_INFO, "TXCal: raw %d fixed %d\n", phase,    p->calib.delta_tx_phy);

	      p->tx_cal_pending = 0;// TX DMTD is connected in the opposite direction - it measures input - refclk value, so we need to invert it.
	    } else { 
	      p->calibrated_rx =1;
	      p->calib.delta_rx_phy = fix_phy_delay(8000-phase, p->calib.phy_rx_bias, p->calib.phy_rx_min, p->calib.phy_rx_range); 

	      TRACE(TRACE_INFO, "RXCal: raw %d fixed %d\n", phase,    p->calib.delta_rx_phy);

	      p->rx_cal_pending = 0;
	  }
	}
    }
}

static int update_port_leds(hal_port_state_t *p)
{
	uint32_t dsr = _fpga_readl(p->ep_base + EP_REG_DSR);


	
	if(p->led_index & LED_UP_MASK)
	{
		int i = p->led_index & 0xf;
		
		if(i==1)
			shw_pio_set(&PIN_fled4[0],  dsr & EP_DSR_LSTATUS ? 0 : 1);
		else
			shw_pio_set(&PIN_fled0[0],  dsr & EP_DSR_LSTATUS ? 0 : 1);
			
		// uplinks....
	} else {
//			printf("li %d\n", p->led_index);
			shw_mbl_set_leds(p->led_index, 0, dsr & EP_DSR_LSTATUS ? MBL_LED_ON : MBL_LED_OFF);
	}
}

static void port_fsm(hal_port_state_t *p)
{
	int link_up = check_link_up(p->name);
	
	update_port_leds(p);

	if(!link_up && p->state != HAL_PORT_STATE_LINK_DOWN)
	{
		TRACE(TRACE_INFO, "%s: link down", p->name);
		p->state = HAL_PORT_STATE_LINK_DOWN;
		reset_port_state(p);
		p->calibrated_tx = 1;
		return;
	}

	port_locking_fsm(p);

	switch(p->state)
	  {
	  case HAL_PORT_STATE_LINK_DOWN:
	    {
	      if(link_up)
		{
		  TRACE(TRACE_INFO, "%s: link up", p->name);
		  p->state = HAL_PORT_STATE_UP;
		}
	      break;
	    }
			
	  case HAL_PORT_STATE_UP:
	    	    poll_dmtd(p);
			
	    break;
			
	  case HAL_PORT_STATE_LOCKING:

	    if(p->locked) 
	      {
		TRACE(TRACE_INFO,"[main-fsm] Port %s locked.", p->name);
	      p->state = HAL_PORT_STATE_UP;
	      }

	    break;

	  case HAL_PORT_STATE_CALIBRATION:
	    calibration_fsm(p);


			
	    break;
	  }
}

void hal_update_ports()
{
	int i;
	for(i=0; i<MAX_PORTS;i++)
	 if(ports[i].in_use)
  	 port_fsm(&ports[i]);
}

static hal_port_state_t *lookup_port(const char *name)
{
	int i;
	for(i = 0; i< MAX_PORTS;i++)
		if(ports[i].in_use && !strcmp(name, ports[i].name))
		  return &ports[i];

	return NULL;
}

int hal_port_start_lock(const char  *port_name, int priority)
{
	hal_port_state_t *p = lookup_port(port_name);

	if(!p) return PORT_ERROR;

	if(p->state != HAL_PORT_STATE_UP)
	  return PORT_BUSY;
		
	p->state = HAL_PORT_STATE_LOCKING;

	p->lock_state = LOCK_STATE_START;

  TRACE(TRACE_INFO, "Locking to port: %s", port_name);

	return PORT_OK;
}


int hal_port_check_lock(const char  *port_name)
{
	hal_port_state_t *p = lookup_port(port_name);

	if(!p) return PORT_ERROR;
	if(p->lock_state == LOCK_STATE_LOCKED)
		return 1;
		
	return 0;
}


int halexp_get_port_state(hexp_port_state_t *state, const char *port_name)
{
	hal_port_state_t *p = lookup_port(port_name);
	if(!p)
	  return -1;
	
	
	state->valid = 1;
	state->mode = p->mode;
	state->up = p->state != HAL_PORT_STATE_LINK_DOWN;
	state->is_locked = p->lock_state == LOCK_STATE_LOCKED;
	state->phase_val = p->phase_val;
	state->phase_val_valid = p->phase_val_valid;

	state->tx_calibrated = p->calibrated_tx;
	state->rx_calibrated = p->calibrated_rx;

	state->delta_tx = p->calib.delta_tx_phy + p->calib.delta_tx_sfp + p->calib.delta_tx_board;
	state->delta_rx = p->calib.delta_rx_phy + p->calib.delta_rx_sfp + p->calib.delta_rx_board;

	state->t2_phase_transition = 1400;
	state->t4_phase_transition = 1400;
	state->clock_period = 8000;
	memcpy(state->hw_addr, p->hw_addr, 6);
	state->hw_index = p->hw_index;
	//	TRACE(TRACE_INFO,"GetPortSate: phase %lld\n", p->phase_val);

	return 0;
}


static int any_port_calibrating()
{
  int i;
  for(i=0; i<MAX_PORTS;i++)
    if(ports[i].state == HAL_PORT_STATE_CALIBRATION && ports[i].in_use)
      return 1;

  return 0;
}

int halexp_calibration_cmd(const char *port_name, int command, int on_off)
{
	hal_port_state_t *p = lookup_port(port_name);


//	TRACE(TRACE_INFO,"cal_cmd port %s cmn %d\n", port_name, command);
	if(!p)
	  return -1;

	switch(command)
	  {
	  case HEXP_CAL_CMD_CHECK_IDLE: 
	    return !any_port_calibrating() && p->state == HAL_PORT_STATE_UP ? HEXP_CAL_RESP_OK : HEXP_CAL_RESP_BUSY;

	  case HEXP_CAL_CMD_TX_PATTERN: 
	    TRACE(TRACE_INFO, "TXPattern %s, port %s", on_off ? "ON" : "OFF", port_name);

	    // FIXME: error handling
	    if(on_off)
	      p->state = HAL_PORT_STATE_CALIBRATION;
	    else
	      p->state = HAL_PORT_STATE_UP;

	  	    shw_cal_enable_pattern(p->name, on_off);
	    return HEXP_CAL_RESP_OK;
	    break;

	  case HEXP_CAL_CMD_TX_MEASURE: 
	    TRACE(TRACE_INFO, "TXMeasure %s, port %s", on_off ? "ON" : "OFF", port_name);

	    if(on_off)
	      {
	      if(p->state == HAL_PORT_STATE_CALIBRATION)
		{
		  TRACE(TRACE_INFO, "TXFeedbackOn");
		  
		  p->tx_cal_pending = 1;
		  shw_cal_enable_feedback(p->name, 1, PHY_CALIBRATE_TX);

		  return HEXP_CAL_RESP_OK;

		} else return HEXP_CAL_RESP_ERROR;
	      } else {
	      shw_cal_enable_feedback(p->name, 0, PHY_CALIBRATE_TX);
	      return HEXP_CAL_RESP_OK;

	    }
	    
	    break;
	  case HEXP_CAL_CMD_RX_MEASURE: 
	    TRACE(TRACE_INFO, "RXMeasure %s, port %s", on_off ? "ON" : "OFF", port_name);

	    if(on_off)
	      {
		  p->rx_cal_pending = 1;
		  shw_cal_enable_feedback(p->name, 1, PHY_CALIBRATE_RX);
		  p->state = HAL_PORT_STATE_CALIBRATION;
		  return HEXP_CAL_RESP_OK;
	      } else {
	      shw_cal_enable_feedback(p->name, 0, PHY_CALIBRATE_RX);
	      p->state = HAL_PORT_STATE_UP;
	      return HEXP_CAL_RESP_OK;

	    }


	    
	    break;




	  };

	return 0;
}

// returns the name of the interface for given index (0..max) or an empty string if 
// there is no such interface
int halexp_query_ports(hexp_port_list_t *list)
{
  int i;
  int n = 0;
  
  TRACE(TRACE_INFO,"QueryPorts");

  for(i=0; i<MAX_PORTS;i++)
    {
      if(ports[i].in_use)
		  ptpd_wrap_strcpy(list->port_names[n++], ports[i].name);
    }

  list->num_ports = n;
  return 0;
}



