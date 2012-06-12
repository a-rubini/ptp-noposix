/* wr_protocol.c */

/*
  by Maciej Lipinski
  all complaints to be sent to maciej.lipinski@cern.ch

  in this file:
  - implementatinon of WR FSMs (according to wrspc )
  - implementation of WR timeouts
  - WR HW timestamps functions

  have F U N :)

the state machines are "specific",
- main states are changed using toWRState() function
- substates are changed without toWRState()
- timeouts are started/stopped when entering/exiting states (the end of the function)
- substates were implemented to accommodate HW exceptions and
  repetition of HW access functions in such case
  (you need to remember that everything happens in gaint LOOP)

the structure is following

WR  FSM:
|  main state 		|  	substate 	|
|-------------------------------------------------
|WRS_PRESENT  		|			|
|			|			|
|WRS_S_LOCK		|			|
|			|WRS_S_LOCK_1		|
|			|WRS_S_LOCK_2		|
|			|			|
|WRS_M_LOCK		|			|
|			|			|
|WRS_LOCKED		|			|
|			|			|
|WRS_CALIBRATION	|			|
|			|WRS_CALIBRATION_1	|
|			|WRS_CALIBRATION_2	|
|			|WRS_CALIBRATION_3	|
|			|WRS_CALIBRATION_4	|
|			|WRS_CALIBRATION_5	|
|			|WRS_CALIBRATION_6	|
|			|WRS_CALIBRATION_7	|
|			|WRS_CALIBRATION_8	|
|			|			|
|WRS_RESP_CALIB_REQ	|			|
|			|WRS_RESP_CALIB_REQ_1	|
|			|WRS_RESP_CALIB_REQ_2	|
|			|WRS_RESP_CALIB_REQ_3	|
|			|			|
|WRS_CALIBRATED		|			|
|			|			|
|WRS_WR_LINK_ON		|			|
-------------------------------------------------


*/

#include "ptpd.h"

/*
  this function returns TRUE if the port is up (cable connected)
*/
Boolean isPortUp(NetPath *netPath)
{

  Boolean rv= ptpd_netif_get_port_state(netPath->ifaceName) == PTPD_NETIF_OK ? TRUE : FALSE;
  return rv;
}

/*
this function comes as a consequence of implementing substates.
it returns the main state currently being executed
*/
UInteger8 returnCurrentWRMainState( PtpPortDS *ptpPortDS)
{
  /*
   * this (exitingState) have to do with substates and timeouts,
   * if we exit one of substates, the value of exiting state
   * which is inputed into wrTimetoutManage() needs to be of
   * the main state since we calculate timeouts for main states
   */
  UInteger8 state;
  /* leaving state tasks */
  switch(ptpPortDS->wrPortState)
  {
  case WRS_IDLE:
    state = WRS_IDLE;
    break;

  case WRS_PRESENT:
    state = WRS_PRESENT;
    break;

  case WRS_S_LOCK:
  case WRS_S_LOCK_1:
  case WRS_S_LOCK_2:

    state = WRS_S_LOCK;
    break;

  case WRS_M_LOCK:

    state = WRS_M_LOCK;
    break;

   case WRS_LOCKED:

    state = WRS_LOCKED;
    break;

   case WRS_CALIBRATION:
   case WRS_CALIBRATION_1:
   case WRS_CALIBRATION_2:
   case WRS_CALIBRATION_3:
   case WRS_CALIBRATION_4:
   case WRS_CALIBRATION_5:
   case WRS_CALIBRATION_6:
   case WRS_CALIBRATION_7:
   case WRS_CALIBRATION_8:
     

     state = WRS_CALIBRATION;
     break;

   case WRS_CALIBRATED:

     state = WRS_CALIBRATED;
     break;

   case WRS_RESP_CALIB_REQ:
   case WRS_RESP_CALIB_REQ_1:
   case WRS_RESP_CALIB_REQ_2:
   case WRS_RESP_CALIB_REQ_3:

     state = WRS_RESP_CALIB_REQ;
     break;
   case WRS_WR_LINK_ON:

     state = WRS_WR_LINK_ON;

     break;
    default:

     state = ptpPortDS->wrPortState;
     break;
  }

  return state;
}

/*
this function manages WR timeouts, each main state (except IDLE) has timeout
timeouts are "automatically" started/stopped on the transition of main states
by this function
here we also count the number of attempt on the same state, if the retry number
exccedded, we exit WR FSM, no WRPTP, sorry
*/
void wrTimetoutManage(UInteger8 enteringState, UInteger8 exitingState, RunTimeOpts *rtOpts, PtpPortDS *ptpPortDS)
{

  /*
    checking if the state is repeated,
    repeated state means that there was timeout
    so we need to increase repetition counter
  */
  if(enteringState != exitingState)
    ptpPortDS->currentWRstateCnt = 0;
  else
    ptpPortDS->currentWRstateCnt++;

  /*stop time from the state you are leaving (except IDLE)*/
  if(exitingState != WRS_IDLE)
    timerStop(&ptpPortDS->wrTimers[exitingState]);

  /*start timer in the state you are entering (except IDLE) */
  if(enteringState != WRS_IDLE)
    timerStart(&ptpPortDS->wrTimers[enteringState], ptpPortDS->wrTimeouts[enteringState]);
}

