// Wrapper functions for network/timestamping/adjustment operations

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>
#include <linux/net_tstamp.h>
#include <linux/errqueue.h>
#include <linux/sockios.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <asm/types.h>
#include <fcntl.h>
#include <errno.h>


#include <asm/socket.h>

#include "ptpd_netif.h"
#include "hal_client.h"

#ifdef NETIF_VERBOSE
#define netif_dbg(...) printf(__VA_ARGS__)
#else
#define netif_dbg(...)
#endif

#define ETHER_MTU 1518
#define DMTD_UPDATE_INTERVAL 500

struct scm_timestamping {
	struct timespec systime;
	struct timespec hwtimetrans;
	struct timespec hwtimeraw;
};

PACKED struct etherpacket {
	struct ethhdr ether;
	char data[ETHER_MTU];
};

typedef struct
{
	uint64_t start_tics;
	uint64_t timeout;
} timeout_t ;

struct my_socket {
	int fd;
	wr_sockaddr_t bind_addr;
	mac_addr_t local_mac;
	int if_index;

	// parameters for linearization of RX timestamps
	uint32_t clock_period;
	uint32_t phase_transition;
	uint32_t dmtd_phase;
	int dmtd_phase_valid;
	timeout_t dmtd_update_tmo;
};

static uint64_t get_tics()
{
	struct timezone tz = {0, 0};
	struct timeval tv;
	gettimeofday(&tv, &tz);

	return (uint64_t) tv.tv_sec * 1000000ULL + (uint64_t) tv.tv_usec;
}

static inline int tmo_init(timeout_t *tmo, uint32_t milliseconds)
{
	tmo->start_tics = get_tics();
	tmo->timeout = (uint64_t) milliseconds * 1000ULL;
	return 0;
}

static inline int tmo_restart(timeout_t *tmo)
{
	tmo->start_tics = get_tics();
	return 0;
}

static inline int tmo_expired(timeout_t *tmo)
{
	return (get_tics() - tmo->start_tics > tmo->timeout);
}

// cheks if x is inside range <min, max>
static inline int inside_range(int min, int max, int x)
{
	if(min < max)
		return (x>=min && x<=max);
	else
		return (x<=max || x>=min);
}

/* For debugging/testing purposes */
int ptpd_netif_get_dmtd_phase(wr_socket_t *sock, int32_t *phase)
{
	struct my_socket *s = (struct my_socket *) sock;
	hexp_port_state_t pstate;

	halexp_get_port_state(&pstate, s->bind_addr.if_name);

	if(phase) *phase = pstate.phase_val;
	return pstate.phase_val_valid;
}

