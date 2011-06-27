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
|WRS_REQ_CALIBRATION	|			|
|			|WRS_REQ_CALIBRATION_1	|
|			|WRS_REQ_CALIBRATION_2	|
|			|WRS_REQ_CALIBRATION_3	|
|			|WRS_REQ_CALIBRATION_4	|
|			|WRS_REQ_CALIBRATION_5	|
|			|WRS_REQ_CALIBRATION_6	|
|			|WRS_REQ_CALIBRATION_7	|
|			|WRS_REQ_CALIBRATION_8	|
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
  return ptpd_netif_get_port_state(netPath->ifaceName) == PTPD_NETIF_OK ? TRUE : FALSE;
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

   case WRS_REQ_CALIBRATION:
   case WRS_REQ_CALIBRATION_1:
   case WRS_REQ_CALIBRATION_2:
   case WRS_REQ_CALIBRATION_3:
   case WRS_REQ_CALIBRATION_4:
   case WRS_REQ_CALIBRATION_5:
   case WRS_REQ_CALIBRATION_6:
   case WRS_REQ_CALIBRATION_7:
   case WRS_REQ_CALIBRATION_8:
     

     state = WRS_REQ_CALIBRATION;
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
  /*WRS_IDLE state does not expire */
  if(currentState == WRS_IDLE)
    return;
  
  if(timerExpired(&ptpPortDS->wrTimers[currentState]))
  {

      if (ptpPortDS->currentWRstateCnt < ptpPortDS->wrStateRetry )
      {
	DBG("WR_Slave_TIMEOUT: state[= %d] timeout, repeat state\n", currentState);
	toWRState(currentState, rtOpts, ptpPortDS);
      }
      else
      {
	DBG("WR_Slave_TIMEOUT: state[=%d] timeout, repeated %d times, going to Standard PTP\n", \
	currentState,ptpPortDS->currentWRstateCnt );
	
	ptpPortDS->wrModeON = FALSE;
        toWRState(WRS_IDLE, rtOpts, ptpPortDS);

	if(wrMode == WR_MASTER)
	  toState(PTP_MASTER, rtOpts, ptpPortDS);
	else
	  toState(PTP_SLAVE, rtOpts, ptpPortDS);
	/*
	 * RE-INITIALIZATION OF White Rabbit Data Sets
	 * (chapter (Re-)Initialization of wrspec
	 */	
	initWrData(ptpPortDS);
      }

  }
}

#ifndef NewTxCal
// this function will probably be unnecessary

