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
UInteger8 returnCurrentWRMainState( PtpClock *ptpClock)
{
  /*
   * this (exitingState) have to do with substates and timeouts,
   * if we exit one of substates, the value of exiting state
   * which is inputed into wrTimetoutManage() needs to be of
   * the main state since we calculate timeouts for main states
   */
  UInteger8 state;
  /* leaving state tasks */
  switch(ptpClock->wrPortState)
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

     state = ptpClock->wrPortState;
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
void wrTimetoutManage(UInteger8 enteringState, UInteger8 exitingState, RunTimeOpts *rtOpts, PtpClock *ptpClock)
{

  /*
    checking if the state is repeated,
    repeated state means that there was timeout
    so we need to increase repetition counter
  */
  if(enteringState != exitingState)
    ptpClock->currentWRstateCnt = 0;
  else
    ptpClock->currentWRstateCnt++;

  /*stop time from the state you are leaving (except IDLE)*/
  if(exitingState != WRS_IDLE)
    timerStop(&ptpClock->wrTimers[exitingState]);

  /*start timer in the state you are entering (except IDLE) */
  if(enteringState != WRS_IDLE)
    timerStart(&ptpClock->wrTimers[enteringState], ptpClock->wrTimeouts[enteringState]);
}

/*
this function checks if wr timer has expired for a current WR state
*/
void wrTimerExpired(UInteger8 currentState, RunTimeOpts *rtOpts, PtpClock *ptpClock, Enumeration8 wrNodeMode)
{

  if(timerExpired(&ptpClock->wrTimers[currentState]))
  {
      if (ptpClock->currentWRstateCnt < WR_DEFAULT_STATE_REPEAT )
      {
	DBG("WR_Slave_TIMEOUT: state[= %d] timeout, repeat state\n", currentState);
	toWRState(currentState, rtOpts, ptpClock);
      }
      else
      {
	DBG("WR_Slave_TIMEOUT: state[=%d] timeout, repeated %d times, going to Standard PTP\n", currentState,ptpClock->currentWRstateCnt );
	ptpClock->isWRmode = FALSE;
        toWRState(WRS_IDLE, rtOpts, ptpClock);

	if(wrNodeMode == WR_MASTER)
	  toState(PTP_MASTER, rtOpts, ptpClock);
	else
	  toState(PTP_SLAVE, rtOpts, ptpClock);
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
Boolean initWRcalibration(const char *ifaceName,PtpClock *ptpClock )
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
    ptpClock->deltaTx.scaledPicoseconds.msb = 0xFFFFFFFF & (deltaTx >> 16);
    ptpClock->deltaTx.scaledPicoseconds.lsb = 0xFFFFFFFF & (deltaTx << 16);

    ptpClock->deltaRx.scaledPicoseconds.msb = 0xFFFFFFFF & (deltaRx >> 16);
    ptpClock->deltaRx.scaledPicoseconds.lsb = 0xFFFFFFFF & (deltaRx << 16);

    ptpClock->isCalibrated = TRUE;

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
	    ptpClock->deltaTx.scaledPicoseconds.msb = 0xFFFFFFFF & (deltaTx >> 16);
	    ptpClock->deltaTx.scaledPicoseconds.lsb = 0xFFFFFFFF & (deltaTx << 16);
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

void doWRState(RunTimeOpts *rtOpts, PtpClock *ptpClock)
{
  /*
   * here WR Slave FSM is implemented, please note that the implementation is
   * "unusual" (or nasty). there are substates to include the cases when
   * HW is not cooperative (returns: "no, I don't want to do that" and
   * we need to ask HW again, again and again, therefore substates
   *
   */
  uint64_t delta;

  DBG("DoWRState enter st: %d\n", ptpClock->wrPortState);
  switch(ptpClock->wrPortState)
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
    DBG("DoState WRS_PRESENT");
    handle(rtOpts, ptpClock);

#ifdef WRPTPv2
    if(ptpClock->msgTmpWrMessageID == LOCK)
#else      
    if(ptpClock->msgTmpManagementId == LOCK)
#endif      
    {


      toWRState(WRS_S_LOCK, rtOpts, ptpClock);

      // management message used, so clean tmp
#ifdef WRPTPv2
      ptpClock->msgTmpWrMessageID = NULL_WR_TLV;
#else
      ptpClock->msgTmpManagementId = NULL_MANAGEMENT;
#endif      
    }

 DBG("DoState WRS_PRESENT done");
    break;
  /**********************************  S_LOCK  ***************************************************************************/
  case WRS_S_LOCK:


      //substate 0  	- locking_enable failed when called while entering this state (toWRSlaveState()) so we
      //		  we need to try again

        if(ptpd_netif_locking_enable(ptpClock->wrNodeMode, ptpClock->netPath.ifaceName) == PTPD_NETIF_OK)
	  {
	    DBG("LockingSuccess\n");
	    ptpClock->wrPortState = WRS_S_LOCK_1; //success, go ahead
	  }
	break;

      //substate 1 	- polling HW
      case WRS_S_LOCK_1:

	 if(ptpd_netif_locking_poll(ptpClock->wrNodeMode, ptpClock->netPath.ifaceName) == PTPD_NETIF_READY)
	    ptpClock->wrPortState = WRS_S_LOCK_2; //next level achieved

	 break; //try again

      //substate 2 	- somehow, HW disagree to disable locking, so try again, and again...until timeout
      case WRS_S_LOCK_2:
	  if(ptpd_netif_locking_disable(ptpClock->wrNodeMode, ptpClock->netPath.ifaceName) == PTPD_NETIF_OK);
	    toWRState(WRS_LOCKED, rtOpts, ptpClock);
	  break;
  /**********************************  M_LOCK  ***************************************************************************/
  case WRS_M_LOCK:

	  handle(rtOpts, ptpClock);

#ifdef WRPTPv2
	  if(ptpClock->msgTmpWrMessageID == LOCKED)
#else	    
	  if(ptpClock->msgTmpManagementId == LOCKED)
#endif	    
	    toWRState(WRS_REQ_CALIBRATION, rtOpts, ptpClock);


	  break;
  /**********************************  LOCKED  ***************************************************************************/
  case WRS_LOCKED:


	   handle(rtOpts, ptpClock);
#ifdef WRPTPv2
	   if(ptpClock->msgTmpWrMessageID == CALIBRATE)
#else
	   if(ptpClock->msgTmpManagementId == CALIBRATE)
#endif	     
	     toWRState(WRS_RESP_CALIB_REQ, rtOpts, ptpClock);

	   break;

  /**********************************  S_CALIBRATE  ***************************************************************************/
  case WRS_REQ_CALIBRATION:
	//substate 0	- first attempt to start calibration was while entering state (toWRSlaveState())
	//		  here we repeat if faild before

	    if(ptpd_netif_calibrating_enable(PTPD_NETIF_RX, ptpClock->netPath.ifaceName) == PTPD_NETIF_OK)
	    {
	      //reset timeout [??????????//]
	      timerStart(&ptpClock->wrTimers[WRS_REQ_CALIBRATION],
			 ptpClock->wrTimeouts[WRS_REQ_CALIBRATION] );

#ifdef WRPTPv2	      
	      issueWRSignalingMsg(CALIBRATE,rtOpts, ptpClock);
#else	      
	      issueWRManagement(CALIBRATE,rtOpts, ptpClock);
#endif	      
	      ptpClock->wrPortState = WRS_REQ_CALIBRATION_1;
	    }
	    else
	      break; //try again

	//substate 1	- waiting for HW to finish measurement
	case WRS_REQ_CALIBRATION_1:

	    if(ptpd_netif_calibrating_poll(PTPD_NETIF_RX, ptpClock->netPath.ifaceName,&delta) == PTPD_NETIF_READY)
	    {
	      DBG("PTPWR_S_CALIBRATE_1: delta = 0x%x\n",delta);
	      ptpClock->deltaRx.scaledPicoseconds.msb = 0xFFFFFFFF & (delta >> 16);
	      ptpClock->deltaRx.scaledPicoseconds.lsb = 0xFFFFFFFF & (delta << 16);
	      DBG("scaledPicoseconds.msb = 0x%x\n",ptpClock->deltaRx.scaledPicoseconds.msb);
	      DBG("scaledPicoseconds.lsb = 0x%x\n",ptpClock->deltaRx.scaledPicoseconds.lsb);

	      ptpClock->wrPortState = WRS_REQ_CALIBRATION_2;
	    }
	    else
	      break; //try again

	//substate 2	- trying to disable calibration
	case WRS_REQ_CALIBRATION_2:

	    if( ptpd_netif_calibrating_disable(PTPD_NETIF_RX, ptpClock->netPath.ifaceName) != PTPD_NETIF_OK)
	      break; // try again

#ifdef WRPTPv2
	    issueWRSignalingMsg(CALIBRATED,rtOpts, ptpClock);
#else
	    issueWRManagement(CALIBRATED,rtOpts, ptpClock);
#endif	    
	    toWRState(WRS_CALIBRATED, rtOpts, ptpClock);
	    ptpClock->isCalibrated = TRUE;



    break;
  /**********************************  CAL_COMPLETED  ***************************************************************************/
  case WRS_CALIBRATED:
	    handle(rtOpts, ptpClock);

#ifdef WRPTPv2    
	    if(ptpClock->msgTmpWrMessageID == CALIBRATE && ptpClock->wrNodeMode == WR_MASTER)
	      toWRState(WRS_RESP_CALIB_REQ, rtOpts, ptpClock);
	    
	    if(ptpClock->msgTmpWrMessageID == WR_MODE_ON && ptpClock->wrNodeMode == WR_SLAVE)
	      toWRState(WRS_WR_LINK_ON, rtOpts, ptpClock);
#else
	    if(ptpClock->msgTmpManagementId == CALIBRATE && ptpClock->wrNodeMode == WR_MASTER)
	      toWRState(WRS_RESP_CALIB_REQ, rtOpts, ptpClock);

	    if(ptpClock->msgTmpManagementId == WR_MODE_ON && ptpClock->wrNodeMode == WR_SLAVE)
	      toWRState(WRS_WR_LINK_ON, rtOpts, ptpClock);
#endif
	    break;

/**********************************  WRS_RESP_CALIB_REQ  ***************************************************************************/
  case WRS_RESP_CALIB_REQ:


	  if( ptpd_netif_calibration_pattern_enable( 	ptpClock->netPath.ifaceName, \
							ptpClock->otherNodeCalibrationPeriod, \
							ptpClock->otherNodeCalibrationPattern, \
							ptpClock->otherNodeCalibrationPatternLen) == PTPD_NETIF_OK)
	    ptpClock->wrPortState = WRS_RESP_CALIB_REQ_1; //go to substate 1
	  else
	    break;   //try again

      //substate 1	- waiting for instruction from the master
      case WRS_RESP_CALIB_REQ_1 :

	    handle(rtOpts, ptpClock);
#ifdef WRPTPv2
	    if(ptpClock->msgTmpWrMessageID == CALIBRATED /* || timeout */)
#else	      
	    if(ptpClock->msgTmpManagementId == CALIBRATED /* || timeout */)
#endif	      
	    {
	      if(ptpClock->otherNodeCalibrationSendPattern ==  TRUE)
		ptpClock->wrPortState = WRS_RESP_CALIB_REQ_2;
	      else
		ptpClock->wrPortState = WRS_RESP_CALIB_REQ_3;
	    }
	    else
	      break; // try again

      //substate 2	- so the master finished, so we try to disable pattern, repeat if failed
      case WRS_RESP_CALIB_REQ_2 :

	     if(ptpd_netif_calibration_pattern_disable(ptpClock->netPath.ifaceName) == PTPD_NETIF_OK)
		ptpClock->wrPortState = WRS_RESP_CALIB_REQ_3;
	     else
		break; // try again

      case WRS_RESP_CALIB_REQ_3:

	  if(ptpClock->wrNodeMode == WR_MASTER)
	    toWRState(WRS_WR_LINK_ON, rtOpts, ptpClock);
	  else if(ptpClock->wrNodeMode == WR_SLAVE)
	    toWRState(WRS_REQ_CALIBRATION, rtOpts, ptpClock);
	  else
	  {
	    DBG("ERRRORROR!!!!!!!!!!\n");
	    toWRState(WRS_IDLE, rtOpts, ptpClock);
	   }

	break;

  /**********************************  WRS_WR_LINK_ON ***************************************************************************/
  case WRS_WR_LINK_ON:
	    /*
	     * While entering the state, we sent WR_MODE_ON to the Master and set isWRmode TRUE and assuming that Master
	     * is calibrated, set grandmaster to isWRmode and isCalibrated, see (toWRSlaveState())
	     */


	    if(ptpClock->wrNodeMode == WR_SLAVE)
	      toState(PTP_SLAVE, rtOpts, ptpClock);
	    else if(ptpClock->wrNodeMode == WR_MASTER)
	      toState(PTP_MASTER, rtOpts, ptpClock);
	    else
	      DBG("SHIT !!!\n");

	    toWRState(WRS_IDLE, rtOpts, ptpClock);

	    break;



   /**********************************  default  ***************************************************************************/
  default:

	    DBG("(doWhiteRabbitState) do unrecognized state\n");
	    break;
  }

  /*
   * we need to know "main state" to check if the timer expired,
   * timeouts are measured for main states only
   */
  UInteger8 currentState = returnCurrentWRMainState(ptpClock);

  /* handling timeouts globally, may chage state*/
  wrTimerExpired(currentState,rtOpts,ptpClock,ptpClock->wrNodeMode);

}



/* perform actions required when leaving 'wrPortState' and entering 'state' */
void toWRState(UInteger8 enteringState, RunTimeOpts *rtOpts, PtpClock *ptpClock)
{
  /*
   * this (exitingState) have to do with substates and timeouts,
   * if we exit one of substates, the value of exiting state
   * which is inputed into wrTimetoutManage() needs to be of
   * the main state since we calculate timeouts for main states
   */
  UInteger8 exitingState = returnCurrentWRMainState(ptpClock);

  /******** WR TIMEOUT STAFF **********
   * turn of timeout of exitingState
   * turn out timeout of enteringState
   */
  wrTimetoutManage(enteringState,exitingState,rtOpts,ptpClock);

  /* leaving state tasks */
  switch(ptpClock->wrPortState)
  {
  case WRS_IDLE:
    break;

  case WRS_PRESENT:
    break;

  case WRS_S_LOCK:
  case WRS_S_LOCK_1:
  case WRS_S_LOCK_2:
    break;

  case WRS_M_LOCK:
     break;

  case WRS_LOCKED:
     break;

   case WRS_REQ_CALIBRATION:
   case WRS_REQ_CALIBRATION_1:
   case WRS_REQ_CALIBRATION_2:
     break;

   case WRS_CALIBRATED:
     break;

   case WRS_WR_LINK_ON:

     DBG("*** WR Link is ON ***\n");

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
    DBG("state WRS_IDLE\n");

    ptpClock->wrPortState = WRS_IDLE;
    break;

  case WRS_PRESENT:
    /* no substates here*/
    DBG("state WRS_PRESENT\n");
    /*send message to the Master to enforce entering UNCALIBRATED state*/
    
#ifdef WRPTPv2
    issueWRSignalingMsg(SLAVE_PRESENT,rtOpts, ptpClock);
#else
    issueWRManagement(SLAVE_PRESENT,rtOpts, ptpClock);
#endif
    ptpClock->wrPortState = WRS_PRESENT;
    break;

  case WRS_S_LOCK:
    /* LOCK state implements 3 substates:
     * 0 - enable locking
     * 1 - locking enabled, polling
     * 2 - locked, disabling locking
     */
    DBG("state WR_LOCK (modded?)\n");


    if( ptpd_netif_locking_enable(ptpClock->wrNodeMode, ptpClock->netPath.ifaceName) == PTPD_NETIF_OK)
      ptpClock->wrPortState = WRS_S_LOCK_1; //go to substate 1
    else
     ptpClock->wrPortState = WRS_S_LOCK;   //stay in substate 0, try again

    DBG("state WR_LOCK (modded done?)\n");

    break;

  case WRS_LOCKED:
    /* no substates here*/
    DBG("state WR_LOCKED\n");

    /* say Master that you are locked */
#ifdef WRPTPv2
    issueWRSignalingMsg(LOCKED,rtOpts, ptpClock);
#else
    issueWRManagement(LOCKED,rtOpts, ptpClock);
#endif
    ptpClock->wrPortState = WRS_LOCKED;
    break;

  case WRS_M_LOCK:
    /* no substates here*/
    DBG("state WRS_M_LOCK\n");
#ifdef WRPTPv2
    issueWRSignalingMsg(LOCK,rtOpts, ptpClock);
#else
    issueWRManagement(LOCK,rtOpts, ptpClock);
#endif
    ptpClock->wrPortState = WRS_M_LOCK;
    break;


   case WRS_REQ_CALIBRATION:
    /* WRS_REQ_CALIBRATION state implements 3 substates:
     * 0 - enable calibration
     * 1 - calibration enabled, polling
     * 2 - HW finished calibration, disable calibration
     */
    DBG("state WRS_REQ_CALIBRATION\n");

    if( ptpClock->isCalibrated == TRUE)
    {
      /*
       * NO CALIBRATION NEEDED !!!!!
       * just go to the last step of this state
       * which is going to WRS_CALIBRATED
       */
#ifdef WRPTPv2
      issueWRSignalingMsg(CALIBRATE,rtOpts, ptpClock);
#else
      issueWRManagement(CALIBRATE,rtOpts, ptpClock);
#endif      
      ptpClock->wrPortState = WRS_REQ_CALIBRATION_2; // go to substate 1
      break;
    }

    //turn on calibration when entering state
    if(ptpd_netif_calibrating_enable(PTPD_NETIF_RX, ptpClock->netPath.ifaceName) == PTPD_NETIF_OK)
    {
      //successfully enabled calibration, inform master
#ifdef WRPTPv2
      issueWRSignalingMsg(CALIBRATE,rtOpts, ptpClock);
#else      
      issueWRManagement(CALIBRATE,rtOpts, ptpClock);
#endif      
      ptpClock->wrPortState = WRS_REQ_CALIBRATION_1; // go to substate 1
    }
    else
      //crap, probably calibration module busy with
      //calibrating other port, repeat attempt to enable calibration
      ptpClock->wrPortState = WRS_REQ_CALIBRATION;

    break;

  case WRS_CALIBRATED:
    DBG("state WRS_CALIBRATED\n");

    ptpClock->wrPortState = WRS_CALIBRATED;
    break;

  case WRS_RESP_CALIB_REQ:
    /* M_CALIBRATE state implements 3 substates:
     * 0 - enable pattern
     * 1 - pattern enabled, polling
     * 2 - MASTER_CALIBRATED received, disabling pattern
     * 3 -
     * 4 -
     */
    DBG("state WRS_RESP_CALIB_REQ\n");

    // to send the pattern or not to send
    // here is the answer to the question.....
    if(ptpClock->otherNodeCalibrationSendPattern == TRUE)
    {
      /*
       * the other node needs calibration, so
       * turn on calibration pattern
       */
	if( ptpd_netif_calibration_pattern_enable( ptpClock->netPath.ifaceName, \
				ptpClock->otherNodeCalibrationPeriod, \
				ptpClock->otherNodeCalibrationPattern, \
				ptpClock->otherNodeCalibrationPatternLen) == PTPD_NETIF_OK)
	  ptpClock->wrPortState = WRS_RESP_CALIB_REQ_1; //go to substate 1
	else
	  ptpClock->wrPortState = WRS_RESP_CALIB_REQ;   //try again

    }
    else
    {
      /*
       * the other node knows its fixed delays(deltaRx and deltaTx)
       * go straight to step 2 of this state: wait for CALIBRATED message
       */
	ptpClock->wrPortState = WRS_RESP_CALIB_REQ_1; //go to substate 1
    }
    break;

  case WRS_WR_LINK_ON:
    DBG("state WRS_LINK_ON\n");

    ptpClock->isWRmode = TRUE;

    if(ptpClock->wrNodeMode == WR_MASTER)
#ifdef WRPTPv2
      issueWRSignalingMsg(WR_MODE_ON,rtOpts, ptpClock);
#else      
      issueWRManagement(WR_MODE_ON,rtOpts, ptpClock);
#endif
    /*Assume that Master is calibrated and in WR mode, it will be verified with the next Annonce msg*/
    ptpClock->grandmasterIsWRmode     = TRUE;
    ptpClock->grandmasterIsCalibrated = TRUE;

    ptpClock->wrPortState = WRS_WR_LINK_ON;
    break;


  default:
    DBG("to unrecognized state\n");
    break;
  }


}