static void update_dmtd(wr_socket_t *sock)
{
	struct my_socket *s = (struct my_socket *) sock;
	hexp_port_state_t pstate;

	if(tmo_expired(&s->dmtd_update_tmo))
	{
		halexp_get_port_state(&pstate, s->bind_addr.if_name);

		// FIXME: ccheck if phase value is ready
		s->dmtd_phase = pstate.phase_val;
		s->dmtd_phase_valid = pstate.phase_val_valid;

		tmo_restart(&s->dmtd_update_tmo);
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

#define HAL_CONNECT_RETRIES 10
#define HAL_CONNECT_TIMEOUT 2000000 /* us */


int ptpd_netif_init()
{
	if(halexp_client_try_connect(HAL_CONNECT_RETRIES, HAL_CONNECT_TIMEOUT) < 0)
		return PTPD_NETIF_ERROR;

	return PTPD_NETIF_OK;
}


wr_socket_t *ptpd_netif_create_socket(int sock_type, int flags,
				      wr_sockaddr_t *bind_addr)
{
	struct my_socket *s;
	struct sockaddr_ll sll;
	struct ifreq f;

	hexp_port_state_t pstate;

	int fd;

	//    fprintf(stderr,"CreateSocket!\n");

	if(sock_type != PTPD_SOCK_RAW_ETHERNET)
		return NULL;

	if(halexp_get_port_state(&pstate, bind_addr->if_name) < 0)
		return NULL;

	fd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));

	if(fd < 0)
	{
		perror("socket()");
		return NULL;
	}

	fcntl(fd, F_SETFL, O_NONBLOCK);

	// Put the controller in promiscious mode, so it receives everything
	strcpy(f.ifr_name, bind_addr->if_name);
	if(ioctl(fd, SIOCGIFFLAGS,&f) < 0) { perror("ioctl()"); return NULL; }
	f.ifr_flags |= IFF_PROMISC;
	if(ioctl(fd, SIOCSIFFLAGS,&f) < 0) { perror("ioctl()"); return NULL; }

	// Find the inteface index
	strcpy(f.ifr_name, bind_addr->if_name);
	ioctl(fd, SIOCGIFINDEX, &f);


	sll.sll_ifindex = f.ifr_ifindex;
	sll.sll_family   = AF_PACKET;
	sll.sll_protocol = htons(bind_addr->ethertype);
	sll.sll_halen = 6;

	memcpy(sll.sll_addr, bind_addr->mac, 6);

	if(bind(fd, (struct sockaddr *)&sll, sizeof(struct sockaddr_ll)) < 0)
	{
		close(fd);
		perror("bind()");
		return NULL;
	}

	// timestamping stuff:

	int so_timestamping_flags = SOF_TIMESTAMPING_TX_HARDWARE |
		SOF_TIMESTAMPING_RX_HARDWARE |
		SOF_TIMESTAMPING_RAW_HARDWARE;

	struct ifreq ifr;
	struct hwtstamp_config hwconfig;

	strncpy(ifr.ifr_name, bind_addr->if_name, sizeof(ifr.ifr_name));

	hwconfig.tx_type = HWTSTAMP_TX_ON;
	hwconfig.rx_filter = HWTSTAMP_FILTER_PTP_V2_L2_EVENT;

	ifr.ifr_data = &hwconfig;

	if (ioctl(fd, SIOCSHWTSTAMP, &ifr) < 0)
	{
		perror("SIOCSHWTSTAMP");
		return NULL;
	}

	if(setsockopt(fd, SOL_SOCKET, SO_TIMESTAMPING, &so_timestamping_flags,
		      sizeof(int)) < 0)
	{
		perror("setsockopt(SO_TIMESTAMPING)");
		return NULL;
	}

	s=calloc(sizeof(struct my_socket), 1);

	s->if_index = f.ifr_ifindex;

	// get interface MAC address
	if (ioctl(fd, SIOCGIFHWADDR, &f) < 0) {
		perror("ioctl()"); return NULL;
	}

	memcpy(s->local_mac, f.ifr_hwaddr.sa_data, 6);
	memcpy(&s->bind_addr, bind_addr, sizeof(wr_sockaddr_t));

	s->fd = fd;

	// store the linearization parameters
	s->clock_period = pstate.clock_period;
	s->phase_transition = pstate.t2_phase_transition;
	s->dmtd_phase_valid = 0;
	s->dmtd_phase = pstate.phase_val;

	tmo_init(&s->dmtd_update_tmo, DMTD_UPDATE_INTERVAL);

	return (wr_socket_t*)s;
}

int ptpd_netif_close_socket(wr_socket_t *sock)
{
	struct my_socket *s = (struct my_socket *) sock;

	if(!s)
		return 0;

	close(s->fd);
	return 0;
}

static void poll_tx_timestamp(wr_socket_t *sock, wr_timestamp_t *tx_timestamp);

int ptpd_netif_sendto(wr_socket_t *sock, wr_sockaddr_t *to, void *data,
		      size_t data_length, wr_timestamp_t *tx_ts)
{
	struct etherpacket pkt;
	struct my_socket *s = (struct my_socket *)sock;
	struct sockaddr_ll sll;
	int rval;

	if(s->bind_addr.family != PTPD_SOCK_RAW_ETHERNET)
		return -ENOTSUP;

	if(data_length > ETHER_MTU-8) return -EINVAL;

	memset(&pkt, 0, sizeof(struct etherpacket));

	memcpy(pkt.ether.h_dest, to->mac, 6);
	memcpy(pkt.ether.h_source, s->local_mac, 6);
	pkt.ether.h_proto =htons(to->ethertype);

	memcpy(pkt.data, data, data_length);

	size_t len = data_length + sizeof(struct ethhdr);

	if(len < 60) /* pad to the minimum allowed packet size */
		len = 60;

	memset(&sll, 0, sizeof(struct sockaddr_ll));

	sll.sll_ifindex = s->if_index;
	sll.sll_family = AF_PACKET;
	sll.sll_protocol = htons(to->ethertype);
	sll.sll_halen = 6;

	rval =  sendto(s->fd, &pkt, len, 0, (struct sockaddr *)&sll,
		       sizeof(struct sockaddr_ll));

	poll_tx_timestamp(sock, tx_ts);

	return rval;
}


