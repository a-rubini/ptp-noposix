// this is a simple, PTP-like synchronization test program
// usage: netif_test [-m/-s] interface. (-m = master, -s = slave)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <math.h>

//#include <inttypes.h>  -- now in ptpd-wrappers.h
#include <sys/time.h>

#include "ptpd_netif.h"
#include "hal_client.h"
#include <hw/fpga_regs.h>
#include <hw/pps_gen_regs.h>

#define PTYPE_ANNOUNCE 1
#define PTYPE_ANNOUNCE_RESP 2
#define PTYPE_LOCK_REQUEST 2
#define PTYPE_LOCK_ACK 3
#define PTYPE_CAL_REQ 4
#define PTYPE_CAL_DONE 5
#define PTYPE_REMOTE_RESET 6
#define PTYPE_SYNC 7
#define PTYPE_SYNC_FOLLOWUP 8
#define PTYPE_DELAY_REQ 9
#define PTYPE_DELAY_RESP 10


typedef struct {
  int ptype;
  int seq;
  struct  {
    wr_timestamp_t t1, t2, t3, t4;
    uint32_t master_delta_tx;
    uint32_t master_delta_rx;
    
  } delay;
		
  struct {
    int is_master;
  } announce;

  struct {
    int ok;
  } announce_resp;
} sync_packet_t;

#define ST_INIT 0
#define ST_WAIT_LINK 1
#define ST_S_WAIT_ANNOUNCE 2
#define ST_M_SEND_ANNOUNCE 3
#define ST_M_CALIBRATE 4
#define ST_S_CALIBRATE 5
#define ST_S_WAIT_SYNC 6
#define ST_M_WAIT_SLAVE_LOCK 7
#define ST_M_WAIT_CALIBRATE 8
#define ST_S_WAIT_LOCK 9
#define ST_M_SEND_SYNC 10
#define ST_S_WAIT_FOLLOWUP 12
#define ST_M_TX_CALIBRATE 13
#define ST_M_WAIT_COMMAND 14
#define ST_M_RX_CALIBRATE 15
#define ST_S_TX_CALIBRATE 16
#define ST_S_RX_CALIBRATE 17
#define ST_S_START_CALIBRATION 18

#define ST_S_WAIT_DELAY_RESP 20

typedef struct
{
  uint64_t start_tics;
  uint64_t timeout;
} timeout_t ;

uint64_t get_tics()
{
  struct timezone tz = {0,0};
  struct timeval tv;
  gettimeofday(&tv, &tz);
	
  return (uint64_t) tv.tv_sec * 1000000ULL + tv.tv_usec;
}

static inline int tmo_init(timeout_t *tmo, uint32_t milliseconds)
{
  tmo->start_tics = get_tics();
  tmo->timeout = (uint64_t) milliseconds * 1000ULL;
}

static inline int tmo_restart(timeout_t *tmo)
{
  tmo->start_tics = get_tics();
}

static inline int tmo_expired(timeout_t *tmo)
{
  return (get_tics() - tmo->start_tics > tmo->timeout);
}


#define ANNOUNCE_INTERVAL 200
#define LINK_POLL_INTERVAL 200
#define SYNC_INTERVAL 500
#define CALIBRATION_TIME 1000
#define LOCK_CHECK_INTERVAL 500

#define OUR_ETHERTYPE 0x88f8
#define OUR_MCAST_ADDR { 0x01, 0x80, 0xc2, 0x01, 0x00, 0x00 }

int check_link_up(char *iface)
{
  hexp_port_state_t state;
  int rval = halexp_get_port_state(&state, iface);
  return state.up;
}

void sync_pkt_send(wr_socket_t *sock, sync_packet_t *pkt, wr_timestamp_t *tx_ts)
{
  uint8_t mac[] = OUR_MCAST_ADDR;
  wr_sockaddr_t send_addr;
 
  send_addr.ethertype = OUR_ETHERTYPE;
  memcpy(send_addr.mac, mac, 6); 

  ptpd_netif_sendto(sock, &send_addr, pkt, sizeof(sync_packet_t)-2, tx_ts);
}


int sync_pkt_receive(wr_socket_t *sock, sync_packet_t *pkt, wr_timestamp_t *ts)
{
  wr_sockaddr_t from;
  int nrx = ptpd_netif_recvfrom(sock, &from, pkt, sizeof(sync_packet_t), ts);
	
  return (nrx == sizeof(sync_packet_t) && from.ethertype == OUR_ETHERTYPE);
}