/*
Function tries to read fixed delays (if PTPWRd restarted, they are remembered by HW
if delays not known, Tx fixed delays are measured

we wait here as long as it takes to calibrate the delay !!!!!!

return:
  TRUE 	- calibration OK
  FALSE - sth wrong

*/
Boolean initWRcalibration(const char *ifaceName,PtpPortDS *ptpPortDS )
{
  DBG("starting\n");
  uint64_t deltaTx;
  int ret;
  /*
   * check if Rx & Tx delays known
   * on this interface, this would mean
   * that the demon was restarted
   * or deterministic HW used
   *
   * otherwise, calibrate Rx
   */


  {
    DBG(" measuring Tx fixed delay for interface %s\n",__func__,ifaceName );
    /*
     * here we calibrate Tx of a given interface
     * since only one interface can be calibrated at a time
     * you can find here usleep(), in other words, function does not exit
     * until calibration is finished
     * [below implementation has no "style" it just works]
     *
     * -- no, it just doesn't - Tom
     * There should be no calibration conflicts for TX calibration if the function actively waits
     */



    DBG("CalPatEnable!\n");
    if(ptpd_netif_calibration_pattern_enable(ifaceName, 0, 0, 0) != PTPD_NETIF_OK)
      return FALSE;

    DBG("CalMesaEnable!\n");
    if(ptpd_netif_calibrating_enable(PTPD_NETIF_TX, ifaceName) != PTPD_NETIF_OK)
      return FALSE;

    DBG("CalPoll!\n");

    for(;;)
      {
	ret = ptpd_netif_calibrating_poll(PTPD_NETIF_TX, ifaceName,&deltaTx);

	if(ret == PTPD_NETIF_READY)
	  {
	    printf("TX fixed delay = %d\n\n",(int)deltaTx);
	    ptpPortDS->deltaTx.scaledPicoseconds.msb = 0xFFFFFFFF & (deltaTx >> 16);
	    ptpPortDS->deltaTx.scaledPicoseconds.lsb = 0xFFFFFFFF & (deltaTx << 16);
	    break;
	  } else usleep(10000);
      }


    DBG("CalMeasDisable\n");

    ptpd_netif_calibrating_disable(PTPD_NETIF_TX,ifaceName);
    ptpd_netif_calibration_pattern_disable(ifaceName);

    return TRUE;


  }

}
#endif


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
char *format_wr_timestamp(wr_timestamp_t ts)
{
  char buf[64];

  snprintf(buf,64, "sec: %lld nsec: %ld ", (long long)ts.utc,
	   (long) ts.nsec);

  return strdup(buf);
}




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
  
  DBGV("DoWRState enter st: %d\n", ptpPortDS->wrPortState);
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
    DBGV("DoState WRS_PRESENT");
    handle(rtOpts, ptpPortDS);


    if(ptpPortDS->msgTmpWrMessageID == LOCK)
    {
      toWRState(WRS_S_LOCK, rtOpts, ptpPortDS);
      
      // management message used, so clean tmp
      ptpPortDS->msgTmpWrMessageID = NULL_WR_TLV;
    }

 DBGV("DoState WRS_PRESENT done");
    break;
  /**********************************  S_LOCK  ***************************************************************************/
  case WRS_S_LOCK:


	//substate 0  	- locking_enable failed when called while entering this state (toWRState()) so we
	//		  we need to try again

	/* depending what kind of slave the port is, different locking is used ("slave's role" is 
	   decided by the modifiedBMC) */
	if(ptpPortDS->wrSlaveRole == PRIMARY_SLAVE)
	{
	   DBGWRFSM("locking primary slave\n");
	   if(ptpd_netif_locking_enable(ptpPortDS->wrMode, ptpPortDS->netPath.ifaceName, SLAVE_PRIORITY_0) == PTPD_NETIF_OK )
	      ptpPortDS->wrPortState = WRS_S_LOCK_1;
	}
	else if(ptpPortDS->wrSlaveRole == SECONDARY_SLAVE)
	{
	   DBGWRFSM("locking secondary slave\n");
	   //TODO: make for more secondary slaves
	   if(ptpd_netif_locking_enable(ptpPortDS->wrMode, ptpPortDS->netPath.ifaceName, SLAVE_PRIORITY_1) == PTPD_NETIF_OK )
	      ptpPortDS->wrPortState = WRS_S_LOCK_1;	
	}
	else
	{
	    DBG("ERROR: Should not get here, trying to lock not a slave port\n");
	}

	//check if enabling() succeeded
	if( ptpPortDS->wrPortState == WRS_S_LOCK_1)
	    DBGWRFSM("LockingSuccess\n"); //if yes, no break, go ahead
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
	    //TODO: more secondary slaves here
	    if(ptpd_netif_locking_poll(ptpPortDS->wrMode, ptpPortDS->netPath.ifaceName, SLAVE_PRIORITY_1) == PTPD_NETIF_READY)
	      ptpPortDS->wrPortState = WRS_S_LOCK_2; //next level achieved
	    else
	      break; //try again  
	}  
	else
	{
	    DBG("ERROR: Should not get here, trying to lock not slave port\n");
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
	    //TODO: more secondary slaves here
	    if(ptpd_netif_locking_disable(ptpPortDS->wrMode, ptpPortDS->netPath.ifaceName, SLAVE_PRIORITY_1) == PTPD_NETIF_OK)
	      toWRState(WRS_LOCKED, rtOpts, ptpPortDS);
	    else
	      break; //try again  	    
	}  
	else
	{
	    DBG("ERROR: Should not get here, trying to lock not slave port\n");
	    break;
	}
	  
	break; //this is needed here, just in case, we separate WR States  
  /**********************************  M_LOCK  ***************************************************************************/
  case WRS_M_LOCK:

	  handle(rtOpts, ptpPortDS);

	  if(ptpPortDS->msgTmpWrMessageID == LOCKED)
	  {
	    toWRState(WRS_REQ_CALIBRATION, rtOpts, ptpPortDS);
	  }
	  break;
  /**********************************  LOCKED  ***************************************************************************/
  case WRS_LOCKED:


	   handle(rtOpts, ptpPortDS);
	   if(ptpPortDS->msgTmpWrMessageID == CALIBRATE)
	     toWRState(WRS_RESP_CALIB_REQ, rtOpts, ptpPortDS);

	   break;

  /**********************************  WRS_REQ_CALIBRATION  ***************************************************************************/
  case WRS_REQ_CALIBRATION:
	//substate 0	- first attempt to start calibration was while entering state (toWRState())
	//		  here we repeat if faild before

