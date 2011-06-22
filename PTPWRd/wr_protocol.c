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
- main states are change using toWRSlaveState() function
- substates are changed without toWRSlaveState()
- timeouts are started/stopped when entering/exiting states
- substates were implemented to accommodate HW exceptions and
  repetition of HW access functions in such case
  (you need to remember that everything happens in gaint LOOP)

the structure is following

WR  FSM:
|  main state 		|  	substate 	|
|-------------------------------------------------
|PTPWR_PRESENT  	|			|
|PTPWR_LOCK		|			|
|			|PTPWR_LOCK_1		|
|			|PTPWR_LOCK_2		|
|PTPWR_LOCKED		|			|
|PTPWR_M_CALIBRATE	|			|
|			|PTPWR_M_CALIBRATE_1	|
|			|PTPWR_M_CALIBRATE_2	|
|PTPWR_S_CALIBRATE	|			|
|			|PTPWR_S_CALIBRATE_1	|
|			|PTPWR_S_CALIBRATE_2	|
|			|PTPWR_S_CALIBRATE_3	|(WR Master FSM only)
|PTPWR_CAL_COMPLETED	|			|
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
    return 0;
  
  if(timerExpired(&ptpPortDS->wrTimers[currentState]))
  {
#ifdef WRPTPv2
      if (ptpPortDS->currentWRstateCnt < ptpPortDS->wrStateRetry )
#else
      if (ptpPortDS->currentWRstateCnt < WR_DEFAULT_STATE_REPEAT )
#endif
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

/*  if( ptpd_netif_read_calibration_data(ifaceName, &deltaTx, &deltaRx) == PTPD_NETIF_OK)
  {
    DBG(" fixed delays known\n");
    ptpPortDS->deltaTx.scaledPicoseconds.msb = 0xFFFFFFFF & (deltaTx >> 16);
    ptpPortDS->deltaTx.scaledPicoseconds.lsb = 0xFFFFFFFF & (deltaTx << 16);

    ptpPortDS->deltaRx.scaledPicoseconds.msb = 0xFFFFFFFF & (deltaRx >> 16);
    ptpPortDS->deltaRx.scaledPicoseconds.lsb = 0xFFFFFFFF & (deltaRx << 16);

    ptpPortDS->calibrated = TRUE;

    return TRUE;

  }
  else*/
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



/*
 * ***********  WR SLAVE FSM  *****************
 */


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

  DBGV("DoWRState enter st: %d\n", ptpPortDS->wrPortState);
  switch(ptpPortDS->wrPortState)
  {

  case WRS_IDLE:

    //do nothing

    break;

  /**********************************  PRESENT  ***************************************************************************/
  case WRS_PRESENT:
    /*
     * message S_PRESENT sent to Master while entering state (toWRSlaveState())
     * here we wait for the answer from the Master asking us to LOCK
     */
    DBGV("DoState WRS_PRESENT");
    handle(rtOpts, ptpPortDS);

#ifdef WRPTPv2
    if(ptpPortDS->msgTmpWrMessageID == LOCK)
#else      
    if(ptpPortDS->msgTmpManagementId == LOCK)
#endif      
    {


      toWRState(WRS_S_LOCK, rtOpts, ptpPortDS);

      // management message used, so clean tmp
#ifdef WRPTPv2
      ptpPortDS->msgTmpWrMessageID = NULL_WR_TLV;
#else
      ptpPortDS->msgTmpManagementId = NULL_MANAGEMENT;
#endif      
    }

 DBGV("DoState WRS_PRESENT done");
    break;
  /**********************************  S_LOCK  ***************************************************************************/
  case WRS_S_LOCK:


      //substate 0  	- locking_enable failed when called while entering this state (toWRSlaveState()) so we
      //		  we need to try again

        //if(ptpd_netif_locking_enable(ptpPortDS->wrMode, ptpPortDS->netPath.ifaceName) == PTPD_NETIF_OK || ptpPortDS->isSecondarySlave)
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
	    DBG("ERROR: Should not get here, trying to lock not slave port\n");
	}

	if( ptpPortDS->wrPortState == WRS_S_LOCK_1)
	    DBGWRFSM("LockingSuccess\n");

	
	break;

      //substate 1 	- polling HW
      case WRS_S_LOCK_1:
	if(ptpPortDS->wrSlaveRole == PRIMARY_SLAVE)
	{
	    if(ptpd_netif_locking_poll(ptpPortDS->wrMode, ptpPortDS->netPath.ifaceName, SLAVE_PRIORITY_0) == PTPD_NETIF_READY)
	      ptpPortDS->wrPortState = WRS_S_LOCK_2; //next level achieved
	}
	else if(ptpPortDS->wrSlaveRole == SECONDARY_SLAVE)
	{
	    //TODO: more secondary slaves here
	    if(ptpd_netif_locking_poll(ptpPortDS->wrMode, ptpPortDS->netPath.ifaceName, SLAVE_PRIORITY_1) == PTPD_NETIF_READY)
	     ptpPortDS->wrPortState = WRS_S_LOCK_2; //next level achieved
	}  
	else
	{
	    DBG("ERROR: Should not get here, trying to lock not slave port\n");
	}
	

	 break; //try again

      //substate 2 	- somehow, HW disagree to disable locking, so try again, and again...until timeout
      case WRS_S_LOCK_2:
	
	if(ptpPortDS->wrSlaveRole == PRIMARY_SLAVE)
	{
	    if(ptpd_netif_locking_disable(ptpPortDS->wrMode, ptpPortDS->netPath.ifaceName, SLAVE_PRIORITY_0) == PTPD_NETIF_OK);
	      toWRState(WRS_LOCKED, rtOpts, ptpPortDS); 
	}
	else if(ptpPortDS->wrSlaveRole == SECONDARY_SLAVE)
	{
	    //TODO: more secondary slaves here
	    if(ptpd_netif_locking_disable(ptpPortDS->wrMode, ptpPortDS->netPath.ifaceName, SLAVE_PRIORITY_1) == PTPD_NETIF_OK);
	      toWRState(WRS_LOCKED, rtOpts, ptpPortDS);
	}  
	else
	{
	    DBG("ERROR: Should not get here, trying to lock not slave port\n");
	}
	

	  break;
  /**********************************  M_LOCK  ***************************************************************************/
  case WRS_M_LOCK:

	  handle(rtOpts, ptpPortDS);

#ifdef WRPTPv2
	  if(ptpPortDS->msgTmpWrMessageID == LOCKED)
#else	    
	  if(ptpPortDS->msgTmpManagementId == LOCKED)
#endif	    
	    toWRState(WRS_REQ_CALIBRATION, rtOpts, ptpPortDS);


	  break;
  /**********************************  LOCKED  ***************************************************************************/
  case WRS_LOCKED:


	   handle(rtOpts, ptpPortDS);
#ifdef WRPTPv2
	   if(ptpPortDS->msgTmpWrMessageID == CALIBRATE)
#else
	   if(ptpPortDS->msgTmpManagementId == CALIBRATE)
#endif	     
	     toWRState(WRS_RESP_CALIB_REQ, rtOpts, ptpPortDS);

	   break;

  /**********************************  S_CALIBRATE  ***************************************************************************/
  case WRS_REQ_CALIBRATION:
	//substate 0	- first attempt to start calibration was while entering state (toWRSlaveState())
	//		  here we repeat if faild before
	DBG("PROBLEM: repeating attempt to enable calibration\n");
	    if(ptpd_netif_calibrating_enable(PTPD_NETIF_RX, ptpPortDS->netPath.ifaceName) == PTPD_NETIF_OK)
	    {
	      //reset timeout [??????????//]
	      DBG("PROBLEM: succedded to enable calibratin\n");
	      timerStart(&ptpPortDS->wrTimers[WRS_REQ_CALIBRATION],
			 ptpPortDS->wrTimeouts[WRS_REQ_CALIBRATION] );

#ifdef WRPTPv2	      
	      issueWRSignalingMsg(CALIBRATE,rtOpts, ptpPortDS);
#else	      
	      issueWRManagement(CALIBRATE,rtOpts, ptpPortDS);
#endif	      
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

#ifdef WRPTPv2
	    issueWRSignalingMsg(CALIBRATED,rtOpts, ptpPortDS);
#else
	    issueWRManagement(CALIBRATED,rtOpts, ptpPortDS);
#endif	    
	    toWRState(WRS_CALIBRATED, rtOpts, ptpPortDS);
	    ptpPortDS->calibrated = TRUE;



    break;
  /**********************************  CAL_COMPLETED  ***************************************************************************/
  case WRS_CALIBRATED:
	    handle(rtOpts, ptpPortDS);

#ifdef WRPTPv2    
	    if(ptpPortDS->msgTmpWrMessageID == CALIBRATE && ptpPortDS->wrMode == WR_MASTER)
	      toWRState(WRS_RESP_CALIB_REQ, rtOpts, ptpPortDS);
	    
	    if(ptpPortDS->msgTmpWrMessageID == WR_MODE_ON && ptpPortDS->wrMode == WR_SLAVE)
	      toWRState(WRS_WR_LINK_ON, rtOpts, ptpPortDS);
#else
	    if(ptpPortDS->msgTmpManagementId == CALIBRATE && ptpPortDS->wrMode == WR_MASTER)
	      toWRState(WRS_RESP_CALIB_REQ, rtOpts, ptpPortDS);

	    if(ptpPortDS->msgTmpManagementId == WR_MODE_ON && ptpPortDS->wrMode == WR_SLAVE)
	      toWRState(WRS_WR_LINK_ON, rtOpts, ptpPortDS);
#endif
	    break;

/**********************************  WRS_RESP_CALIB_REQ  ***************************************************************************/
  case WRS_RESP_CALIB_REQ:

#ifdef 	WRPTPv2	
	  if( ptpd_netif_calibration_pattern_enable( 	ptpPortDS->netPath.ifaceName, \
							ptpPortDS->otherNodeCalPeriod, \
							0, \
							0) == PTPD_NETIF_OK)
#else
	  if( ptpd_netif_calibration_pattern_enable( 	ptpPortDS->netPath.ifaceName, \
							ptpPortDS->otherNodeCalPeriod, \
							ptpPortDS->otherNodeCalibrationPattern, \
							ptpPortDS->otherNodeCalibrationPatternLen) == PTPD_NETIF_OK)
#endif
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
#ifdef WRPTPv2
	    if(ptpPortDS->msgTmpWrMessageID == CALIBRATED /* || timeout */)
#else	      
	    if(ptpPortDS->msgTmpManagementId == CALIBRATED /* || timeout */)
#endif	      
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
	    DBG("ERRRORROR!!!!!!!!!!\n");
	    toWRState(WRS_IDLE, rtOpts, ptpPortDS);
	   }

	break;

  /**********************************  WRS_WR_LINK_ON ***************************************************************************/
  case WRS_WR_LINK_ON:
	    /*
	     * While entering the state, we sent WR_MODE_ON to the Master and set wrModeON TRUE and assuming that Master
	     * is calibrated, set grandmaster to wrModeON and calibrated, see (toWRSlaveState())
	     */
#ifdef WRPTPv2	    
	    /*
	     * kind-of non-pre-emption of WR FSM is
	     * implemented by banning change of PTP state
	     * in the toState() function if WR state is different then
	     * WRS_IDLE. so we need to change the WR state first, before
	     * changing PTP state
	     */
	    toWRState(WRS_IDLE, rtOpts, ptpPortDS);
#endif
	    if(ptpPortDS->wrMode == WR_SLAVE)
	      /* 
	       * this is MASTER_CLOCK_SELECTED event defined in PTP in 9.2.6.13
	       */
	      toState(PTP_SLAVE, rtOpts, ptpPortDS); 
	    else if(ptpPortDS->wrMode == WR_MASTER)
	      toState(PTP_MASTER, rtOpts, ptpPortDS);
	    else
	      DBGWRFSM("SHIT !!!\n");

#ifndef WRPTPv2
	    toWRState(WRS_IDLE, rtOpts, ptpPortDS);
#endif
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
  UInteger8 currentState = returnCurrentWRMainState(ptpPortDS);

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

#ifndef WRPTPv2  
  /******** WR TIMEOUT STAFF **********
   * turn of timeout of exitingState
   * turn out timeout of enteringState
   */
  wrTimetoutManage(enteringState,exitingState,rtOpts,ptpPortDS);
#endif


#ifdef WRPTPv2
  Enumeration8 tmpWrMode;
#endif  
  
  /* leaving state tasks */
  switch(ptpPortDS->wrPortState)
  {
  case WRS_IDLE:
      
    
    /*
     * RE-INITIALIZATION OF White Rabbit Data Sets
     * (chapter (Re-)Initialization of wrspec
     *
     * with a "hack" to remember the desired wrMode,
     * Fixme/TODO: do it nicer
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
  /*No need of PRE_MASTER state because of only ordinary clock implementation*/

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
    
#ifdef WRPTPv2
    issueWRSignalingMsg(SLAVE_PRESENT,rtOpts, ptpPortDS);
#else
    issueWRManagement(SLAVE_PRESENT,rtOpts, ptpPortDS);
#endif
    ptpPortDS->wrPortState = WRS_PRESENT;
    break;

  case WRS_S_LOCK:
    /* LOCK state implements 3 substates:
     * 0 - enable locking
     * 1 - locking enabled, polling
     * 2 - locked, disabling locking
     */
    DBGWRFSM("entering  WR_LOCK (modded?)\n");


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

    //DBG("state WR_LOCK (modded done?)\n");

    break;

  case WRS_LOCKED:
    /* no substates here*/
    DBGWRFSM("entering  WR_LOCKED\n");

    /* say Master that you are locked */
#ifdef WRPTPv2
    issueWRSignalingMsg(LOCKED,rtOpts, ptpPortDS);
#else
    issueWRManagement(LOCKED,rtOpts, ptpPortDS);
#endif
    ptpPortDS->wrPortState = WRS_LOCKED;
    break;

  case WRS_M_LOCK:
    /* no substates here*/
    DBGWRFSM("entering  WRS_M_LOCK\n");
#ifdef WRPTPv2
    issueWRSignalingMsg(LOCK,rtOpts, ptpPortDS);
#else
    issueWRManagement(LOCK,rtOpts, ptpPortDS);
#endif
    ptpPortDS->wrPortState = WRS_M_LOCK;
    break;


   case WRS_REQ_CALIBRATION:
    /* WRS_REQ_CALIBRATION state implements 3 substates:
     * 0 - enable calibration
     * 1 - calibration enabled, polling
     * 2 - HW finished calibration, disable calibration
     */
    DBGWRFSM("entering  WRS_REQ_CALIBRATION\n");

#ifdef WRPTPv2   
    if(ptpPortDS->calPeriod > 0)
    {
       ptpPortDS->wrTimeouts[WRS_REQ_CALIBRATION]   = ptpPortDS->calPeriod;
       DBG("set wrTimeout of WRS_REQ_CALIBRATION based on calPeriod:  %u [us]\n", ptpPortDS->calPeriod);
    }
    else
       ptpPortDS->wrTimeouts[WRS_REQ_CALIBRATION]   = ptpPortDS->wrStateTimeout;
#endif   
   
    if( ptpPortDS->calibrated == TRUE)
    {
      /*
       * NO CALIBRATION NEEDED !!!!!
       * just go to the last step of this state
       * which is going to WRS_CALIBRATED
       */
#ifdef WRPTPv2
      issueWRSignalingMsg(CALIBRATE,rtOpts, ptpPortDS);
#else
      issueWRManagement(CALIBRATE,rtOpts, ptpPortDS);
#endif      
      ptpPortDS->wrPortState = WRS_REQ_CALIBRATION_2; // go to substate 1
      break;
    }
    //turn on calibration when entering state
    if(ptpd_netif_calibrating_enable(PTPD_NETIF_RX, ptpPortDS->netPath.ifaceName) == PTPD_NETIF_OK)
    {
      //successfully enabled calibration, inform master
#ifdef WRPTPv2
      issueWRSignalingMsg(CALIBRATE,rtOpts, ptpPortDS);
#else      
      issueWRManagement(CALIBRATE,rtOpts, ptpPortDS);
#endif      
      ptpPortDS->wrPortState = WRS_REQ_CALIBRATION_1; // go to substate 1
    }
    else
    {
      //crap, probably calibration module busy with
      //calibrating other port, repeat attempt to enable calibration
      ptpPortDS->wrPortState = WRS_REQ_CALIBRATION;
    }

    break;

  case WRS_CALIBRATED:
    DBGWRFSM("entering  WRS_CALIBRATED\n");

    ptpPortDS->wrPortState = WRS_CALIBRATED;
    break;

  case WRS_RESP_CALIB_REQ:
    /* M_CALIBRATE state implements 3 substates:
     * 0 - enable pattern
     * 1 - pattern enabled, polling
     * 2 - MASTER_CALIBRATED received, disabling pattern
     * 3 -
     * 4 -
     */
    DBGWRFSM("entering  WRS_RESP_CALIB_REQ\n");

    // to send the pattern or not to send
    // here is the answer to the question.....
    if(ptpPortDS->otherNodeCalSendPattern == TRUE)
    {
      /*
       * the other node needs calibration, so
       * turn on calibration pattern
       */

#ifdef 	WRPTPv2

	if(ptpPortDS->otherNodeCalPeriod > 0)
	{
	    ptpPortDS->wrTimeouts[WRS_RESP_CALIB_REQ]   = ptpPortDS->otherNodeCalPeriod;
	    DBG("set wrTimeout of WRS_RESP_CALIB_REQ based on calPeriod:  %u [us]\n", ptpPortDS->otherNodeCalPeriod);
	}
	else
	    ptpPortDS->wrTimeouts[WRS_RESP_CALIB_REQ]   = ptpPortDS->wrStateTimeout;
	
	DBG("PROBLEM: trying to enable calibration pattern\n");
	if( ptpd_netif_calibration_pattern_enable( ptpPortDS->netPath.ifaceName, \
				ptpPortDS->otherNodeCalPeriod, \
				0, 0) == PTPD_NETIF_OK)
#else
	if( ptpd_netif_calibration_pattern_enable( ptpPortDS->netPath.ifaceName, \
				ptpPortDS->otherNodeCalPeriod, \
				ptpPortDS->otherNodeCalibrationPattern, \
				ptpPortDS->otherNodeCalibrationPatternLen) == PTPD_NETIF_OK)

#endif
	{
	  DBG("PROBLEM: Succeded to enable calibration pattern\n");
	  ptpPortDS->wrPortState = WRS_RESP_CALIB_REQ_1; //go to substate 1
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

    ptpPortDS->wrModeON = TRUE;

    if(ptpPortDS->wrMode == WR_MASTER)
#ifdef WRPTPv2
      issueWRSignalingMsg(WR_MODE_ON,rtOpts, ptpPortDS);
#else      
      issueWRManagement(WR_MODE_ON,rtOpts, ptpPortDS);
#endif
    /*Assume that Master is calibrated and in WR mode, it will be verified with the next Annonce msg*/
    ptpPortDS->parentWrModeON     = TRUE;
    ptpPortDS->parentCalibrated = TRUE;

    ptpPortDS->wrPortState = WRS_WR_LINK_ON;
    break;


  default:
    DBGWRFSM("to unrecognized state\n");
    break;
  }

#ifndef WRPTPv2  
  /******** WR TIMEOUT STAFF **********
   * turn of timeout of exitingState
   * turn out timeout of enteringState,
   * called at the end, since we set timeouts of 
   * WRS_RESP_CALIB_REQ and WRS_REQ_CALIBRATION states
   */
  wrTimetoutManage(enteringState,exitingState,rtOpts,ptpPortDS);
#endif

}

#ifdef WRPTPv2
/*
  It initializes White Rabbit dynamic data fields as 
  defined in the WRSPEC, talbe 1
  Called in the places defined in the WRSpec, (Re-)Initialization 
  section
*/
Boolean initWrData(PtpPortDS *ptpPortDS)
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
  //ptpPortDS->parentWrMode 		  = NON_WR;
  ptpPortDS->parentWrModeON		  = FALSE;
  ptpPortDS->parentCalibrated		  = FALSE;
  
  ptpPortDS->otherNodeCalPeriod		  		= 0;
  ptpPortDS->otherNodeCalSendPattern	  		= 0;
  ptpPortDS->otherNodeDeltaTx.scaledPicoseconds.lsb  	= 0;
  ptpPortDS->otherNodeDeltaTx.scaledPicoseconds.msb  	= 0;
  ptpPortDS->otherNodeDeltaRx.scaledPicoseconds.lsb  	= 0;
  ptpPortDS->otherNodeDeltaRx.scaledPicoseconds.msb  	= 0;
  
  for(i = 0; i < WR_TIMER_ARRAY_SIZE;i++)
  {
    ptpPortDS->wrTimeouts[i] = ptpPortDS->wrStateTimeout;
  }
/*
  // TODO: fixme: locking timeout should be bigger ????
  ptpPortDS->wrTimeouts[WRS_S_LOCK]   = 10000;
  ptpPortDS->wrTimeouts[WRS_S_LOCK_1] = 10000;
  ptpPortDS->wrTimeouts[WRS_S_LOCK_2] = 10000;  
*/  
  
}
#endif