void master_fsm(char *if_name)
{
  int state = ST_INIT;
  timeout_t tmo, link_timer;
  wr_socket_t *m_sock;
  wr_sockaddr_t sock_addr;
  sync_packet_t tx_pkt, rx_pkt;
  hexp_port_state_t port_state;
  uint32_t delta_rx, delta_tx;

  wr_timestamp_t sync_t1, sync_t4, rx_ts;

  int got_packet;
  int seq = 0;
  int link_up = 0;
	
  ptpd_wrap_strcpy(sock_addr.if_name, if_name);
  sock_addr.family = PTPD_SOCK_RAW_ETHERNET; // socket type
  sock_addr.ethertype = OUR_ETHERTYPE;
  memset(sock_addr.mac, 0, 6); 

  m_sock = ptpd_netif_create_socket(PTPD_SOCK_RAW_ETHERNET, 0, &sock_addr);
	
	
  fprintf(stderr,"Running as a master\n");


  tmo_init(&link_timer, 100);

  for(;;)
    {

      if(tmo_expired(&link_timer))
	{
	  link_up = check_link_up(if_name);
	  tmo_restart(&link_timer);
	}

      

      got_packet = sync_pkt_receive(m_sock, &rx_pkt, &rx_ts);

    // handle "universal" messages (not state-dependent)
      if(got_packet)
	{
	  
	  switch(rx_pkt.ptype)
	    {
	    case PTYPE_REMOTE_RESET:
	      fprintf(stderr, "[master] Slave triggered a protocol reset\n");
	      state = ST_INIT;
	      got_packet = 0;
	      break;
	    case PTYPE_CAL_REQ:
	      fprintf(stderr, "[master] Slave requested calibration pattern\n");
	      ptpd_netif_calibration_pattern_enable(if_name, 0, 0, 0);
	      got_packet = 0;
	      break;
	    case PTYPE_CAL_DONE:
	      fprintf(stderr, "[master] Slave is done with its calibration\n");
	      ptpd_netif_calibration_pattern_disable(if_name);
	      got_packet = 0;
	      break;
	    case PTYPE_DELAY_REQ:
	      fprintf(stderr, "[master] Got DELAY_REQ from the slave\n");
	      
	      tx_pkt.ptype = PTYPE_DELAY_RESP;
	      tx_pkt.delay.t4 = rx_ts;
	      sync_pkt_send(m_sock, &tx_pkt, NULL);
	      
	      break;
	    }
	}

      if(state != ST_INIT && state !=ST_WAIT_LINK && !link_up)
	{
	  fprintf(stderr, "[master] Link has fucked up.\n");
	  state = ST_INIT;

	}

      switch(state)
	{
	case ST_INIT:
	  tmo_init(&tmo, LINK_POLL_INTERVAL);
	  state = ST_WAIT_LINK;
	  fprintf(stderr, "[master] Waiting for the link to go up");
		  
	  break;
		
	case ST_WAIT_LINK:
	  if(tmo_expired(&tmo))
	    {
	      if(link_up)
		{
		  fprintf(stderr,"\n[master] Link up.\n");
		  state = ST_M_SEND_ANNOUNCE;
		  tmo_init(&tmo, ANNOUNCE_INTERVAL);
		} else {
		fprintf(stderr, ".");
		tmo_restart(&tmo);
	      }
	    }
	  break;
	  

			
	case ST_M_SEND_ANNOUNCE:
	  if(tmo_expired(&tmo))
	    {
	      tx_pkt.ptype = PTYPE_ANNOUNCE;
	      tx_pkt.announce.is_master = 1;
	      sync_pkt_send(m_sock,  &tx_pkt, NULL);
	      tmo_restart(&tmo);
	    }
			
	  if(got_packet && rx_pkt.ptype == PTYPE_ANNOUNCE_RESP)
	    {
	      tx_pkt.ptype = PTYPE_LOCK_REQUEST;
	      sync_pkt_send(m_sock, &tx_pkt, NULL);
	      state = ST_M_WAIT_SLAVE_LOCK;
	      fprintf(stderr,"[master] Got ANNOUNCE_RESP, sending LOCK command.\n");
	    }
	  break;
		
	case ST_M_WAIT_SLAVE_LOCK:
	  {
	    if(got_packet  && rx_pkt.ptype == PTYPE_LOCK_ACK)
	      {
		state = ST_M_TX_CALIBRATE;
		fprintf(stderr,"[master] Slave ACKed its lock.\n");
		ptpd_netif_calibration_pattern_enable(if_name, 0, 0 ,0);
		ptpd_netif_calibrating_enable(PTPD_NETIF_TX, if_name);
		tmo_init(&tmo, 100);
	      }
	
	  }
	  break;

	case ST_M_TX_CALIBRATE:
	  if(tmo_expired(&tmo))
	    {
	      if(ptpd_netif_calibrating_poll(PTPD_NETIF_TX, if_name, &delta_tx) == PTPD_NETIF_READY)
		{
		  fprintf(stderr,"[master] Calibrated TX, deltaTX=%d.\n",delta_tx);
		  
		  ptpd_netif_calibrating_disable(PTPD_NETIF_TX, if_name);
		  ptpd_netif_calibration_pattern_disable(if_name);
		  ptpd_netif_calibrating_enable(PTPD_NETIF_RX, if_name);

		  tx_pkt.ptype = PTYPE_CAL_REQ;
		  sync_pkt_send(m_sock, &tx_pkt,  NULL);

		  state = ST_M_RX_CALIBRATE;
		  tmo_init(&tmo, 100);

		} else tmo_restart(&tmo);
	    }

	  break;

	case ST_M_RX_CALIBRATE:

	  if(tmo_expired(&tmo))
	    {

	      if(ptpd_netif_calibrating_poll(PTPD_NETIF_RX, if_name, &delta_rx) == PTPD_NETIF_READY)
		{
		  ptpd_netif_calibrating_disable(PTPD_NETIF_RX, if_name);
		  fprintf(stderr,"[master] Calibrated RX, deltaRX=%d.\n",delta_rx);

		  tx_pkt.ptype = PTYPE_CAL_DONE;
		  sync_pkt_send(m_sock, &tx_pkt,  NULL);

		  state = ST_M_SEND_SYNC;
		  tmo_init(&tmo, SYNC_INTERVAL);
		} else
		tmo_restart(&tmo);
	    }


	  break;

	case ST_M_SEND_SYNC:
	  if(tmo_expired(&tmo))
	    {
	      fprintf(stderr,"[master] Sending SYNC/FOLLOWUP.\n");
	      
	      tx_pkt.ptype = PTYPE_SYNC;
	      tx_pkt.delay.master_delta_tx = delta_tx;
	      tx_pkt.delay.master_delta_rx = delta_rx;
	      sync_pkt_send(m_sock, &tx_pkt, &sync_t1);
	      tx_pkt.ptype = PTYPE_SYNC_FOLLOWUP;
	      tx_pkt.delay.t1 = sync_t1;
	      sync_pkt_send(m_sock, &tx_pkt, NULL);
	      tmo_restart(&tmo);
	      
	      


	    }
	  break;

	}
      usleep(1000);
    }
}