#if 0
static void hdump(uint8_t *buf, int size)
{
	int i;
	netif_dbg("Dump: ");
	for(i=0;i<size;i++) netif_dbg("%02x ", buf[i]);
	netif_dbg("\n");
}
#endif


/* Waits for the transmission timestamp and stores it in tx_timestamp (if not null). */
static void poll_tx_timestamp(wr_socket_t *sock, wr_timestamp_t *tx_timestamp)
{
	char data[16384];

	struct my_socket *s = (struct my_socket *) sock;
	struct msghdr msg;
	struct iovec entry;
	struct sockaddr_ll from_addr;
	struct {
		struct cmsghdr cm;
		char control[1024];
	} control;
	struct cmsghdr *cmsg;
	int res;
	uint32_t rtag;

	struct sock_extended_err *serr = NULL;
	struct scm_timestamping *sts = NULL;

	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = &entry;
	msg.msg_iovlen = 1;
	entry.iov_base = data;
	entry.iov_len = sizeof(data);
	msg.msg_name = (caddr_t)&from_addr;
	msg.msg_namelen = sizeof(from_addr);
	msg.msg_control = &control;
	msg.msg_controllen = sizeof(control);

	res = recvmsg(s->fd, &msg, MSG_ERRQUEUE); //|MSG_DONTWAIT);

	if(tx_timestamp) tx_timestamp->correct = 0;

	if(res <= 0)
		return;

	memcpy(&rtag, data+res-4, 4);

	for (cmsg = CMSG_FIRSTHDR(&msg);
	     cmsg;
	     cmsg = CMSG_NXTHDR(&msg, cmsg)) {

			void *dp = CMSG_DATA(cmsg);

			if(cmsg->cmsg_level == SOL_PACKET
			   && cmsg->cmsg_type == PACKET_TX_TIMESTAMP)
				serr = (struct sock_extended_err *) dp;

			if(cmsg->cmsg_level == SOL_SOCKET
			   && cmsg->cmsg_type == SO_TIMESTAMPING)
				sts = (struct scm_timestamping *) dp;

			//fprintf(stderr, "Serr %x sts %x\n", serr, sts);

			if(serr && sts && tx_timestamp)
			{
				tx_timestamp->correct = 1;
				tx_timestamp->phase = 0;
				tx_timestamp->nsec = sts->hwtimeraw.tv_nsec;
				tx_timestamp->sec = (uint64_t) sts->hwtimeraw.tv_sec & 0x7fffffff;
			}
	}
}