#ifdef NewTxCal	  
// this is a new way of calibrating Tx - before it was when ptpd started, but it's a bad idea,
// so now we do it during WR Link Setup

	// first we start calibration pattern
	    if(ptpd_netif_calibration_pattern_enable(ptpPortDS->netPath.ifaceName, 0, 0, 0) == PTPD_NETIF_OK)
	      ptpPortDS->wrPortState = WRS_REQ_CALIBRATION_1;
	    else
	      break; // go again

	    // no break here
	// then we start calibration of the port's Tx     
	case WRS_REQ_CALIBRATION_1:

	    if(ptpd_netif_calibrating_enable(PTPD_NETIF_TX, ptpPortDS->netPath.ifaceName) == PTPD_NETIF_OK)
	      ptpPortDS->wrPortState = WRS_REQ_CALIBRATION_2; // go to substate 1
	    else
	      break; // again

	    // no braek here
	    
	//we wait until the calibration is finished
	case WRS_REQ_CALIBRATION_2:
	    
	    if(ptpd_netif_calibrating_poll(PTPD_NETIF_TX, ptpPortDS->netPath.ifaceName,&delta) == PTPD_NETIF_READY)
	    {
		printf("TX fixed delay = %d\n\n",(int)delta);
		ptpPortDS->deltaTx.scaledPicoseconds.msb = 0xFFFFFFFF & (delta >> 16);
		ptpPortDS->deltaTx.scaledPicoseconds.lsb = 0xFFFFFFFF & (delta << 16);
		DBGWRFSM("Tx=>>scaledPicoseconds.msb = 0x%x\n",ptpPortDS->deltaTx.scaledPicoseconds.msb);
	        DBGWRFSM("Tx=>>scaledPicoseconds.lsb = 0x%x\n",ptpPortDS->deltaTx.scaledPicoseconds.lsb);
	
		ptpPortDS->wrPortState = WRS_REQ_CALIBRATION_3;
	    }
	    else
		break; // again

	// now we disable port's Tx calibration
	case WRS_REQ_CALIBRATION_3:
	    
    
	    if(ptpd_netif_calibrating_disable(PTPD_NETIF_TX, ptpPortDS->netPath.ifaceName) == PTPD_NETIF_OK)
		ptpPortDS->wrPortState = WRS_REQ_CALIBRATION_4;
	    else
		break; // again

	// we disable the pattern
	case WRS_REQ_CALIBRATION_4:

	    if(ptpd_netif_calibration_pattern_disable(ptpPortDS->netPath.ifaceName) == PTPD_NETIF_OK)
		ptpPortDS->wrPortState = WRS_REQ_CALIBRATION_5;
	    else
		break; // again    
    
	// now we go to the calibration of Rx using the pattern send by the other port, enable Rx calibration
	case WRS_REQ_CALIBRATION_5:
	    
	    if(ptpd_netif_calibrating_enable(PTPD_NETIF_RX, ptpPortDS->netPath.ifaceName) == PTPD_NETIF_OK)
	      ptpPortDS->wrPortState = WRS_REQ_CALIBRATION_6;
	    else
	      break; //try again

	//check whether Rx calibration is finished
	case WRS_REQ_CALIBRATION_6:

	    if(ptpd_netif_calibrating_poll(PTPD_NETIF_RX, ptpPortDS->netPath.ifaceName,&delta) == PTPD_NETIF_READY)
	    {
	      DBGWRFSM("RX fixed delay = %d\n",(int)delta);
	      ptpPortDS->deltaRx.scaledPicoseconds.msb = 0xFFFFFFFF & (delta >> 16);
	      ptpPortDS->deltaRx.scaledPicoseconds.lsb = 0xFFFFFFFF & (delta << 16);
	      DBGWRFSM("Rx=>>scaledPicoseconds.msb = 0x%x\n",ptpPortDS->deltaRx.scaledPicoseconds.msb);
	      DBGWRFSM("Rx=>>scaledPicoseconds.lsb = 0x%x\n",ptpPortDS->deltaRx.scaledPicoseconds.lsb);

	      ptpPortDS->wrPortState = WRS_REQ_CALIBRATION_7;
	    }
	    else
	      break; //try again
	      
	// disable Rx calibration 
	case WRS_REQ_CALIBRATION_7:

	    if( ptpd_netif_calibrating_disable(PTPD_NETIF_RX, ptpPortDS->netPath.ifaceName) == PTPD_NETIF_OK)
	      ptpPortDS->wrPortState = WRS_REQ_CALIBRATION_8;
	    else
	      break; // try again
	    
	 // send deltas to the other port and go to the next state   
	 case WRS_REQ_CALIBRATION_8:

	    issueWRSignalingMsg(CALIBRATED,rtOpts, ptpPortDS);
   
	    toWRState(WRS_CALIBRATED, rtOpts, ptpPortDS);
	    ptpPortDS->calibrated = TRUE;	    
	    
	    