void dump_timestamp(char *what, wr_timestamp_t ts)
{
  fprintf(stderr, "%s = %lld:%d:%d\n", what, ts.utc, ts.nsec, ts.phase);
}



int64_t ts_to_picos(wr_timestamp_t ts)
{
  return (int64_t) ts.utc * (int64_t)1000000000000LL
  + (int64_t) ts.nsec * (int64_t)1000LL
    + (int64_t) ts.phase;
}


wr_timestamp_t picos_to_ts(int64_t picos)
{
  int64_t phase = picos % 1000LL;
  picos = (picos - phase) / 1000LL;

  int64_t nsec = picos % 1000000000LL;
  picos = (picos-nsec)/1000000000LL;

  wr_timestamp_t ts;
  ts.utc = (int64_t) picos;
  ts.nsec = (int32_t) nsec;
  ts.phase = (int32_t) phase;

  return ts;
}

wr_timestamp_t ts_add(wr_timestamp_t a, wr_timestamp_t b)
{
  wr_timestamp_t c;

  c.utc = 0;
  c.nsec = 0;

  c.phase =a.phase + b.phase;
  if(c.phase >= 1000)
    {
     c.phase -= 1000;
     c.nsec++;
    }

  c.nsec += (a.nsec + b.nsec);
  
  if(c.nsec >= 1000000000L)
    {
      c.nsec -= 1000000000L;
      c.utc++;
    }

  c.utc += (a.utc + b.utc);

  return c;
}

