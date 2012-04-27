/* net.c */
/*

Here we have some intermediate layer between platform-independent *PTPd* and *ptpd_netif* library
It's not entirely necessary, it is a consequence of the fact that PTPd was adapted from opensource PTPd

*/


#include "ptpd.h"

#include "ptpd_netif.h"
#include "hal_exports.h"
const mac_addr_t PTP_MULTICAST_ADDR = {0x01, 0x1b, 0x19, 0 , 0, 0};
const mac_addr_t PTP_UNICAST_ADDR   = {0x01, 0x1b, 0x19, 0 , 0, 0};
const mac_addr_t ZERO_ADDR          = {0x00, 0x00, 0x00, 0x00, 0x001, 0x00};

/*Test if network layer is OK for PTP*/
UInteger8 lookupCommunicationTechnology(UInteger8 communicationTechnology)
{
  /*
   * maybe it would be good to have it for the rabbit ??
   */

//merge problem:  PERROR("WR: not implemented: %s\n", __FUNCTION__ );
  return PTP_DEFAULT;
}

Boolean netStartup()
{
  if(ptpd_netif_init() < 0)
    return FALSE;

  return TRUE;
}

/* start all of the UDP stuff 
* must specify 'subdomainName', optionally 'ifaceName', if not then pass ifaceName == "" 
* returns other args 
* on socket options, see the 'socket(7)' and 'ip' man pages 
* 
* returns True if successful
*/
Boolean netInit(NetPath *netPath, RunTimeOpts *rtOpts, PtpPortDS *ptpPortDS)
{
  mac_addr_t portMacAddress[6];

  // Create a PTP socket:
  wr_sockaddr_t bindaddr;
  
  //port numbers start with 1, but port indexing starts with 0.... a bit of mess
  int port_index = ptpPortDS->portIdentity.portNumber-1;
  if(port_index < 0)
    {
      PTPD_TRACE(TRACE_ERROR, ptpPortDS, "ERROR: port numbering problem, index returned: %d\n", port_index);
      return FALSE;
    }
  ////////////////// establish the network interface name //////////////////////////////
  if(rtOpts->ifaceName[ptpPortDS->portIdentity.portNumber - 1][0] != '\0')
  {
    /*interface specified at PTPd start*/
    strcpy(bindaddr.if_name, rtOpts->ifaceName[ptpPortDS->portIdentity.portNumber - 1]);// TODO: network intarface 

    PTPD_TRACE(TRACE_NET, ptpPortDS,"Network interface : %s\n",rtOpts->ifaceName[ptpPortDS->portIdentity.portNumber - 1]  );
  }
  else
  {
    /*
      get interface name (port name) for the port
     */
    if(  ptpd_netif_get_ifName(bindaddr.if_name,port_index ) == PTPD_NETIF_ERROR )
    {
      strcpy(bindaddr.if_name,"wru1");		// TODO: network intarface
      PTPD_TRACE(TRACE_NET, ptpPortDS,"Network interface (port=%d) forced to be wru1, but none of the WR ports seems to be up \n",ptpPortDS->portIdentity.portNumber);
    }
    else
     PTPD_TRACE(TRACE_NET, ptpPortDS,"Network interface (port=%d) retrieved automatically by ptpd_netif: %s\n",ptpPortDS->portIdentity.portNumber, bindaddr.if_name);

  }

  strncpy(netPath->ifaceName,bindaddr.if_name,IFACE_NAME_LENGTH);

  PTPD_TRACE(TRACE_NET, ptpPortDS,"Network interface : %s\n",netPath->ifaceName);

  bindaddr.family = PTPD_SOCK_RAW_ETHERNET;	// socket type
  bindaddr.ethertype = 0x88f7; 	        // PTPv2
  memcpy(bindaddr.mac, PTP_MULTICAST_ADDR, sizeof(mac_addr_t));

  // Create one socket for event and general messages (WR lower level layer requires that
  netPath->wrSock = ptpd_netif_create_socket(PTPD_SOCK_RAW_ETHERNET, 0, &bindaddr);

  if(netPath->wrSock ==  NULL)
  {
    return FALSE;
  }

  /* send a uni-cast address if specified (useful for testing) */
  if(rtOpts->unicastAddress[0])
  {
    memcpy(netPath->unicastAddr.mac, PTP_UNICAST_ADDR,  sizeof(mac_addr_t));
  }
  else
    memcpy(netPath->unicastAddr.mac, ZERO_ADDR,  sizeof(mac_addr_t));

  memcpy(netPath->multicastAddr.mac, PTP_MULTICAST_ADDR,  sizeof(mac_addr_t));
  memcpy(netPath->peerMulticastAddr.mac, PTP_MULTICAST_ADDR,  sizeof(mac_addr_t));

  netPath->unicastAddr.ethertype = 0x88f7;
  netPath->multicastAddr.ethertype = 0x88f7;
  netPath->peerMulticastAddr.ethertype = 0x88f7;

  ptpd_netif_get_hw_addr(netPath->wrSock, portMacAddress);

  /* copy mac part to uuid */
  memcpy(ptpPortDS->port_uuid_field,portMacAddress, PTP_UUID_LENGTH);
  memcpy(netPath->selfAddr.mac, portMacAddress, 6);

  PTPD_TRACE(TRACE_NET, ptpPortDS,"[%s] mac: %x:%x:%x:%x:%x:%x\n",__func__,\
    ptpPortDS->port_uuid_field[0],\
    ptpPortDS->port_uuid_field[1],\
    ptpPortDS->port_uuid_field[2],\
    ptpPortDS->port_uuid_field[3],\
    ptpPortDS->port_uuid_field[4],\
    ptpPortDS->port_uuid_field[5]);

  ptpPortDS->wrConfig = rtOpts->wrConfig;
  

  PTPD_TRACE(TRACE_NET, ptpPortDS,"netInit: exiting OK\n");

  return TRUE;

}
/*
 * auto detect port's wrConfig 
 *
 * return: TRUE if successful
 */