/*
this function checks if wr timer has expired for a current WR state
*/
void wrTimerExpired(UInteger8 currentState, RunTimeOpts *rtOpts, PtpPortDS *ptpPortDS, Enumeration8 wrMode)
{
  UInteger8 wrStateRetry;
  /*WRS_IDLE state does not expire */
  if(currentState == WRS_IDLE)
    return;
  
  if(timerExpired(&ptpPortDS->wrTimers[currentState]))
  {

      if(currentState == WRS_CALIBRATION && ptpPortDS->calRetry > 0)
	wrStateRetry = ptpPortDS->calRetry;
      else if(currentState == WRS_RESP_CALIB_REQ && ptpPortDS->otherNodeCalRetry > 0)
	wrStateRetry = ptpPortDS->otherNodeCalRetry;
      else
	wrStateRetry = ptpPortDS->wrStateRetry;
	
      if (ptpPortDS->currentWRstateCnt < wrStateRetry)
      {
	PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS, "WR_Slave_TIMEOUT: state[= %d] timeout, repeat state\n", currentState);
	toWRState(currentState, rtOpts, ptpPortDS);
      }
      else
      {
//	PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"WR_Slave_TIMEOUT: state[=%d] timeout, repeated %d times, going to Standard PTP\n", \
	currentState,ptpPortDS->currentWRstateCnt );
	
	ptpPortDS->wrModeON = FALSE;
        toWRState(WRS_IDLE, rtOpts, ptpPortDS);

	if(rtOpts->disableFallbackIfWRFails)
	{
		PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"WR_Slave_TIMEOUT: state[=%d] timeout, disabling port (standard PTP fallback OFF).\n", currentState);

	  toState(PTP_DISABLED, rtOpts, ptpPortDS);
	} else if(wrMode == WR_MASTER)
	  toState(PTP_MASTER, rtOpts, ptpPortDS);
	else toState(PTP_SLAVE, rtOpts, ptpPortDS);
	/*
	 * RE-INITIALIZATION OF White Rabbit Data Sets
	 * (chapter (Re-)Initialization of wrspec
	 */	
	initWrData(ptpPortDS, INIT); //INIT mode because we don't need to remember WR port mode and port role
      }

  }
}


/* for printing bits of the pattern*/
char *printf_bits(UInteger32 bits)
{
  static char buf[33];
  int i;
  for(i = 0; i < 32; i++)
    if((bits >> i) & 0x1)
      buf[31 - i] = '1';
    else
      buf[31 - i] = '0';

  buf[i]='\0';

  return buf;
}


/*prints RAW timestamp*/
#ifndef WRPC_EXTRA_SLIM
char *format_wr_timestamp(wr_timestamp_t ts)
{
  static char ts_buf[64];

  snprintf(ts_buf,64, "sec: %lld nsec: %ld ", (long long)ts.sec,
	   (long) ts.nsec);

  return ts_buf;
}
#endif




/* handle actions and events for 'wrPortState' */