wr_timestamp_t ts_sub(wr_timestamp_t a, wr_timestamp_t b)
{
  wr_timestamp_t c;

  c.utc = 0;
  c.nsec = 0;

  c.phase = a.phase - b.phase;

  if(c.phase < 0) 
    {
    c.phase+=1000;
    c.nsec--;
    }

  c.nsec += a.nsec - b.nsec;
  if(c.nsec < 0)
    {
      c.nsec += 1000000000L;
      c.utc--;
    }

  c.utc += a.utc - b.utc;

  return c;
}


wr_timestamp_t ts_div2(wr_timestamp_t a)
{
  if(a.utc % 1LL)
    {
      a.nsec += 500000000L;
    }
  a.utc /= 2;

  if(a.nsec % 1L)
    {
      a.phase += 500;
    }

  a.nsec /= 2;
  a.phase /= 2;

  return a;
}

wr_timestamp_t slave_current_offset;
int slave_phase_track ;
int32_t slave_phase_setpoint;

// "Hardwarizes" the timestamp - e.g. makes the nanosecond field a multiple of 8ns cycles
// and puts the extra nanoseconds in the phase field

wr_timestamp_t ts_hardwarize(wr_timestamp_t ts)
{


  
  
  if(ts.nsec > 0)
    {
      int32_t extra_nsec = ts.nsec % 8;

      if(extra_nsec)
	{
	  ts.nsec -=extra_nsec;
	  ts.phase += extra_nsec * 1000;
	}
    }

  return ts;
}

wr_timestamp_t ts_zero()
{
  wr_timestamp_t a;
  a.utc = 0;
  a.nsec = 0;
  a.phase = 0;
  return a;
}


#define WR_SYNC_NSEC 1
#define WR_SYNC_TAI 2
#define WR_SYNC_PHASE 3
#define WR_TRACK_PHASE 4
#define WR_WAIT_SYNC_IDLE 5

typedef struct {
  char if_name[16];
  int state;
  int next_state;

  wr_timestamp_t t4, prev_t4;
  wr_timestamp_t mu;
  wr_timestamp_t nsec_offset;
  

  int32_t delta_tx_m;
  int32_t delta_rx_m;
  int32_t delta_tx_s;
  int32_t delta_rx_s;

  int32_t cur_setpoint;

  int64_t delta_ms;
  int64_t delta_ms_prev;

  int fuckup_cycles;
} wr_servo_state_t;


FILE *log_file = NULL;

void slave_servo_init(wr_servo_state_t *s, const char *if_name, 
		      int32_t delta_tx_m, int32_t delta_rx_m, int32_t delta_tx_s, int32_t delta_rx_s)
{
  ptpd_wrap_strncpy(s->if_name, if_name, 16);

  fprintf(stderr,"[slave] initializing clock servo\n");

  s->state = WR_SYNC_TAI;
  s->cur_setpoint = 0;

  s->delta_tx_m = delta_tx_m;
  s->delta_rx_m = delta_rx_m;

  s->delta_tx_s = delta_tx_s;
  s->delta_rx_s = delta_rx_s;
  s->fuckup_cycles = 0;
}



