// Network API for WR-PTPd

#ifndef __WRAPPER_PRIVATE_H
#define __WRAPPER_PRIVATE_H


#define SOCKQ_SIZE 200
#define MAX_PAYLOAD 100 
#define SOCKS_NUM 3
#define DMTD_UPDATE_INTERVAL 100

extern int32_t sfp_alpha;
extern uint32_t cal_phase_transition;

//PACKED struct etherpacket {
//	struct ethhdr ether;
//	char data[ETHER_MTU];
//};
typedef struct
{
  uint8_t dstmac[6];
  uint8_t srcmac[6];
  uint16_t ethtype;
} ethhdr_t;

typedef struct
{
	uint64_t start_tics;
	uint64_t timeout;
} timeout_t ;

typedef struct
{
  uint8_t buf[SOCKQ_SIZE];
  uint8_t head, tail;
  uint8_t n;
} sockq_t;

struct my_socket {
	int in_use;
	wr_sockaddr_t bind_addr;
	mac_addr_t local_mac;
	int if_index;

	// parameters for linearization of RX timestamps
	uint32_t clock_period;
	uint32_t phase_transition;
	uint32_t dmtd_phase;

	timeout_t dmtd_update_tmo;

  sockq_t queue;
};


#endif