int ptpd_netif_recvfrom(wr_socket_t *sock, wr_sockaddr_t *from, void *data,
			size_t data_length, wr_timestamp_t *rx_timestamp)
{
	struct my_socket *s = (struct my_socket *)sock;
	struct etherpacket pkt;
	struct msghdr msg;
	struct iovec entry;
	struct sockaddr_ll from_addr;
	struct {
		struct cmsghdr cm;
		char control[1024];
	} control;
	struct cmsghdr *cmsg;
	struct scm_timestamping *sts = NULL;

	size_t len = data_length + sizeof(struct ethhdr);

	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = &entry;
	msg.msg_iovlen = 1;
	entry.iov_base = &pkt;
	entry.iov_len = len;
	msg.msg_name = (caddr_t)&from_addr;
	msg.msg_namelen = sizeof(from_addr);
	msg.msg_control = &control;
	msg.msg_controllen = sizeof(control);

	int ret = recvmsg(s->fd, &msg, MSG_DONTWAIT);

	if(ret < 0 && errno == EAGAIN) return 0; // would be blocking
	if(ret == -EAGAIN) return 0;

	if(ret <= 0) return ret;

	memcpy(data, pkt.data, ret - sizeof(struct ethhdr));

	from->ethertype = ntohs(pkt.ether.h_proto);
	memcpy(from->mac, pkt.ether.h_source, 6);
	memcpy(from->mac_dest, pkt.ether.h_dest, 6);

	if(rx_timestamp)
		rx_timestamp->correct = 0;

	for (cmsg = CMSG_FIRSTHDR(&msg);
	     cmsg;
	     cmsg = CMSG_NXTHDR(&msg, cmsg)) {

		void *dp = CMSG_DATA(cmsg);

		if(cmsg->cmsg_level == SOL_SOCKET
		   && cmsg->cmsg_type == SO_TIMESTAMPING)
			sts = (struct scm_timestamping *) dp;

	}

	if(sts && rx_timestamp)
	{
		int cntr_ahead = sts->hwtimeraw.tv_sec & 0x80000000 ? 1: 0;
		rx_timestamp->nsec = sts->hwtimeraw.tv_nsec;
		rx_timestamp->sec =
			(uint64_t) sts->hwtimeraw.tv_sec & 0x7fffffff;

		rx_timestamp->raw_nsec = sts->hwtimeraw.tv_nsec;
		rx_timestamp->raw_ahead = cntr_ahead;

		update_dmtd(sock);
		if(s->dmtd_phase_valid)
		{
			ptpd_netif_linearize_rx_timestamp(rx_timestamp, s->dmtd_phase, cntr_ahead, s->phase_transition, s->clock_period);
			rx_timestamp->correct = 1;
		}
	}

	return ret - sizeof(struct ethhdr);
}

/*
 * Turns on locking
 */
int ptpd_netif_locking_enable(int txrx, const char *ifaceName, int priority)
{
  netif_dbg("(PTPD_NETIF): start locking\n");
  int ret = halexp_lock_cmd(ifaceName, HEXP_LOCK_CMD_START, 0);

 	return (ret < 0 ? PTPD_NETIF_ERROR : PTPD_NETIF_OK);
}

int ptpd_netif_locking_disable(int txrx, const char *ifaceName, int priority)
{
	return PTPD_NETIF_OK;
}

int ptpd_netif_locking_poll(int txrx, const char *ifaceName, int priority)
{
  if( halexp_lock_cmd(ifaceName, HEXP_LOCK_CMD_CHECK, 0) == HEXP_LOCK_STATUS_LOCKED)
	  return PTPD_NETIF_READY;
	else
	  return PTPD_NETIF_NOT_READY;
}

/* We don't need these functions in V3/spec anymore - the transceivers are deterministic! */