void slave_update_clock(wr_servo_state_t *s, wr_timestamp_t t1, wr_timestamp_t t2, wr_timestamp_t t3, wr_timestamp_t t4)
{
  double big_delta, mu, alpha, asymmetry;
  double delay_ms;
  wr_timestamp_t ts_offset, ts_offset_hw, ts_phase_adjust;
  hexp_pps_params_t adjust;


  dump_timestamp("t1", t1);
  dump_timestamp("t2", t2); 
  dump_timestamp("t3", t3);
  dump_timestamp("t4", t4);

  s->mu = ts_sub(ts_sub(t4, t1), ts_sub(t3, t2));

  dump_timestamp("mdelay", s->mu);

  alpha = 1.4682e-04*1.76; // EXPERIMENTALLY DERIVED. VALID.

  big_delta = (double) s->delta_tx_m + (double) s->delta_tx_s + (double) s->delta_rx_m + (double) s->delta_rx_s;


  delay_ms = ((double)ts_to_picos(s->mu) - big_delta) / (2.0 + alpha) // fiber part
    + (double)s->delta_tx_m + (double) s->delta_rx_s; // PHY/routing part

  printf("delay_ms = %.0f\n", delay_ms);
  printf("mu = %lld\n", ts_to_picos(s->mu));
  
  ts_offset = ts_add(ts_sub(t1, t2), picos_to_ts((int64_t)delay_ms));
  ts_offset_hw = ts_hardwarize(ts_offset);

  dump_timestamp("offset", ts_offset_hw);

  printf("state %d\n", s->state);

  s->delta_ms = delay_ms;

  if(!log_file)
    log_file=fopen("/tmp/callog", "wb");

  fprintf(log_file, "%lld %d %d %d %d %.0f\n", ts_to_picos(s->mu),  s->delta_tx_m , s->delta_tx_s, s->delta_rx_m, s->delta_rx_s, delay_ms);

  fflush(log_file);

  switch(s->state)
    {
    case WR_WAIT_SYNC_IDLE:
      ptpd_wrap_strcpy(adjust.port_name, s->if_name); 

      if(!halexp_pps_cmd(HEXP_PPSG_CMD_POLL, &adjust))
	{
	  s->state = s->next_state;
	  sleep(2);
	}
      break;

    case WR_SYNC_TAI:
      if(ts_offset_hw.utc != 0)
	{
	  ptpd_wrap_strcpy(adjust.port_name, s->if_name); 
	  adjust.adjust_utc = ts_offset_hw.utc;

	  fprintf(stderr,"[slave] Adjusting UTC counter\n");

	  halexp_pps_cmd(HEXP_PPSG_CMD_ADJUST_UTC, &adjust);
	  s->next_state = WR_SYNC_NSEC;
	  s->state = WR_WAIT_SYNC_IDLE;
	} else s->state = WR_SYNC_NSEC;
      break;

    case WR_SYNC_NSEC:
      if(ts_offset_hw.nsec != 0)
	{
	  ptpd_wrap_strcpy(adjust.port_name, s->if_name); 
	  adjust.adjust_nsec = ts_offset_hw.nsec;

	  fprintf(stderr,"[slave] Adjusting NSEC counter\n");

	  halexp_pps_cmd(HEXP_PPSG_CMD_ADJUST_NSEC, &adjust);
	  s->next_state = WR_SYNC_PHASE;
	  s->state = WR_WAIT_SYNC_IDLE;
	} else s->state = WR_SYNC_PHASE;
      break;

    case WR_SYNC_PHASE:

      s->cur_setpoint = -ts_offset_hw.phase;

      ptpd_wrap_strcpy(adjust.port_name, s->if_name); 
      adjust.adjust_phase_shift = s->cur_setpoint;
      halexp_pps_cmd(HEXP_PPSG_CMD_ADJUST_PHASE, &adjust);

      s->next_state = WR_TRACK_PHASE;
      s->state = WR_WAIT_SYNC_IDLE;
      s->delta_ms_prev = s->delta_ms;

      break;


    case WR_TRACK_PHASE:

      // just follow the changes of deltaMS
      s->cur_setpoint -= (s->delta_ms - s->delta_ms_prev);
      
      ptpd_wrap_strcpy(adjust.port_name, s->if_name); 
      adjust.adjust_phase_shift = s->cur_setpoint;
      halexp_pps_cmd(HEXP_PPSG_CMD_ADJUST_PHASE, &adjust);

      s->delta_ms_prev = s->delta_ms;

      sleep(1);
      break;

    }
}