Boolean autoDetectPortWrConfig(NetPath *netPath, PtpPortDS *ptpPortDS)
{
  hexp_port_state_t pstate;

  //TODO (12): fixme: error handling
  halexp_get_port_state(&pstate, netPath->ifaceName);


  PTPD_TRACE(TRACE_NET, ptpPortDS," netif_WR_mode = %d\n", pstate.mode);

  switch(pstate.mode)
  {

       case HEXP_PORT_MODE_WR_M_AND_S:

	  PTPD_TRACE(TRACE_NET, ptpPortDS,"wrConfig(auto config) ....... MASTER & SLAVE\n");
	  ptpPortDS->wrConfig = WR_M_AND_S;
	  ptpd_init_exports();
	  break;

       case HEXP_PORT_MODE_WR_MASTER:
	   PTPD_TRACE(TRACE_NET, ptpPortDS,"wrConfig(auto config) ....... MASTER\n");
	   ptpPortDS->wrConfig = WR_M_ONLY;
	   break;
	case HEXP_PORT_MODE_WR_SLAVE:
	   PTPD_TRACE(TRACE_NET, ptpPortDS,"wrConfig(auto config) ........ SLAVE\n");
	   ptpPortDS->wrConfig = WR_S_ONLY;
	   ptpd_init_exports();
	   //tmp solution
	   break;
	case HEXP_PORT_MODE_NON_WR:
	  PTPD_TRACE(TRACE_NET, ptpPortDS,"wrConfig(auto config) ........  NON_WR\n");
	  ptpPortDS->wrConfig = NON_WR;
	  break;
	default:
	  PTPD_TRACE(TRACE_NET, ptpPortDS,"wrConfig(auto config) ........ auto detection failed: NON_WR\n");
	  ptpPortDS->wrConfig = NON_WR;
	  return FALSE;
	  break;
   }

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


/*
store received data from network to "buf" , get and store the SO_TIMESTAMP value in "time" for an event message

return: received msg's size
*/
ssize_t netRecvMsg(Octet *buf, NetPath *netPath, wr_timestamp_t *current_rx_ts)
{
    wr_sockaddr_t from_addr;
    int ret, drop = 1;

    ret = ptpd_netif_recvfrom(netPath->wrSock, &from_addr, buf, 1518, current_rx_ts);
    if(ret < 0)
        return ret;

    if( !memcmp(from_addr.mac_dest, netPath->selfAddr.mac, 6) ||
        !memcmp(from_addr.mac_dest, netPath->multicastAddr.mac, 6) ||
        !memcmp(from_addr.mac_dest, netPath->unicastAddr.mac, 6))
            drop = 0;

//    mprintf("Drop: %x\n", drop);

  return drop ? 0 : ret;
}

/*
sending even messages,

return: size of the sent msg
*/
ssize_t netSendEvent(Octet *buf, UInteger16 length, NetPath *netPath, wr_timestamp_t *current_tx_ts)
{
  int ret;

  //Send a frame

  ret = ptpd_netif_sendto(netPath->wrSock, &netPath->multicastAddr, buf, length, current_tx_ts);

  if(ret <= 0)
      PTPD_TRACE(TRACE_ERROR, NULL,"error sending multi-cast event message\n");
  return (ssize_t)ret;

}
/*
sending general messages,

return: size of the sent msg
*/
ssize_t netSendGeneral(Octet *buf, UInteger16 length, NetPath *netPath)
{
  wr_timestamp_t ts;

  int ret;
  //Send a frame
  ret = ptpd_netif_sendto(netPath->wrSock, &(netPath->multicastAddr), buf, length, &ts);


  if(ret <= 0)
      PTPD_TRACE(TRACE_ERROR, NULL,"error sending multi-cast general message\n");

  return (ssize_t)ret;


}

int netEnablePhaseTracking(NetPath *netPath)
{

  int ret;
  ret = ptpd_netif_enable_phase_tracking(netPath->ifaceName);


	return ret;

}

/*
sending Peer Generals messages,

return: size of the sent msg
*/
ssize_t netSendPeerGeneral(Octet *buf,UInteger16 length,NetPath *netPath)
{

  int ret;

  //Send a frame
  ret = ptpd_netif_sendto(netPath->wrSock, &(netPath->multicastAddr), buf, length, NULL);

  if(ret <= 0)
    PTPD_TRACE(TRACE_ERROR, NULL,"error sending multi-cast peer general message\n");
  
  return (ssize_t)ret;

}
/*
sending Peer Events messages,

return: size of the sent msg
*/
ssize_t netSendPeerEvent(Octet *buf,UInteger16 length,NetPath *netPath,wr_timestamp_t *current_tx_ts)
{

  int ret;

  //Send a frame
  ret = ptpd_netif_sendto(netPath->wrSock, &(netPath->multicastAddr), buf, length, current_tx_ts);

  if(ret <= 0)
      PTPD_TRACE(TRACE_ERROR, NULL,"error sending multi-cast peer event message\n");
  return (ssize_t)ret;

}
UInteger16 autoPortNumberDiscovery(void)
{
	char dummy[16];

	int portNumber = 0;
	for(;;)
	{
		
 		if(ptpd_netif_get_ifName(dummy, portNumber) == PTPD_NETIF_OK)
 		{
 			portNumber++;
 		}
 		else
 			break;
 	
	}
	return (UInteger16)portNumber;
}
/* 
 * function checks whetehr the PLL is locked to an external source (e.x. GPS).
 * OK, it's not the best place for this function, if you find a better place, move it there
 * 
 * return : 	TRUE if locked
 *		FLASE if not locked
 */
Boolean extsrcLocked(void)
{
   if(ptpd_netif_extsrc_detection() == PTPD_NETIF_OK)
      return TRUE;
    else
      return FALSE;
}