void doWRState(RunTimeOpts *rtOpts, PtpPortDS *ptpPortDS)
{
  /*
   * here WR Slave FSM is implemented, please note that the implementation is
   * "unusual" (or nasty). there are substates to include the cases when
   * HW is not cooperative (returns: "no, I don't want to do that" and
   * we need to ask HW again, again and again, therefore substates
   *
   */
  uint64_t delta;
  UInteger8 currentState;
  
  
  switch(ptpPortDS->wrPortState)
  {

  case WRS_IDLE:

    //do nothing

    break;

  /**********************************  PRESENT  ***************************************************************************/
  case WRS_PRESENT:
    /*
     * message S_PRESENT sent to Master while entering state (toWRState())
     * here we wait for the answer from the Master asking us to LOCK
     */
    
    handle(rtOpts, ptpPortDS);


    if(ptpPortDS->msgTmpWrMessageID == LOCK)
    {
      toWRState(WRS_S_LOCK, rtOpts, ptpPortDS);
      
      // management message used, so clean tmp
      ptpPortDS->msgTmpWrMessageID = NULL_WR_TLV;
    }

 
    break;
  /**********************************  S_LOCK  ***************************************************************************/
  case WRS_S_LOCK:


	//substate 0  	- locking_enable failed when called while entering this state (toWRState()) so we
	//		  we need to try again

	/* depending what kind of slave the port is, different locking is used ("slave's role" is 
	   decided by the modifiedBMC) */
	if(ptpPortDS->wrSlaveRole == PRIMARY_SLAVE)
	{
	   PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"locking primary slave\n");
	   if(ptpd_netif_locking_enable(ptpPortDS->wrMode, ptpPortDS->netPath.ifaceName, SLAVE_PRIORITY_0) == PTPD_NETIF_OK )
	      ptpPortDS->wrPortState = WRS_S_LOCK_1;
	}
	else if(ptpPortDS->wrSlaveRole == SECONDARY_SLAVE)
	{
	   PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"locking secondary slave\n");
	   //TODO(2): make for more secondary slaves
	   if(ptpd_netif_locking_enable(ptpPortDS->wrMode, ptpPortDS->netPath.ifaceName, SLAVE_PRIORITY_1) == PTPD_NETIF_OK )
	      ptpPortDS->wrPortState = WRS_S_LOCK_1;	
	}
	else
	{
	    PTPD_TRACE(TRACE_ERROR, ptpPortDS,"ERROR: Should not get here, trying to lock not a slave port\n");
	}

	//check if enabling() succeeded
	if( ptpPortDS->wrPortState == WRS_S_LOCK_1)
	    PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"LockingSuccess\n") //if yes, no break, go ahead
	else
	    break; //try again

      //substate 1 	- polling HW
      case WRS_S_LOCK_1:
	if(ptpPortDS->wrSlaveRole == PRIMARY_SLAVE)
	{
	    if(ptpd_netif_locking_poll(ptpPortDS->wrMode, ptpPortDS->netPath.ifaceName, SLAVE_PRIORITY_0) == PTPD_NETIF_READY)
	      ptpPortDS->wrPortState = WRS_S_LOCK_2; //next level achieved
	    else
	      break; //try again
	}
	else if(ptpPortDS->wrSlaveRole == SECONDARY_SLAVE)
	{
	    //TODO(2): more secondary slaves here
	    if(ptpd_netif_locking_poll(ptpPortDS->wrMode, ptpPortDS->netPath.ifaceName, SLAVE_PRIORITY_1) == PTPD_NETIF_READY)
	      ptpPortDS->wrPortState = WRS_S_LOCK_2; //next level achieved
	    else
	      break; //try again  
	}  
	else
	{
	    PTPD_TRACE(TRACE_ERROR, ptpPortDS,"ERROR: Should not get here, trying to lock not slave port\n");
	    break; //try again
	}
	
	// no break, go ahead
	 
      //substate 2 	-disabling locking (not really implemented in HW)
      case WRS_S_LOCK_2:
	
	if(ptpPortDS->wrSlaveRole == PRIMARY_SLAVE)
	{
	    if(ptpd_netif_locking_disable(ptpPortDS->wrMode, ptpPortDS->netPath.ifaceName, SLAVE_PRIORITY_0) == PTPD_NETIF_OK)
	      toWRState(WRS_LOCKED, rtOpts, ptpPortDS); 
	    else
	      break; //try again  
	}
	else if(ptpPortDS->wrSlaveRole == SECONDARY_SLAVE)
	{
	    //TODO(2): more secondary slaves here
	    if(ptpd_netif_locking_disable(ptpPortDS->wrMode, ptpPortDS->netPath.ifaceName, SLAVE_PRIORITY_1) == PTPD_NETIF_OK)
	      toWRState(WRS_LOCKED, rtOpts, ptpPortDS);
	    else
	      break; //try again  	    
	}  
	else
	{
	    PTPD_TRACE(TRACE_ERROR, ptpPortDS,"ERROR: Should not get here, trying to lock not slave port\n");
	    break;
	}
	  
	break; //this is needed here, just in case, we separate WR States  
  /**********************************  M_LOCK  ***************************************************************************/
  case WRS_M_LOCK:

	  handle(rtOpts, ptpPortDS);

	  if(ptpPortDS->msgTmpWrMessageID == LOCKED)
	  {
	    toWRState(WRS_CALIBRATION, rtOpts, ptpPortDS);
	  }
	  break;
  /**********************************  LOCKED  ***************************************************************************/
  case WRS_LOCKED:


	   handle(rtOpts, ptpPortDS);
	   if(ptpPortDS->msgTmpWrMessageID == CALIBRATE)
	     toWRState(WRS_RESP_CALIB_REQ, rtOpts, ptpPortDS);

	   break;

  /**********************************  WRS_CALIBRATION  ***************************************************************************/
  case WRS_CALIBRATION:
	//substate 0	- first attempt to start calibration was while entering state (toWRState())
	//		  here we repeat if faild before

	// first we start calibration pattern
	    if(ptpd_netif_calibration_pattern_enable(ptpPortDS->netPath.ifaceName, 0, 0, 0) == PTPD_NETIF_OK)
	      ptpPortDS->wrPortState = WRS_CALIBRATION_1;
	    else
	      break; // go again

	    // no break here
	// then we start calibration of the port's Tx     
	case WRS_CALIBRATION_1:

	    if(ptpd_netif_calibrating_enable(PTPD_NETIF_TX, ptpPortDS->netPath.ifaceName) == PTPD_NETIF_OK)
	      ptpPortDS->wrPortState = WRS_CALIBRATION_2; // go to substate 1
	    else
	      break; // again

	    // no braek here
	    
	//we wait until the calibration is finished
	case WRS_CALIBRATION_2:
	    
	    if(ptpd_netif_calibrating_poll(PTPD_NETIF_TX, ptpPortDS->netPath.ifaceName,&delta) == PTPD_NETIF_READY)
	    {
		ptpPortDS->deltaTx.scaledPicoseconds.msb = 0xFFFFFFFF & (delta >> 16);
		ptpPortDS->deltaTx.scaledPicoseconds.lsb = 0xFFFFFFFF & (delta << 16);
		PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"Tx=>>scaledPicoseconds.msb = 0x%x\n",ptpPortDS->deltaTx.scaledPicoseconds.msb);
	        PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"Tx=>>scaledPicoseconds.lsb = 0x%x\n",ptpPortDS->deltaTx.scaledPicoseconds.lsb);
	
		ptpPortDS->wrPortState = WRS_CALIBRATION_3;
	    }
	    else
		break; // again

	// now we disable port's Tx calibration
	case WRS_CALIBRATION_3:
	    
    
	    if(ptpd_netif_calibrating_disable(PTPD_NETIF_TX, ptpPortDS->netPath.ifaceName) == PTPD_NETIF_OK)
		ptpPortDS->wrPortState = WRS_CALIBRATION_4;
	    else
		break; // again

	// we disable the pattern
	case WRS_CALIBRATION_4:

	    if(ptpd_netif_calibration_pattern_disable(ptpPortDS->netPath.ifaceName) == PTPD_NETIF_OK)
		ptpPortDS->wrPortState = WRS_CALIBRATION_5;
	    else
		break; // again    
    
	// now we go to the calibration of Rx using the pattern send by the other port, enable Rx calibration
	case WRS_CALIBRATION_5:
	    
	    if(ptpd_netif_calibrating_enable(PTPD_NETIF_RX, ptpPortDS->netPath.ifaceName) == PTPD_NETIF_OK)
	      ptpPortDS->wrPortState = WRS_CALIBRATION_6;
	    else
	      break; //try again

	//check whether Rx calibration is finished
	case WRS_CALIBRATION_6:

	    if(ptpd_netif_calibrating_poll(PTPD_NETIF_RX, ptpPortDS->netPath.ifaceName,&delta) == PTPD_NETIF_READY)
	    {
	      PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"RX fixed delay = %d\n",(int)delta);
	      ptpPortDS->deltaRx.scaledPicoseconds.msb = 0xFFFFFFFF & (delta >> 16);
	      ptpPortDS->deltaRx.scaledPicoseconds.lsb = 0xFFFFFFFF & (delta << 16);
	      PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"Rx=>>scaledPicoseconds.msb = 0x%x\n",ptpPortDS->deltaRx.scaledPicoseconds.msb);
	      PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"Rx=>>scaledPicoseconds.lsb = 0x%x\n",ptpPortDS->deltaRx.scaledPicoseconds.lsb);

	      ptpPortDS->wrPortState = WRS_CALIBRATION_7;
	    }
	    else
	      break; //try again
	      
	// disable Rx calibration 
	case WRS_CALIBRATION_7:

	    if( ptpd_netif_calibrating_disable(PTPD_NETIF_RX, ptpPortDS->netPath.ifaceName) == PTPD_NETIF_OK)
	      ptpPortDS->wrPortState = WRS_CALIBRATION_8;
	    else
	      break; // try again
	    
	 // send deltas to the other port and go to the next state   
	 case WRS_CALIBRATION_8:

	    issueWRSignalingMsg(CALIBRATED,rtOpts, ptpPortDS);
   
	    toWRState(WRS_CALIBRATED, rtOpts, ptpPortDS);
	    ptpPortDS->calibrated = TRUE;	    


    break;
  /**********************************  WRS_CALIBRATED  ***************************************************************************/
  case WRS_CALIBRATED:
	    handle(rtOpts, ptpPortDS);

	    if(ptpPortDS->msgTmpWrMessageID == CALIBRATE && ptpPortDS->wrMode == WR_MASTER)
	      toWRState(WRS_RESP_CALIB_REQ, rtOpts, ptpPortDS);
	    
	    if(ptpPortDS->msgTmpWrMessageID == WR_MODE_ON && ptpPortDS->wrMode == WR_SLAVE)
	      toWRState(WRS_WR_LINK_ON, rtOpts, ptpPortDS);
	    break;