void slave_fsm(char *if_name)
{
  int state = ST_INIT;
  timeout_t tmo, link_timer;
  wr_socket_t *m_sock;
  wr_sockaddr_t sock_addr;
  sync_packet_t tx_pkt, rx_pkt;
  int got_packet;
  hexp_port_state_t port_state;
  wr_timestamp_t rx_ts, t1, t2, t3, t4;
  uint32_t delta_tx_m, delta_rx_m;
  uint32_t delta_tx_s, delta_rx_s;
  wr_servo_state_t servo;
  int slave_servo_ok;


  int phase = 0;
  int link_up = 0;

  FILE *f_phlog = fopen("/tmp/phase_log_slave", "wb");
	
  ptpd_wrap_strcpy(sock_addr.if_name, if_name);
  sock_addr.family = PTPD_SOCK_RAW_ETHERNET; // socket type
  sock_addr.ethertype = OUR_ETHERTYPE;
  memset(sock_addr.mac, 0, 6); 

  m_sock = ptpd_netif_create_socket(PTPD_SOCK_RAW_ETHERNET, 0, &sock_addr);
	
	
  fprintf(stderr,"Running as a slave\n");

  tx_pkt.ptype = PTYPE_REMOTE_RESET;
  sync_pkt_send(m_sock,  &tx_pkt, NULL);

  tmo_init(&link_timer, 100);

  for(;;)
    {

      if(tmo_expired(&link_timer))
	{
	  link_up = check_link_up(if_name);
	  tmo_restart(&link_timer);
	}

 
      got_packet = sync_pkt_receive(m_sock, &rx_pkt, &rx_ts);

      // handle "universal" messages (not state-dependent)
      if(got_packet)
	{
	  switch(rx_pkt.ptype)
	    {
	    case PTYPE_REMOTE_RESET:
	      fprintf(stderr, "[slave] Master triggered a protocol reset\n");
	      state = ST_INIT;
	      got_packet = 0;
	      break;
	    case PTYPE_CAL_REQ:
	      fprintf(stderr, "[slave] Master requested calibration pattern\n");
	      ptpd_netif_calibration_pattern_enable(if_name, 0, 0, 0);
	      got_packet = 0;
	      break;
	    case PTYPE_CAL_DONE:
	      fprintf(stderr, "[slave] Master is done with its calibration\n");
	      ptpd_netif_calibration_pattern_disable(if_name);
	      got_packet = 0;

	      state = ST_S_START_CALIBRATION;
	      break;
	    }
	}

      if(state != ST_INIT && state !=ST_WAIT_LINK && !link_up)
	{
	  fprintf(stderr, "[slave] Link has fucked up.\n");
	  state = ST_INIT;

	}


      switch(state)
	{
	case ST_INIT:
	  tmo_init(&tmo, LINK_POLL_INTERVAL);
	  state = ST_WAIT_LINK;
	  fprintf(stderr, "Waiting for the link to go up");
	  slave_servo_ok = 0;
	  break;
		
	case ST_WAIT_LINK:
	  if(tmo_expired(&tmo))
	    {
	      if(link_up)
		{
		  fprintf(stderr,"\n[slave] Link up.\n");
		  state = ST_S_WAIT_ANNOUNCE;
		  tmo_init(&tmo, ANNOUNCE_INTERVAL);
		} else {
		fprintf(stderr, ".");
		tmo_restart(&tmo);
	      }
	    }
	  break;
			
	case ST_S_WAIT_ANNOUNCE: 
	  if(got_packet && rx_pkt.ptype == PTYPE_ANNOUNCE)
	    {
	      tx_pkt.ptype = PTYPE_ANNOUNCE_RESP;
	      tx_pkt.announce_resp.ok = 1;
	      fprintf(stderr,"[slave] Got ANNOUNCE message!\n");
	      sync_pkt_send(m_sock, &tx_pkt, NULL);

	      ptpd_netif_locking_enable(0, if_name); //txrx is irrelevant


	      state = ST_S_WAIT_LOCK;
	      tmo_init(&tmo, LOCK_CHECK_INTERVAL);
	    }
	  break;
				
	case ST_S_WAIT_LOCK:
	  {
	    if(tmo_expired(&tmo))
	      {
		tmo_restart(&tmo);

		int rval = ptpd_netif_locking_poll(0, if_name);
 						
 		if(rval  == PTPD_NETIF_READY)
		  {
		    fprintf(stderr, "[slave] Port %s locked.\n", if_name);
		    tx_pkt.ptype = PTYPE_LOCK_ACK;
		    sync_pkt_send(m_sock, &tx_pkt, NULL);
		    state = ST_S_WAIT_SYNC;
		    tmo_init(&tmo, 500);
		  }
	      }
	    break;
	  }

	case ST_S_START_CALIBRATION:
	  ptpd_netif_calibration_pattern_enable(if_name, 0, 0 ,0);
	  ptpd_netif_calibrating_enable(PTPD_NETIF_TX, if_name);
	  tmo_init(&tmo, 100);
	  state = ST_S_TX_CALIBRATE;
	  break;

	case ST_S_TX_CALIBRATE:
	  if(tmo_expired(&tmo))
	    {
	      uint64_t tx_delta;
	      if(ptpd_netif_calibrating_poll(PTPD_NETIF_TX, if_name, &tx_delta) == PTPD_NETIF_READY)
		{
		  fprintf(stderr,"[slave] Calibrated TX, deltaTX=%lld.\n",tx_delta);
		  delta_tx_s = tx_delta;
		  
		  ptpd_netif_calibrating_disable(PTPD_NETIF_TX, if_name);
		  ptpd_netif_calibration_pattern_disable(if_name);
		  ptpd_netif_calibrating_enable(PTPD_NETIF_RX, if_name);

		  tx_pkt.ptype = PTYPE_CAL_REQ;
		  sync_pkt_send(m_sock, &tx_pkt,  NULL);

		  state = ST_S_RX_CALIBRATE;
		  tmo_init(&tmo, 100);

		} else tmo_restart(&tmo);
	    }
	  break;

	case ST_S_RX_CALIBRATE:

	  if(tmo_expired(&tmo))
	    {
	      uint64_t rx_delta;
	      if(ptpd_netif_calibrating_poll(PTPD_NETIF_RX, if_name, &rx_delta) == PTPD_NETIF_READY)
		{
		  ptpd_netif_calibrating_disable(PTPD_NETIF_RX, if_name);
		  fprintf(stderr,"[slave] Calibrated RX, deltaRX=%lld.\n",rx_delta);
		  delta_rx_s = rx_delta;

		  tx_pkt.ptype = PTYPE_CAL_DONE;
		  sync_pkt_send(m_sock, &tx_pkt,  NULL);

		  //		  silly_adjust_test();

		  state = ST_S_WAIT_SYNC;
		} else
		tmo_restart(&tmo);
	    }

	  break;

	case ST_S_WAIT_SYNC:
	  if(rx_pkt.ptype == PTYPE_SYNC)
	    {
	      t2 = rx_ts;

	      delta_rx_m = rx_pkt.delay.master_delta_rx;
	      delta_tx_m = rx_pkt.delay.master_delta_tx;

	      if(!slave_servo_ok)
		{
		  slave_servo_init(&servo, if_name, delta_tx_m, delta_rx_m, delta_tx_s, delta_rx_s);
		  slave_servo_ok = 1;
		}

	      state = ST_S_WAIT_FOLLOWUP;
	    }
	  break;

	case ST_S_WAIT_FOLLOWUP:
	  if(rx_pkt.ptype == PTYPE_SYNC_FOLLOWUP)
	    {
	      t1 = rx_pkt.delay.t1;
	      state =ST_S_WAIT_SYNC;
	      fprintf(stderr,"[slave] Got SYNC/FOLLOWUP message \n");
	      
	      tx_pkt.ptype = PTYPE_DELAY_REQ;
	      sync_pkt_send(m_sock, &tx_pkt, &t3);
	      state = ST_S_WAIT_DELAY_RESP;
	    }

	  break;

	case ST_S_WAIT_DELAY_RESP:

	  if(rx_pkt.ptype == PTYPE_DELAY_RESP)
	    {
	      t4 = rx_pkt.delay.t4;
	      fprintf(stderr,"[slave] Got DELAY_RESP message \n");
	      state = ST_S_WAIT_SYNC;
	      slave_update_clock(&servo, t1,t2,t3,t4);
	      while(sync_pkt_receive(m_sock, &rx_pkt, NULL));
	    }
	  break;

	}
      
    }
}



main(int argc, char *argv[])
{

  if(argc != 3)
    {
      fprintf(stderr,"Usage: %s [-m/-s] iface\n\n",argv[0]);
      return 0;
    }

  if(ptpd_netif_init() < 0)
    {
      fprintf(stderr,"Unable to initialize PTPD-netif. Is the HAL running?\n\n");
    }
  
  hexp_port_list_t ports;
  halexp_query_ports(&ports);
  
  printf("num_ports %d\n", ports.num_ports);
  int i;
  for(i=0;i<ports.num_ports;i++) printf("Port %s\n", ports.port_names[i]);
  
  
  
  if(!strcmp(argv[1], "-m"))
    master_fsm(argv[2]);
  else 	if(!strcmp(argv[1], "-s"))
    slave_fsm(argv[2]);
  else{
    fprintf(stderr,"Invalid parameter: %s\n", argv[1]);
    return -1;
  }
  return 0;
}