#else
	    if(ptpd_netif_calibrating_enable(PTPD_NETIF_RX, ptpPortDS->netPath.ifaceName) == PTPD_NETIF_OK)
	    {
	      //reset timeout [??????????//]
	
	      timerStart(&ptpPortDS->wrTimers[WRS_REQ_CALIBRATION],
			 ptpPortDS->wrTimeouts[WRS_REQ_CALIBRATION] );


	      issueWRSignalingMsg(CALIBRATE,rtOpts, ptpPortDS);
	      ptpPortDS->wrPortState = WRS_REQ_CALIBRATION_1;
	    }
	    else
	    {
	       DBG("PROBLEM: failed to enable calibratin\n");
	      break; //try again
	    }

	//substate 1	- waiting for HW to finish measurement
	case WRS_REQ_CALIBRATION_1:

	    if(ptpd_netif_calibrating_poll(PTPD_NETIF_RX, ptpPortDS->netPath.ifaceName,&delta) == PTPD_NETIF_READY)
	    {
	      DBGWRFSM("PTPWR_S_CALIBRATE_1: delta = 0x%x\n",delta);
	      ptpPortDS->deltaRx.scaledPicoseconds.msb = 0xFFFFFFFF & (delta >> 16);
	      ptpPortDS->deltaRx.scaledPicoseconds.lsb = 0xFFFFFFFF & (delta << 16);
	      DBGWRFSM("scaledPicoseconds.msb = 0x%x\n",ptpPortDS->deltaRx.scaledPicoseconds.msb);
	      DBGWRFSM("scaledPicoseconds.lsb = 0x%x\n",ptpPortDS->deltaRx.scaledPicoseconds.lsb);

	      ptpPortDS->wrPortState = WRS_REQ_CALIBRATION_2;
	    }
	    else
	      break; //try again

	//substate 2	- trying to disable calibration
	case WRS_REQ_CALIBRATION_2:

	    if( ptpd_netif_calibrating_disable(PTPD_NETIF_RX, ptpPortDS->netPath.ifaceName) != PTPD_NETIF_OK)
	      break; // try again


	    issueWRSignalingMsg(CALIBRATED,rtOpts, ptpPortDS);
   
	    toWRState(WRS_CALIBRATED, rtOpts, ptpPortDS);
	    ptpPortDS->calibrated = TRUE;