/**********************************  WRS_RESP_CALIB_REQ  ***************************************************************************/
  case WRS_RESP_CALIB_REQ:

	  if( ptpd_netif_calibration_pattern_enable( 	ptpPortDS->netPath.ifaceName, \
							ptpPortDS->otherNodeCalPeriod, \
							0, \
							0) == PTPD_NETIF_OK)
	  {
	    ptpPortDS->wrPortState = WRS_RESP_CALIB_REQ_1; //go to substate 1
	  }
	  else
	  {
	    break;   //try again
	  }

      //substate 1	- waiting for instruction from the master
      case WRS_RESP_CALIB_REQ_1 :

	    handle(rtOpts, ptpPortDS);
	    if(ptpPortDS->msgTmpWrMessageID == CALIBRATED)
	    {
	      if(ptpPortDS->otherNodeCalSendPattern ==  TRUE)
		ptpPortDS->wrPortState = WRS_RESP_CALIB_REQ_2;
	      else
		ptpPortDS->wrPortState = WRS_RESP_CALIB_REQ_3;
	    }
	    else
	      break; // try again

      //substate 2	- so the master finished, so we try to disable pattern, repeat if failed
      case WRS_RESP_CALIB_REQ_2 :

	     if(ptpd_netif_calibration_pattern_disable(ptpPortDS->netPath.ifaceName) == PTPD_NETIF_OK)
		ptpPortDS->wrPortState = WRS_RESP_CALIB_REQ_3;
	     else
		break; // try again

      case WRS_RESP_CALIB_REQ_3:

	  if(ptpPortDS->wrMode == WR_MASTER)
	    toWRState(WRS_WR_LINK_ON, rtOpts, ptpPortDS);
	  else if(ptpPortDS->wrMode == WR_SLAVE)
	    toWRState(WRS_CALIBRATION, rtOpts, ptpPortDS);
	  else
	  {
	    PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"ERRRORROR!!!!!!!!!! : WRS_RESP_CALIB_REQ_3\n");
	    toWRState(WRS_IDLE, rtOpts, ptpPortDS);
	   }

	break;

  /**********************************  WRS_WR_LINK_ON ***************************************************************************/
  case WRS_WR_LINK_ON:
	    /*
	     * While entering the state, we sent WR_MODE_ON to the Master and set wrModeON TRUE and assuming that Master
	     * is calibrated, set grandmaster to wrModeON and calibrated, see (toWRState())
	     */
	    toWRState(WRS_IDLE, rtOpts, ptpPortDS);
	    if(ptpPortDS->wrMode == WR_SLAVE)
	      /* 
	       * this is MASTER_CLOCK_SELECTED event defined in PTP in 9.2.6.13
	       */
	      toState(PTP_SLAVE, rtOpts, ptpPortDS); 
	    else if(ptpPortDS->wrMode == WR_MASTER)
	      toState(PTP_MASTER, rtOpts, ptpPortDS);
	    else
	      PTPD_TRACE(TRACE_ERROR, ptpPortDS,"ERROR: WRS_WR_LINK_ON !!!\n");
	    break;



   /**********************************  default  ***************************************************************************/
  default:

	    PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"(doWhiteRabbitState) do unrecognized state\n");
	    break;
  }

  /*
   * we need to know "main state" to check if the timer expired,
   * timeouts are measured for main states only
   */
  currentState = returnCurrentWRMainState(ptpPortDS);

  /* handling timeouts globally, may chage state*/
  wrTimerExpired(currentState,rtOpts,ptpPortDS,ptpPortDS->wrMode);

}