int ptpd_netif_calibration_pattern_enable(const char *ifaceName,
					  unsigned int calibrationPeriod,
					  unsigned int calibrationPattern,
					  unsigned int calibrationPatternLen)
{
	return PTPD_NETIF_OK;
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

int ptpd_netif_calibration_pattern_disable(const char *ifaceName)
{
	return PTPD_NETIF_OK;
}

int ptpd_netif_read_calibration_data(const char *ifaceName, uint64_t *deltaTx,
				     uint64_t *deltaRx, int32_t *fix_alpha, int32_t *clock_period)
{
	hexp_port_state_t state;

	//read the port state
	halexp_get_port_state(&state, ifaceName);


	// check if the data is available
	if(state.valid && state.tx_calibrated && state.rx_calibrated)
	{

        fprintf(stderr, "servo:ifaceName: %s (%d / %d)\n", ifaceName, state.delta_tx ,state.delta_rx);

		if(deltaTx)
			*deltaTx = state.delta_tx;

		if(deltaRx)
			*deltaRx = state.delta_rx;

		if(fix_alpha)
            *fix_alpha = state.fiber_fix_alpha;

        if(clock_period)
            *clock_period = state.clock_period;

		return PTPD_NETIF_OK;
	}

	return PTPD_NETIF_NOT_FOUND;
}

int ptpd_netif_select( wr_socket_t *wrSock)
{
	struct my_socket *s = (struct my_socket *)wrSock;

	int ret;
	fd_set readfds;

	FD_ZERO(&readfds);
	FD_SET(s->fd, &readfds);

	ret = select(s->fd + 1, &readfds, 0, 0, 0) > 0;

	if(ret < 0)
	{
		if(errno == EAGAIN || errno == EINTR)
			return 0;
	}

	return 1;
}

int ptpd_netif_get_hw_addr(wr_socket_t *sock, mac_addr_t *mac)
{
	struct my_socket *s = (struct my_socket *)sock;
	memcpy(mac, s->local_mac, 6);
	return 0;
}

int ptpd_netif_get_port_state(const char *ifaceName)
{
	hexp_port_state_t state;

	//read the port state
	halexp_get_port_state(&state, ifaceName);

	// check if the data is available
	if(state.valid)
	{
		//check if link is UP
		if(state.up > 0)
			return PTPD_NETIF_OK;
		else
		{
//      if(!strcmp(ifaceName,"wru1") || !strcmp(ifaceName,"wru0"))
//	printf("(ptpd_netif) linkdown detected on port: %s\n",ifaceName);
			return PTPD_NETIF_ERROR;
		}

	}
/*	printf("(ptpd_netif) linkdown detected on port: %s "
	       "[no valid port state data)\n",ifaceName);*/
	//should not get here
	return PTPD_NETIF_ERROR;
}

int ptpd_netif_extsrc_detection()
{
       return 0;


}

int ptpd_netif_get_ifName(char *ifname, int number)
{

	int i;
	int j = 0;
	hexp_port_list_t list;

	halexp_query_ports(&list);

	for( i = 0; i < list.num_ports; i++)
	{
		if(j == number)
		{
			strcpy(ifname,list.port_names[i]);
			return PTPD_NETIF_OK;
		}
		else
			j++;
	}
	return PTPD_NETIF_ERROR;
}

uint64_t ptpd_netif_get_msec_tics()
{
//	printf("getmsec: %lld\n", get_tics() / 1000ULL);
	return get_tics() / 1000ULL;
}


int ptpd_netif_adjust_counters(int64_t adjust_sec, int32_t adjust_nsec)
{
	hexp_pps_params_t p;
	int cmd;

    if(!adjust_nsec && !adjust_sec)
        return PTPD_NETIF_OK;

	if(adjust_sec && adjust_nsec)
	{
	    fprintf(stderr, " FATAL : trying to adjust both the SEC and the NS counters simultaneously. \n");
	    exit(-1);
	}

	if(adjust_sec)
	{
	    cmd = HEXP_PPSG_CMD_ADJUST_SEC;
	    p.adjust_sec = adjust_sec;
	} else {
      cmd = HEXP_PPSG_CMD_ADJUST_NSEC;
      p.adjust_nsec = adjust_nsec;
  }

    if(!halexp_pps_cmd(cmd, &p))
        return PTPD_NETIF_OK;

    return PTPD_NETIF_ERROR;
}


int ptpd_netif_adjust_phase(int32_t phase_ps)
{
	hexp_pps_params_t p;
    p.adjust_phase_shift = phase_ps;
    if(!halexp_pps_cmd(HEXP_PPSG_CMD_ADJUST_PHASE, &p))
        return PTPD_NETIF_OK;
    return PTPD_NETIF_ERROR;
}

int ptpd_netif_adjust_in_progress()
{
	hexp_pps_params_t p;

    if(halexp_pps_cmd(HEXP_PPSG_CMD_POLL, &p))
        return PTPD_NETIF_OK;
    else
        return PTPD_NETIF_NOT_READY;
}

int ptpd_netif_enable_timing_output(int enable)
{
    hexp_pps_params_t p;

    p.pps_valid = enable;

    if(halexp_pps_cmd(HEXP_PPSG_CMD_SET_VALID, &p))
        return PTPD_NETIF_OK;
    else
        return PTPD_NETIF_NOT_READY;
}

int ptpd_netif_enable_phase_tracking(const char *if_name)
{
  int ret = halexp_lock_cmd(if_name, HEXP_LOCK_CMD_ENABLE_TRACKING, 0);

 	return (ret < 0 ? PTPD_NETIF_ERROR : PTPD_NETIF_OK);
}