#endif


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
	    toWRState(WRS_REQ_CALIBRATION, rtOpts, ptpPortDS);
	  else
	  {
	    DBG("ERRRORROR!!!!!!!!!! : WRS_RESP_CALIB_REQ_3\n");
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
	      DBGWRFSM("ERROR: WRS_WR_LINK_ON !!!\n");
	    break;



   /**********************************  default  ***************************************************************************/
  default:

	    DBGWRFSM("(doWhiteRabbitState) do unrecognized state\n");
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
  Enumeration8 tmpWrMode;
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
     * with a "hack" to remember the desired wrMode,
     * Fixme/TODO (7): do it nicer
     */
    tmpWrMode = ptpPortDS->wrMode;
    initWrData(ptpPortDS);
    ptpPortDS->wrMode = tmpWrMode; //re-set the desired wrMode    
    
    
    DBGWRFSM("exiting WRS_IDLE\n");
    DBGWRFSM("^^^^^^^^^^^^^^^^ starting White Rabbit State Machine^^^^^^^^^^^^^^^^^^\n");
    DBGWRFSM("\n");
    DBGWRFSM("\n");
    DBGWRFSM("\n");
    DBGWRFSM("\n");
    if(ptpPortDS->wrMode== WR_MASTER)
    DBGWRFSM("                            W R   M A S T E R\n");
    else if(ptpPortDS->wrMode== WR_SLAVE)
    DBGWRFSM("                             W R   S L A V E \n");
    else
    DBGWRFSM("                        I SHOULD NOT SHOW THIS MSG !!! \n");
    DBGWRFSM("\n");
    DBGWRFSM("\n");
    DBGWRFSM("\n");
    DBGWRFSM("\n");    
    break;

  case WRS_PRESENT:
    DBGWRFSM("exiting WRS_PRESENT\n");
    break;

  case WRS_S_LOCK:
    DBGWRFSM("exiting WRS_S_LOCK\n");    
  case WRS_S_LOCK_1:
  case WRS_S_LOCK_2:
    break;

  case WRS_M_LOCK:
    DBGWRFSM("exiting WRS_M_LOCK\n");    
     break;

  case WRS_LOCKED:
    DBGWRFSM("exiting WRS_LOCKED\n");    
     break;

   case WRS_REQ_CALIBRATION:
    DBGWRFSM("exiting WRS_REQ_CALIBRATION\n");     
   case WRS_REQ_CALIBRATION_1:
   case WRS_REQ_CALIBRATION_2:
          
     break;

   case WRS_CALIBRATED:
     DBGWRFSM("exiting WRS_CALIBRATED\n");     
     break;

   case WRS_WR_LINK_ON:
     DBGWRFSM("\n");
     DBGWRFSM("exiting WRS_WR_LINK_ON\n");
     DBGWRFSM("^^^^^^^^^^^^^^^^^^^^^^^^^ WR Link is ON ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
     DBGWRFSM("\n");
     DBGWRFSM("\n");
     DBGWRFSM("\n");
     DBGWRFSM("\n");
     DBGWRFSM("\n");

     break;

  default:

    break;
  }



  /* entering state tasks */
  /* TODO  (8): Do we need PRE_MASTER state because we now have a boundary clock implementation ?*/

  switch(enteringState)
  {
  case WRS_IDLE:
    /* no substates here*/
    DBGWRFSM("entering WRS_IDLE\n");

    ptpPortDS->wrPortState = WRS_IDLE;
    break;

  case WRS_PRESENT:
    /* no substates here*/
    DBGWRFSM("entering  WRS_PRESENT\n");
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
    DBGWRFSM("entering  WR_LOCK (modded?)\n");


	/* depending what kind of slave the port is, different locking is used ("slave's role" is 
	   decided by the modifiedBMC) */
	if(ptpPortDS->wrSlaveRole == PRIMARY_SLAVE)
	{
	   DBGWRFSM("locking primary slave\n");
	   if(ptpd_netif_locking_enable(ptpPortDS->wrMode, ptpPortDS->netPath.ifaceName, SLAVE_PRIORITY_0) == PTPD_NETIF_OK )
	      ptpPortDS->wrPortState = WRS_S_LOCK_1;  //go to substate 1
	   else
	      ptpPortDS->wrPortState = WRS_S_LOCK;   //stay in substate 0, try again
	}
	else if(ptpPortDS->wrSlaveRole == SECONDARY_SLAVE)
	{
	   DBGWRFSM("locking secondary slave\n");
	   //TODO: make for more secondary slaves
	   if(ptpd_netif_locking_enable(ptpPortDS->wrMode, ptpPortDS->netPath.ifaceName, SLAVE_PRIORITY_1) == PTPD_NETIF_OK )
	      ptpPortDS->wrPortState = WRS_S_LOCK_1;	  //go to substate 1
	   else
	      ptpPortDS->wrPortState = WRS_S_LOCK;   //stay in substate 0, try again
	}
	else
	{
	    DBG("ERROR: Should not get here, trying to lock not slave port\n");
	}

    break;

  case WRS_LOCKED:
    /* no substates here*/
    DBGWRFSM("entering  WR_LOCKED\n");

    /* say to the Master that you are locked */
    issueWRSignalingMsg(LOCKED,rtOpts, ptpPortDS);
    ptpPortDS->wrPortState = WRS_LOCKED;
    break;

  case WRS_M_LOCK:
    /* no substates here*/
    DBGWRFSM("entering  WRS_M_LOCK\n");
    issueWRSignalingMsg(LOCK,rtOpts, ptpPortDS);
    ptpPortDS->wrPortState = WRS_M_LOCK;
    break;


   case WRS_REQ_CALIBRATION: 
     
#ifdef NewTxCal
    /* WRS_REQ_CALIBRATION state implements 8 substates:
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
#else
    /* WRS_REQ_CALIBRATION state implements 3 substates:
     * 0 - enable calibration of Rx
     * 1 - calibration enabled, polling Rx
     * 2 - HW finished calibration, disable calibration of Rx
     */
#endif

    DBGWRFSM("entering  WRS_REQ_CALIBRATION\n");

#ifdef NewTxCal          
    //TODO: I don't really like it here !!
    issueWRSignalingMsg(CALIBRATE,rtOpts, ptpPortDS);
#endif

    if(ptpPortDS->calPeriod > 0)
    {
       ptpPortDS->wrTimeouts[WRS_REQ_CALIBRATION]   = ptpPortDS->calPeriod;
       DBG("set wrTimeout of WRS_REQ_CALIBRATION based on calPeriod:  %u [us]\n", ptpPortDS->calPeriod);
    }
    else
       ptpPortDS->wrTimeouts[WRS_REQ_CALIBRATION]   = ptpPortDS->wrStateTimeout;
 
  
    if( ptpPortDS->calibrated == TRUE)
    {
      /*
       * NO CALIBRATION NEEDED !!!!!
       * just go to the last step of this state
       * which is going to WRS_CALIBRATED
       */
#ifdef NewTxCal
      ptpPortDS->wrPortState = WRS_REQ_CALIBRATION_2; // go to substate 1
#else
      ptpPortDS->wrPortState = WRS_REQ_CALIBRATION_7; // go to substate 1
      issueWRSignalingMsg(CALIBRATE,rtOpts, ptpPortDS);
#endif
      
      break;
    }
    
#ifdef NewTxCal    
    // enable pattern sending
    if(ptpd_netif_calibration_pattern_enable(ptpPortDS->netPath.ifaceName, 0, 0, 0) == PTPD_NETIF_OK)
    {
	// enable Tx calibration
	if(ptpd_netif_calibrating_enable(PTPD_NETIF_TX, ptpPortDS->netPath.ifaceName) == PTPD_NETIF_OK)
	    ptpPortDS->wrPortState = WRS_REQ_CALIBRATION_2; 
	else
	    ptpPortDS->wrPortState = WRS_REQ_CALIBRATION_1;
    }
    else
    {
      //crap, probably calibration module busy with
      //calibrating other port, repeat attempt to enable calibration      
      ptpPortDS->wrPortState = WRS_REQ_CALIBRATION;    
    }
    
#else   
     //turn on calibration when entering state
     if(ptpd_netif_calibrating_enable(PTPD_NETIF_RX, ptpPortDS->netPath.ifaceName) == PTPD_NETIF_OK)
     {
       //successfully enabled calibration, inform master
 
       issueWRSignalingMsg(CALIBRATE,rtOpts, ptpPortDS);
       
       ptpPortDS->wrPortState = WRS_REQ_CALIBRATION_1; // go to substate 1
     }
     else
     {
       //crap, probably calibration module busy with
       //calibrating other port, repeat attempt to enable calibration
       ptpPortDS->wrPortState = WRS_REQ_CALIBRATION;
     }
#endif

    break;

  case WRS_CALIBRATED:
    DBGWRFSM("entering  WRS_CALIBRATED\n");

    ptpPortDS->wrPortState = WRS_CALIBRATED;
    break;

  case WRS_RESP_CALIB_REQ:
    /* M_CALIBRATE state implements 3 substates:
     * 0 - enable pattern sending
     * 1 - pattern enabled, waiting for the CALIBRATED message from the other port
     * 2 - CALIBRATED received, disabling pattern
     * 3 - Go to the next state
     */
    DBGWRFSM("entering  WRS_RESP_CALIB_REQ\n");

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
	    DBG("set wrTimeout of WRS_RESP_CALIB_REQ based on calPeriod:  %u [us]\n", \
	    ptpPortDS->otherNodeCalPeriod);
	}
	else
	    ptpPortDS->wrTimeouts[WRS_RESP_CALIB_REQ]   = ptpPortDS->wrStateTimeout;
	
	DBG("PROBLEM: trying to enable calibration pattern\n");
	if( ptpd_netif_calibration_pattern_enable( ptpPortDS->netPath.ifaceName, \
				ptpPortDS->otherNodeCalPeriod, \
				0, 0) == PTPD_NETIF_OK)
	{
	  DBG("PROBLEM: Succeded to enable calibration pattern\n");
	  ptpPortDS->wrPortState = WRS_RESP_CALIB_REQ_1; 
	}
	else
	{
	  DBG("PROBLEM: failed to enable calibration pattern\n");
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
    DBGWRFSM("entering  WRS_LINK_ON\n");

    ptpPortDS->wrModeON 	= TRUE;
    
    if(ptpPortDS->wrMode == WR_MASTER)
      issueWRSignalingMsg(WR_MODE_ON,rtOpts, ptpPortDS);
    /*Assume that Master is calibrated and in WR mode, it will be verified with the next Annonce msg*/
    ptpPortDS->parentWrModeON     = TRUE;
    
    /* TODO: (9)
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
    DBGWRFSM("to unrecognized state\n");
    break;
  }

  /******** WR TIMEOUT STAFF **********
   * turn of timeout of exitingState
   * turn out timeout of enteringState,
   * called at the end, since we set timeouts of 
   * WRS_RESP_CALIB_REQ and WRS_REQ_CALIBRATION states
   */
  wrTimetoutManage(enteringState,exitingState,rtOpts,ptpPortDS);

}

/*
  It initializes White Rabbit dynamic data fields as 
  defined in the WRSPEC, talbe 1
  Called in the places defined in the WRSpec, (Re-)Initialization 
  section
*/
void initWrData(PtpPortDS *ptpPortDS)
{
  
  DBG("White Rabbit data (re-)initialization\n");
  int i=0;
  ptpPortDS->wrMode 			   = NON_WR;
  ptpPortDS->wrModeON    		   = FALSE;
  ptpPortDS->wrPortState 		   = WRS_IDLE;
  ptpPortDS->calibrated  		   = ptpPortDS->deltasKnown;

  if(ptpPortDS->deltasKnown == TRUE)
  {
    ptpPortDS->deltaTx.scaledPicoseconds.lsb  = ptpPortDS->knownDeltaTx.scaledPicoseconds.lsb;
    ptpPortDS->deltaTx.scaledPicoseconds.msb  = ptpPortDS->knownDeltaTx.scaledPicoseconds.msb;
    ptpPortDS->deltaRx.scaledPicoseconds.lsb  = ptpPortDS->knownDeltaRx.scaledPicoseconds.lsb;
    ptpPortDS->deltaRx.scaledPicoseconds.msb  = ptpPortDS->knownDeltaRx.scaledPicoseconds.msb;
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
  ptpPortDS->otherNodeCalSendPattern	  		= 0;
  ptpPortDS->otherNodeDeltaTx.scaledPicoseconds.lsb  	= 0;
  ptpPortDS->otherNodeDeltaTx.scaledPicoseconds.msb  	= 0;
  ptpPortDS->otherNodeDeltaRx.scaledPicoseconds.lsb  	= 0;
  ptpPortDS->otherNodeDeltaRx.scaledPicoseconds.msb  	= 0;
  
  //TODO (10)
  for(i = 0; i < WR_TIMER_ARRAY_SIZE;i++)
  {
    ptpPortDS->wrTimeouts[i] = ptpPortDS->wrStateTimeout;
  }
    ptpPortDS->wrTimeouts[WRS_PRESENT] = 1000;
    ptpPortDS->wrTimeouts[WRS_S_LOCK]  = 10000;
    ptpPortDS->wrTimeouts[WRS_M_LOCK]  = 10000;
  
}
