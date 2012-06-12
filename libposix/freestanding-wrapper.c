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

int ptpd_netif_init()
{
  memset(wr_sockets, 0, sizeof(wr_sockets));
  return PTPD_NETIF_OK;
}

int ptpd_netif_get_hw_addr(wr_socket_t *sock, mac_addr_t *mac)
{
  get_mac_addr((uint8_t *)mac);

  return 0;
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

int read_phase_val(hexp_port_state_t *state)
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

//static int t2_phase_transition= 7000;

void ptpd_netif_set_phase_transition(wr_socket_t *sock, int phase)
{
	struct my_socket *s = (struct my_socket *) sock;
	s->phase_transition = phase;
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


wr_socket_t *ptpd_netif_create_socket(int sock_type, int flags, wr_sockaddr_t *bind_addr)
{
  int i;
  uint8_t mac[6];
  hexp_port_state_t pstate;
  struct my_socket *sock;

	for(sock = NULL, i=0;i<SOCKS_NUM;i++)
    	if(!wr_sockets[i].in_use)
    	{
    		sock = &wr_sockets[i];
    		break;
    	}
	
	
	if(!sock)
	{
	 	TRACE_WRAP("No sockets left\n");
	 	return NULL;
	}

  if(sock_type != PTPD_SOCK_RAW_ETHERNET)
  {
    TRACE_WRAP("sock_type=%u\n", sock_type);
    return NULL;
  }
  if(halexp_get_port_state(&pstate, bind_addr->if_name) < 0)
    return NULL;

  memcpy(&sock->bind_addr, bind_addr, sizeof(wr_sockaddr_t));
  /*get mac from endpoint*/
  get_mac_addr(mac);
  memcpy(&sock->local_mac, mac, 6);
  TRACE_WRAP("%s: local_mac= %02x:%02x:%02x:%02x:%02x:%02x\n", __FUNCTION__, sock->local_mac[0], sock->local_mac[1], sock->local_mac[2], sock->local_mac[3],
                                                            sock->local_mac[4], sock->local_mac[5], sock->local_mac[6], sock->local_mac[7]);

  sock->clock_period = pstate.clock_period;
  sock->phase_transition = pstate.t2_phase_transition;
  sock->dmtd_phase = pstate.phase_val;

  /*tmo_init() in WRSW*/
  sock->dmtd_update_tmo.start_tics = timer_get_tics();
  sock->dmtd_update_tmo.timeout = 100; 

  /*packet queue*/
  sock->queue.head = sock->queue.tail = 0;
  sock->queue.n = 0;
  sock->in_use = 1;

  return (wr_socket_t*)(sock);
}

int ptpd_netif_close_socket(wr_socket_t *sock)
{
	struct my_socket *s = (struct my_socket *) sock;
	
	TRACE_WRAP("CloseSocket\n");
	if(s) s->in_use = 0;
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

static inline int inside_range(int min, int max, int x)
{
  if(min < max)
    return (x>=min && x<=max);
  else
    return (x<=max || x>=min);
}

static void update_dmtd(wr_socket_t *sock)
{
	struct my_socket *s = (struct my_socket *) sock;
	hexp_port_state_t pstate;

  TRACE_WRAP("TS %s %d %d\n", __FUNCTION__, s->dmtd_update_tmo.start_tics, s->dmtd_update_tmo.timeout);

	if(timer_get_tics() - s->dmtd_update_tmo.start_tics > s->dmtd_update_tmo.timeout)
	{
		TRACE_WRAP("TS PhaseUpdate!\n");
		halexp_get_port_state(&pstate, s->bind_addr.if_name);

		// FIXME: ccheck if phase value is ready
		s->dmtd_phase = pstate.phase_val;
   		s->clock_period = pstate.clock_period;
       	s->phase_transition = pstate.t2_phase_transition;
		s->dmtd_update_tmo.start_tics = timer_get_tics();
	}
}



void ptpd_netif_linearize_rx_timestamp(wr_timestamp_t *ts, int32_t dmtd_phase, int cntr_ahead, int transition_point, int clock_period)
{
  int trip_lo, trip_hi;
  int phase;

  // "phase" transition: DMTD output value (in picoseconds)
  // at which the transition of rising edge
  // TS counter will appear
  ts->raw_phase = dmtd_phase;

  phase = clock_period -1 -dmtd_phase;

  // calculate the range within which falling edge timestamp is stable
  // (no possible transitions)
  trip_lo = transition_point - clock_period / 4;
  if(trip_lo < 0) trip_lo += clock_period;

  trip_hi = transition_point + clock_period / 4;
  if(trip_hi >= clock_period) trip_hi -= clock_period;

  if(inside_range(trip_lo, trip_hi, phase))
  {
    // We are within +- 25% range of transition area of
    // rising counter. Take the falling edge counter value as the
    // "reliable" one. cntr_ahead will be 1 when the rising edge
    //counter is 1 tick ahead of the falling edge counter

    ts->nsec -= cntr_ahead ? (clock_period / 1000) : 0;

    // check if the phase is before the counter transition value
    // and eventually increase the counter by 1 to simulate a
    // timestamp transition exactly at s->phase_transition
    //DMTD phase value
    if(inside_range(trip_lo, transition_point, phase))
      ts->nsec += clock_period / 1000;

  }

  ts->phase = phase - transition_point - 1;
  if(ts->phase  < 0) ts->phase += clock_period;
  ts->phase = clock_period - 1 -ts->phase;
}
        


int ptpd_netif_recvfrom(wr_socket_t *sock, wr_sockaddr_t *from, void *data,
          size_t data_length, wr_timestamp_t *rx_timestamp)
{
  struct my_socket *my_sock = (struct my_socket *)sock;
  uint32_t cpy, size, remain;
  ethhdr_t *header;
  struct hw_timestamp hwts;

  my_sock = (struct my_socket*) sock;

  /*check if there is something to fetch*/
  if( my_sock->queue.n==0 )
  {
    //TRACE_WRAP("%s: queue is empty\n", __FUNCTION__);
    return 0;
  }

  /*fetch size*/
  size = *(my_sock->queue.buf + my_sock->queue.tail);
  my_sock->queue.tail = (my_sock->queue.tail + 1) % SOCKQ_SIZE;
  my_sock->queue.n--;
  //TRACE_WRAP("%s: size=%d, tail=%d, n=%d\n", __FUNCTION__, size, my_sock->queue.tail, my_sock->queue.n);

  /*fetch packet*/
  remain = size;
  while(remain)
  {
    cpy = min(remain, SOCKQ_SIZE-my_sock->queue.tail);
    //TRACE_WRAP("%s: remain=%d, cpy=%d\n", __FUNCTION__, remain, cpy);
    memcpy( pkg + size - remain,
            my_sock->queue.buf + my_sock->queue.tail,
            cpy);
    my_sock->queue.tail = (my_sock->queue.tail + cpy) % SOCKQ_SIZE;
    my_sock->queue.n -= cpy;
    remain -= cpy;
  }
  //TRACE_WRAP("%s: remain=%d, tail=%d, head=%d\n", __FUNCTION__, remain, my_sock->queue.tail, my_sock->queue.head);
  //TRACE_WRAP("got: ");
 

  header = (ethhdr_t*) pkg;
  from->ethertype = ntohs(header->ethtype);
  memcpy( from->mac, header->srcmac, 6);
  memcpy( from->mac_dest, header->dstmac, 6);

  remain = size-sizeof(ethhdr_t)-sizeof(struct hw_timestamp);  /*size of payload*/
  memcpy( data, pkg+sizeof(ethhdr_t), remain);
  
  memcpy(&hwts, pkg+sizeof(ethhdr_t)+remain, sizeof(struct hw_timestamp));
  //for(i=0;i<size;i++)
  //  TRACE_WRAP("%d ", pkg[i]);
  //TRACE_WRAP("\n%s: %x: %x: %x, size=%d, recvd=%d, sizeof=%d\n", "aabc", hwts.utc, hwts.nsec, hwts.phase, size, remain, sizeof(ethhdr_t));
  
  if(rx_timestamp)
  {
    
     rx_timestamp->raw_nsec = hwts.nsec;
     rx_timestamp->raw_ahead = hwts.ahead;
     spll_read_ptracker(0, &rx_timestamp->raw_phase, NULL);
   
	 rx_timestamp->sec   = hwts.sec;
	 rx_timestamp->nsec  = hwts.nsec;
     rx_timestamp->phase  = 0;
     rx_timestamp->correct = hwts.valid;
   
     ptpd_netif_linearize_rx_timestamp(rx_timestamp, rx_timestamp->raw_phase, hwts.ahead, my_sock->phase_transition, my_sock->clock_period);
//   		mprintf("Linearize: rawph: %d period: %d ahead: %d postph %d\n", rx_timestamp->raw_phase, hwts.ahead, my_sock->clock_period, rx_timestamp->phase);

  }

  //TRACE_WRAP("%s: data: (size=%d) \n", __FUNCTION__, remain);
  //for(i=0; i<remain; i++)
  //  TRACE_WRAP("%x ", *((uint8_t*)data+i));


  TRACE_WRAP("%s: received data from %02x:%02x:%02x:%02x:%02x:%02x to %02x:%02x:%02x:%02x:%02x:%02x\n", __FUNCTION__, from->mac[0],from->mac[1],from->mac[2],from->mac[3],
                                                                                                                   from->mac[4],from->mac[5],from->mac[6],from->mac[7],
                                                                                                                   from->mac_dest[0],from->mac_dest[1],from->mac_dest[2],from->mac_dest[3],
                                                                                                                   from->mac_dest[4],from->mac_dest[5],from->mac_dest[6],from->mac_dest[7]);

  return remain;
}

int ptpd_netif_select( wr_socket_t *wrSock)
{
  return 0;
}

int ptpd_netif_sendto(wr_socket_t *sock, wr_sockaddr_t *to, void *data,
              size_t data_length, wr_timestamp_t *tx_ts)
{
  struct my_socket *s = (struct my_socket *)sock;
  int rval;
  struct hw_timestamp ts;
  uint8_t hdr[ETH_HEADER_SIZE];


  //if(s->bind_addr.family != PTPD_SOCK_RAW_ETHERNET)
  //  return -ENOTSUP;

  //if(data_length > ETHER_MTU-8) return -EINVAL;

  /*dstmac*/
  memcpy(hdr, to->mac, 6); 
  /*srcmac*/
  memcpy(hdr+6, s->local_mac, 6);
  /*ethtype*/
  memcpy(hdr+12, &to->ethertype, 2);

  //mprintf("Sending %d bytes from %02x:%02x:%02x:%02x:%02x:%02x\n", data_length + 14, hdr[6], hdr[7], hdr[8], hdr[9], hdr[10], hdr[11]);
  //mprintf("Sending %d bytes to %02x:%02x:%02x:%02x:%02x:%02x\n", data_length + 14, hdr[0], hdr[1], hdr[2], hdr[3], hdr[4], hdr[5]);

  rval = minic_tx_frame(hdr, (uint8_t*)data, data_length + ETH_HEADER_SIZE, &ts);
  
  tx_ts->sec   = ts.sec;
  tx_ts->nsec  = ts.nsec;
  tx_ts->phase = 0; //ts.phase;
  tx_ts->correct = ts.valid;
  
  return rval;
}


/*RX pkg*/
int update_rx_queues(void)
{
  struct hw_timestamp hwts;
  uint8_t sidx, recvd, size;
  uint8_t cpy;
  uint16_t aligned_ethtype;
  ethhdr_t *hdr;
  struct my_socket *sock = NULL;

  recvd = minic_rx_frame(&(pkg[sizeof(uint8_t)]), &(pkg[sizeof(uint8_t)+ETH_HEADER_SIZE]), MAX_PAYLOAD, &hwts);
  //TRACE_WRAP("%s: recvd=%d\n", __FUNCTION__, recvd);
  if( recvd==0 )
    return -1;
  
  hdr = (ethhdr_t*) (pkg+sizeof(uint8_t));
  /*received frame, find the right socket*/
  
  memcpy(&aligned_ethtype, &hdr->ethtype, 2);
  for(sidx=0; sidx<SOCKS_NUM; sidx++)
    if( wr_sockets[sidx].in_use && 
        !memcmp(hdr->dstmac, wr_sockets[sidx].bind_addr.mac, 6) && 
        aligned_ethtype == wr_sockets[sidx].bind_addr.ethertype)
    {
      sock = &wr_sockets[sidx];
      break;  /*they match*/
    }


  if(!sock)
  {
    TRACE_WRAP("%s: could not find socket for packet\n", __FUNCTION__);
    return -1;
  }

  size = ETH_HEADER_SIZE+recvd+sizeof(struct hw_timestamp);
  if( size+sizeof(uint8_t) > (SOCKQ_SIZE-sock->queue.n) )
  {
    TRACE_WRAP("%s: queue for socket %d full; size=%d, n=%d\n", __FUNCTION__, sidx, size, sock->queue.n);
    return -1;
  }
  
  /*put everything in one piece*/
  memcpy(pkg, &size, sizeof(uint8_t));
  memcpy(pkg+ETH_HEADER_SIZE+recvd+sizeof(uint8_t), &hwts, sizeof(struct hw_timestamp));
  size += sizeof(uint8_t); /*size of data + 1 byte for size itself*/

  while(size) 
  {
    cpy = min(size, SOCKQ_SIZE-sock->queue.head);
    memcpy( sock->queue.buf + sock->queue.head, 
            pkg + sizeof(uint8_t) + ETH_HEADER_SIZE+recvd+sizeof(struct hw_timestamp) - size, 
            cpy);
    sock->queue.head = (sock->queue.head+cpy) % SOCKQ_SIZE;
    sock->queue.n += cpy;
    size -= cpy;
  }
  
  TRACE_WRAP("%s: saved packet to queue\n", __FUNCTION__);
  return sizeof(uint8_t)+ETH_HEADER_SIZE+recvd+sizeof(struct hw_timestamp);
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
