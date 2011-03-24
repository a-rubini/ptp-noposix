/* net.c */
/*

Here we have some intermediate layer between platform-independent *PTPd* and *ptpd_netif* library
It's not entirely necessary, it is a consequence of the fact that PTPd was adapted from opensource PTPd


*/


#include "../ptpd.h"

#include "ptpd_netif.h"
#include "hal_exports.h"
const mac_addr_t PTP_MULTICAST_ADDR[6] = {0x01, 0x1b, 0x19, 0 , 0, 0};
const mac_addr_t PTP_UNICAST_ADDR[6]   = {0x01, 0x1b, 0x19, 0 , 0, 0};
const mac_addr_t ZERO_ADDR[6]          = {0x00, 0x00, 0x00, 0x00, 0x001, 0x00};

/* shut down the UDP stuff */
Boolean netShutdown(NetPath *netPath)
{

  PERROR("WR: not implemented: %s\n", __FUNCTION__ );
  return TRUE;

#if 0
//original implementation
  struct ip_mreq imr;

  /*Close General Multicast*/
  imr.imr_multiaddr.s_addr = netPath->multicastAddr;
  imr.imr_interface.s_addr = htonl(INADDR_ANY);

  setsockopt(netPath->eventSock, IPPROTO_IP, IP_DROP_MEMBERSHIP, &imr, sizeof(struct ip_mreq));
  setsockopt(netPath->generalSock, IPPROTO_IP, IP_DROP_MEMBERSHIP, &imr, sizeof(struct ip_mreq));

  /*Close Peer Multicast*/
  imr.imr_multiaddr.s_addr = netPath->peerMulticastAddr;
  imr.imr_interface.s_addr = htonl(INADDR_ANY);

  setsockopt(netPath->eventSock, IPPROTO_IP, IP_DROP_MEMBERSHIP, &imr, sizeof(struct ip_mreq));
  setsockopt(netPath->generalSock, IPPROTO_IP, IP_DROP_MEMBERSHIP, &imr, sizeof(struct ip_mreq));


  netPath->multicastAddr = 0;
  netPath->unicastAddr = 0;
  netPath->peerMulticastAddr = 0;

  /*Close sockets*/
  if(netPath->eventSock > 0)
    close(netPath->eventSock);
  netPath->eventSock = -1;

  if(netPath->generalSock > 0)
    close(netPath->generalSock);
  netPath->generalSock = -1;

  return TRUE;
#endif

}

/*Test if network layer is OK for PTP*/
UInteger8 lookupCommunicationTechnology(UInteger8 communicationTechnology)
{


  PERROR("WR: not implemented: %s\n", __FUNCTION__ );
  return PTP_DEFAULT;

//original implementation
#if 0

#if defined(linux)

  switch(communicationTechnology)
  {
  case ARPHRD_ETHER:
  case ARPHRD_EETHER:
  case ARPHRD_IEEE802:
    return PTP_ETHER;

  default:
    break;
  }

#elif defined(BSD_INTERFACE_FUNCTIONS)

#endif

  return PTP_DEFAULT;
#endif
}



Boolean netStartup()
{
  if(ptpd_netif_init() < 0)
    return FALSE;

  return TRUE;
}