/* perform actions required when leaving 'wrPortState' and entering 'state' */
void toWRState(UInteger8 enteringState, RunTimeOpts *rtOpts, PtpPortDS *ptpPortDS)
{

  /*
   * this (exitingState) have to do with substates and timeouts,
   * if we exit one of substates, the value of exiting state
   * which is inputed into wrTimetoutManage() needs to be of
   * the main state since we calculate timeouts for main states
   */
  UInteger8 exitingState = returnCurrentWRMainState(ptpPortDS);

  /* leaving state tasks */
  switch(ptpPortDS->wrPortState)
  {
  case WRS_IDLE:
      
    
    /*
     * RE-INITIALIZATION OF White Rabbit Data Sets
     * (chapter (Re-)Initialization of wrspec
     *
     */
    initWrData(ptpPortDS, RE_INIT);

    
    
    PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"exiting WRS_IDLE\n");
    PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"^^^^^^^^^^^^^^^^ starting White Rabbit State Machine^^^^^^^^^^^^^^^^^^\n");
    PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"\n");
    PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"\n");
    PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"\n");
    PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"\n");
    if(ptpPortDS->wrMode== WR_MASTER)
    PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"                            W R   M A S T E R\n")
    else if(ptpPortDS->wrMode== WR_SLAVE)
    PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"                             W R   S L A V E \n")
    else
    PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"                        I SHOULD NOT SHOW THIS MSG !!! \n");
    PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"\n");
    PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"\n");
    PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"\n");
    PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"\n");    
    break;

  case WRS_PRESENT:
    PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"exiting WRS_PRESENT\n");
    break;

  case WRS_S_LOCK:
    PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"exiting WRS_S_LOCK\n");    
  case WRS_S_LOCK_1:
  case WRS_S_LOCK_2:
    break;

  case WRS_M_LOCK:
    PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"exiting WRS_M_LOCK\n");    
     break;

  case WRS_LOCKED:
    PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"exiting WRS_LOCKED\n");    
     break;

   case WRS_CALIBRATION:
    PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"exiting WRS_CALIBRATION\n");     
   case WRS_CALIBRATION_1:
   case WRS_CALIBRATION_2:
          
     break;

   case WRS_CALIBRATED:
     PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"exiting WRS_CALIBRATED\n");     
     break;

   case WRS_WR_LINK_ON:
     PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"\n");
     PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"exiting WRS_WR_LINK_ON\n");
     PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"^^^^^^^^^^^^^^^^^^^^^^^^^ WR Link is ON ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
     PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"\n");
     PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"\n");
     PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"\n");
     PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"\n");
     PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"\n");
		

     break;

  default:

    break;
  }



  /* entering state tasks */
  switch(enteringState)
  {
  case WRS_IDLE:
    /* no substates here*/
    PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"entering WRS_IDLE\n");

    ptpPortDS->wrPortState = WRS_IDLE;
    break;

  case WRS_PRESENT:
    /* no substates here*/
    PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"entering  WRS_PRESENT\n");
    /*send message to the Master to enforce entering UNCALIBRATED state*/
    
    issueWRSignalingMsg(SLAVE_PRESENT,rtOpts, ptpPortDS);
    ptpPortDS->wrPortState = WRS_PRESENT;
    break;

  case WRS_S_LOCK:
    /* LOCK state implements 3 substates:
     * 0 - enable locking
     * 1 - locking enabled, polling
     * 2 - locked, disabling locking
     */
    PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"entering  WR_LOCK (modded?)\n");


	/* depending what kind of slave the port is, different locking is used ("slave's role" is 
	   decided by the modifiedBMC) */
	if(ptpPortDS->wrSlaveRole == PRIMARY_SLAVE)
	{
	   PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"locking primary slave\n");
	   if(ptpd_netif_locking_enable(ptpPortDS->wrMode, ptpPortDS->netPath.ifaceName, SLAVE_PRIORITY_0) == PTPD_NETIF_OK )
	      ptpPortDS->wrPortState = WRS_S_LOCK_1;  //go to substate 1
	   else
	      ptpPortDS->wrPortState = WRS_S_LOCK;   //stay in substate 0, try again
	}
	else if(ptpPortDS->wrSlaveRole == SECONDARY_SLAVE)
	{
	   PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"locking secondary slave\n");
	   //TODO(2): make for more secondary slaves
	   if(ptpd_netif_locking_enable(ptpPortDS->wrMode, ptpPortDS->netPath.ifaceName, SLAVE_PRIORITY_1) == PTPD_NETIF_OK )
	      ptpPortDS->wrPortState = WRS_S_LOCK_1;	  //go to substate 1
	   else
	      ptpPortDS->wrPortState = WRS_S_LOCK;   //stay in substate 0, try again
	}
	else
	{
	    PTPD_TRACE(TRACE_ERROR, ptpPortDS,"ERROR: Should not get here, trying to lock not slave port\n");
	}

    break;

  case WRS_LOCKED:
    /* no substates here*/
    PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"entering  WR_LOCKED\n");

    /* say to the Master that you are locked */
    issueWRSignalingMsg(LOCKED,rtOpts, ptpPortDS);
    ptpPortDS->wrPortState = WRS_LOCKED;
    break;

  case WRS_M_LOCK:
    /* no substates here*/
    PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"entering  WRS_M_LOCK\n");
    issueWRSignalingMsg(LOCK,rtOpts, ptpPortDS);
    ptpPortDS->wrPortState = WRS_M_LOCK;
    break;


   case WRS_CALIBRATION: 
     
    /* WRS_CALIBRATION state implements 8 substates:
     * 0 - enable pattern sending
     * 1 - Tx calibration
     * 2 - Tx calibration enabled, polling Tx
     * 3 - disable Tx calibration
     * 4 - disable pattern sending
     * 5 - enable Rx calibration (receiving pattern from the other port)
     * 6 - Rx calibration enabled, polling Rx
     * 7 - disable Rx calibration
     * 8 - send CALIBRATED message and enter next state
     */

    PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"entering  WRS_CALIBRATION\n");

    issueWRSignalingMsg(CALIBRATE,rtOpts, ptpPortDS);

    if(ptpPortDS->calPeriod > 0)
    {
       ptpPortDS->wrTimeouts[WRS_CALIBRATION]   = ptpPortDS->calPeriod;
       PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"set wrTimeout of WRS_CALIBRATION based on calPeriod:  %u [us]\n", ptpPortDS->calPeriod);
    }
    else
       ptpPortDS->wrTimeouts[WRS_CALIBRATION]   = ptpPortDS->wrStateTimeout;
 
  
    if( ptpPortDS->calibrated == TRUE)
    {
      /*
       * NO CALIBRATION NEEDED !!!!!
       * just go to the last step of this state
       * which is going to WRS_CALIBRATED
       */
      ptpPortDS->wrPortState = WRS_CALIBRATION_2; // go to substate 1
      
      break;
    }
    
    // enable pattern sending
    if(ptpd_netif_calibration_pattern_enable(ptpPortDS->netPath.ifaceName, 0, 0, 0) == PTPD_NETIF_OK)
    {
	// enable Tx calibration
	if(ptpd_netif_calibrating_enable(PTPD_NETIF_TX, ptpPortDS->netPath.ifaceName) == PTPD_NETIF_OK)
	    ptpPortDS->wrPortState = WRS_CALIBRATION_2; 
	else
	    ptpPortDS->wrPortState = WRS_CALIBRATION_1;
    }
    else
    {
      //crap, probably calibration module busy with
      //calibrating other port, repeat attempt to enable calibration      
      ptpPortDS->wrPortState = WRS_CALIBRATION;    
    }
    

    break;

  case WRS_CALIBRATED:
    PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"entering  WRS_CALIBRATED\n");

    ptpPortDS->wrPortState = WRS_CALIBRATED;
    break;

  case WRS_RESP_CALIB_REQ:
    /* M_CALIBRATE state implements 3 substates:
     * 0 - enable pattern sending
     * 1 - pattern enabled, waiting for the CALIBRATED message from the other port
     * 2 - CALIBRATED received, disabling pattern
     * 3 - Go to the next state
     */
    PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"entering  WRS_RESP_CALIB_REQ\n");

    // to send the pattern or not to send
    // here is the answer to the question.....
    if(ptpPortDS->otherNodeCalSendPattern == TRUE)
    {
      /*
       * the other node needs calibration, so
       * turn on calibration pattern, if the calibration perios
       * is sent, set it 
       */
	if(ptpPortDS->otherNodeCalPeriod > 0)
	{
	    ptpPortDS->wrTimeouts[WRS_RESP_CALIB_REQ]   = ptpPortDS->otherNodeCalPeriod;
	    PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"set wrTimeout of WRS_RESP_CALIB_REQ based on calPeriod:  %u [us]\n", \
	    ptpPortDS->otherNodeCalPeriod);
	}
	else
	    ptpPortDS->wrTimeouts[WRS_RESP_CALIB_REQ]   = ptpPortDS->wrStateTimeout;
	
	if( ptpd_netif_calibration_pattern_enable( ptpPortDS->netPath.ifaceName, \
				ptpPortDS->otherNodeCalPeriod, \
				0, 0) == PTPD_NETIF_OK)
	{
	  ptpPortDS->wrPortState = WRS_RESP_CALIB_REQ_1; 
	}
	else
	{
	  ptpPortDS->wrPortState = WRS_RESP_CALIB_REQ;   //try again
	}

    }
    else
    {
      /*
       * the other node knows its fixed delays(deltaRx and deltaTx)
       * go straight to step 2 of this state: wait for CALIBRATED message
       */
	ptpPortDS->wrPortState = WRS_RESP_CALIB_REQ_1; //go to substate 1
    }
    break;

  case WRS_WR_LINK_ON:
    PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"entering  WRS_LINK_ON\n");

    ptpPortDS->wrModeON 	= TRUE;
    
    netEnablePhaseTracking(&ptpPortDS->netPath);
    
    if(ptpPortDS->wrMode == WR_MASTER)
      issueWRSignalingMsg(WR_MODE_ON,rtOpts, ptpPortDS);
    /*Assume that Master is calibrated and in WR mode, it will be verified with the next Annonce msg*/
    ptpPortDS->parentWrModeON     = TRUE;
    
    /* 
     * this is nasty, we need to update the announce messaegs, because it is used by s1() in 
     * bmc() to update parentWrModeON, which is (in turn) used in the condition to 
     * to trigger SYNCHRONIZATION_FAULT.
     * the problem is, that it takes few bmc() executions before new Announce message is received
     * from the WR Master, there are these executions because bmc() is executed always for all
     * ports, so even if this port has not received new Announce message, some other could have
     * received it and bmc() is executed on all ports
     */
    ptpPortDS->foreign[ptpPortDS->foreign_record_best].announce.wr_flags = \
	ptpPortDS->foreign[ptpPortDS->foreign_record_best].announce.wr_flags | WR_IS_WR_MODE;
    
    ptpPortDS->parentCalibrated = TRUE;

    ptpPortDS->wrPortState = WRS_WR_LINK_ON;
    break;

  default:
    PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"to unrecognized state\n");
    break;
  }

  /******** WR TIMEOUT STAFF **********
   * turn of timeout of exitingState
   * turn out timeout of enteringState,
   * called at the end, since we set timeouts of 
   * WRS_RESP_CALIB_REQ and WRS_CALIBRATION states
   */
  wrTimetoutManage(enteringState,exitingState,rtOpts,ptpPortDS);

}