/* start all of the UDP stuff */
/* must specify 'subdomainName', optionally 'ifaceName', if not then pass ifaceName == "" */
/* returns other args */
/* on socket options, see the 'socket(7)' and 'ip' man pages */
Boolean netInit(NetPath *netPath, RunTimeOpts *rtOpts, PtpClock *ptpClock)
{
  mac_addr_t portMacAddress[6];
  hexp_port_state_t pstate;

  // Create a PTP socket:
  wr_sockaddr_t bindaddr;



  if(rtOpts->ifaceName[ptpClock->portIdentity.portNumber - 1][0] != '\0')
  {
    /*interface specified at PTPd start*/
    strcpy(bindaddr.if_name, rtOpts->ifaceName[ptpClock->portIdentity.portNumber - 1]);		// TODO: network intarface

    DBG("Network interface : %s\n",rtOpts->ifaceName[ptpClock->portIdentity.portNumber - 1]  );
  }
  else
  {
    /*
      get interface name (port name) for the port
     */
    if(  ptpd_netif_get_ifName(bindaddr.if_name,ptpClock->portIdentity.portNumber ) == PTPD_NETIF_ERROR )
    {
      strcpy(bindaddr.if_name,"wru1");		// TODO: network intarface
      DBG("Network interface forced to be wru1, but none of the WR ports seems to be up \n");
    }
    else
      DBG("Network interface retrieved automatically by ptpd_netif: %s\n",bindaddr.if_name);
  }

  strncpy(netPath->ifaceName,bindaddr.if_name,IFACE_NAME_LENGTH);

  DBG("Network interface : %s\n",netPath->ifaceName);

  bindaddr.family = PTPD_SOCK_RAW_ETHERNET;	// socket type
  bindaddr.ethertype = 0x88f7; 	        // PTPv2
  memcpy(bindaddr.mac, PTP_MULTICAST_ADDR, 6);

  // Create one socket for event and general messages (WR lower level layer requires that
  netPath->wrSock = ptpd_netif_create_socket(PTPD_SOCK_RAW_ETHERNET, 0, &bindaddr);

  if(netPath->wrSock ==  NULL)
  {
    PERROR("failed to initalize sockets");
    return FALSE;
  }

  /* send a uni-cast address if specified (useful for testing) */
  if(rtOpts->unicastAddress[0])
  {
    memcpy(netPath->unicastAddr.mac, PTP_UNICAST_ADDR, 6);
  }
  else
    memcpy(netPath->unicastAddr.mac, ZERO_ADDR, 6);

  memcpy(netPath->multicastAddr.mac, PTP_MULTICAST_ADDR, 6);
  memcpy(netPath->peerMulticastAddr.mac, PTP_MULTICAST_ADDR, 6);

  netPath->unicastAddr.ethertype = 0x88f7;
  netPath->multicastAddr.ethertype = 0x88f7;
  netPath->peerMulticastAddr.ethertype = 0x88f7;

  ptpd_netif_get_hw_addr(netPath->wrSock, portMacAddress);

  /* copy mac part to uuid */
  memcpy(ptpClock->port_uuid_field,portMacAddress, PTP_UUID_LENGTH);

  DBG("[%s] mac: %x:%x:%x:%x:%x:%x\n",__func__,\
    ptpClock->port_uuid_field[0],\
    ptpClock->port_uuid_field[1],\
    ptpClock->port_uuid_field[2],\
    ptpClock->port_uuid_field[3],\
    ptpClock->port_uuid_field[4],\
    ptpClock->port_uuid_field[5]);


  // fixme: error handling
  halexp_get_port_state(&pstate, netPath->ifaceName);


  DBG(" netif_WR_mode = %d\n", pstate.mode);
   if(rtOpts->wrNodeMode == NON_WR)
   {
     switch(pstate.mode)
     {
	case HEXP_PORT_MODE_WR_MASTER:
	   DBG("wrNodeMode(auto config) ....... MASTER\n");
	   ptpClock->wrNodeMode = WR_MASTER;
	   //tmp solution
	   break;
	case HEXP_PORT_MODE_WR_SLAVE:
	   DBG("wrNodeMode(auto config) ........ SLAVE\n");
	   ptpClock->wrNodeMode = WR_SLAVE;
	   ptpd_init_exports();
	   //tmp solution
	   break;
	case HEXP_PORT_MODE_NON_WR:
	default:
	   DBG("wrNodeMode(auto config) ........ NON_WR\n");
	   ptpClock->wrNodeMode = NON_WR;
	   //tmp solution
	   break;
     }
   }else
   {
     ptpClock->wrNodeMode = rtOpts->wrNodeMode;
     DBG("wrNodeMode (............ FORCE ON STARTUP\n");
   }


  DBG("netInit: exiting OK\n");

  return TRUE;

}


/*Check if data have been received*/
int netSelect(TimeInternal *timeout, NetPath *netPath)
{
/*
TODO: ptpd_netif_select improve
*/
   struct timeval tv, *tv_ptr;

  if(timeout)
  {
    tv.tv_sec = timeout->seconds;
    tv.tv_usec = timeout->nanoseconds/1000;
    tv_ptr = &tv;
  }
  else
    tv_ptr = 0;

  return ptpd_netif_select(netPath->wrSock);

}


/*store received data from network to "buf" , get and store the SO_TIMESTAMP value in "time" for an event message*/
ssize_t netRecvMsg(Octet *buf, NetPath *netPath, wr_timestamp_t *current_rx_ts)
{

  wr_sockaddr_t from_addr;
  int ret;

  if((ret = ptpd_netif_recvfrom(netPath->wrSock, &from_addr, buf, 1518, current_rx_ts)) > 0)
  {
    //DBG("RX timestamp %s [ret=%d]\n", format_wr_timestamp(*current_rx_ts), ret);
  }
  return (ssize_t)ret;

}


ssize_t netSendEvent(Octet *buf, UInteger16 length, NetPath *netPath, wr_timestamp_t *current_tx_ts)
{
  int ret;

  //Send a frame

  ret = ptpd_netif_sendto(netPath->wrSock, &netPath->multicastAddr, buf, length, current_tx_ts);

  if(ret <= 0)
    DBGNPI("error sending multi-cast event message\n");

  return (ssize_t)ret;

}

ssize_t netSendGeneral(Octet *buf, UInteger16 length, NetPath *netPath)
{
  wr_timestamp_t ts;

  int ret;
  //Send a frame
  ret = ptpd_netif_sendto(netPath->wrSock, &(netPath->multicastAddr), buf, length, &ts);


  if(ret <= 0)
    DBGNPI("error sending multi-cast event message\n");

  return (ssize_t)ret;


}

ssize_t netSendPeerGeneral(Octet *buf,UInteger16 length,NetPath *netPath)
{

  int ret;

  //Send a frame
  ret = ptpd_netif_sendto(netPath->wrSock, &(netPath->multicastAddr), buf, length, NULL);

  if(ret <= 0)
    DBGNPI("error sending multi-cast general message\n");

  return (ssize_t)ret;

}

ssize_t netSendPeerEvent(Octet *buf,UInteger16 length,NetPath *netPath,wr_timestamp_t *current_tx_ts)
{

  int ret;

  //Send a frame
  // ret = ptpd_netif_sendto(netPath->wrSock, &(netPath->multicastAddr), buf, length, current_tx_ts);

  if(ret <= 0)
    DBGNPI("error sending multi-cast event message\n");

  return (ssize_t)ret;

}