/*
  It initializes White Rabbit dynamic data fields as 
  defined in the WRSPEC, talbe 1
  Called in the places defined in the WRSpec, (Re-)Initialization 
  section
*/
void initWrData(PtpPortDS *ptpPortDS, Enumeration8 mode)
{
  uint64_t d_tx, d_rx;

  PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"White Rabbit data (re-)initialization\n");
  int i=0;
  //ptpPortDS->wrMode 			   = NON_WR;
  ptpPortDS->wrModeON    		   = FALSE;
  ptpPortDS->wrPortState 		   = WRS_IDLE;
  ptpPortDS->calibrated  		   = !ptpPortDS->phyCalibrationRequired;

	if(ptpd_netif_read_calibration_data(ptpPortDS->netPath.ifaceName, &d_tx, &d_rx, NULL, NULL) != PTPD_NETIF_OK)
		PTPD_TRACE(TRACE_ERROR, ptpPortDS, "Failed to obtain port calibration data\n");

  if(ptpPortDS->phyCalibrationRequired == FALSE)
  {

    ptpPortDS->deltaTx.scaledPicoseconds.lsb  = 0xFFFFFFFF & (d_tx << 16);
    ptpPortDS->deltaTx.scaledPicoseconds.msb  = 0xFFFFFFFF & (d_tx >> 16);

    ptpPortDS->deltaRx.scaledPicoseconds.lsb  = 0xFFFFFFFF & (d_rx << 16);
    ptpPortDS->deltaRx.scaledPicoseconds.msb  = 0xFFFFFFFF & (d_rx >> 16);

	PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS, "PHY calibration not required, deltas: tx = %lldps, rx = %lldps\n", d_tx, d_rx);

  }
  else
  {
    ptpPortDS->deltaTx.scaledPicoseconds.lsb  = 0;
    ptpPortDS->deltaTx.scaledPicoseconds.msb  = 0;
    ptpPortDS->deltaRx.scaledPicoseconds.lsb  = 0;
    ptpPortDS->deltaRx.scaledPicoseconds.msb  = 0;
  }


  ptpPortDS->parentWrConfig	 	  = NON_WR;
  //ptpPortDS->parentWrMode 		  = NON_WR; //useless?
  ptpPortDS->parentWrModeON		  = FALSE;
  ptpPortDS->parentCalibrated		  = FALSE;
  
  ptpPortDS->otherNodeCalPeriod		  		= 0;
  ptpPortDS->otherNodeCalRetry		  		= 0;
  ptpPortDS->otherNodeCalSendPattern	  		= 0;
  ptpPortDS->otherNodeDeltaTx.scaledPicoseconds.lsb  	= 0;
  ptpPortDS->otherNodeDeltaTx.scaledPicoseconds.msb  	= 0;
  ptpPortDS->otherNodeDeltaRx.scaledPicoseconds.lsb  	= 0;
  ptpPortDS->otherNodeDeltaRx.scaledPicoseconds.msb  	= 0;
  
  for(i = 0; i < WR_TIMER_ARRAY_SIZE;i++)
  {
    ptpPortDS->wrTimeouts[i] = ptpPortDS->wrStateTimeout;
  }
     ptpPortDS->wrTimeouts[WRS_PRESENT] = 1000;
     ptpPortDS->wrTimeouts[WRS_S_LOCK]  = 10000;
     ptpPortDS->wrTimeouts[WRS_M_LOCK]  = 10000;
  
  if(mode == INIT)
  {
    ptpPortDS->wrMode 	   = NON_WR;
    //implementation specific
    ptpPortDS->wrSlaveRole = NON_SLAVE;  
  }
    
}
