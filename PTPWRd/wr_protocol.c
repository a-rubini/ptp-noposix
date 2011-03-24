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

  if(ptpd_netif_get_port_state(netPath->ifaceName) == PTPD_NETIF_OK)
    return TRUE;
  else
    return FALSE;

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
#ifdef NEW_SINGLE_WRFSM
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



#else /* NEW_SINGLE_WRFSM	 */

  switch(ptpClock->wrPortState)
  {
  case PTPWR_IDLE:
    state = PTPWR_IDLE;
    break;

  case PTPWR_PRESENT:
    state = PTPWR_PRESENT;
    break;

  case PTPWR_LOCK:
  case PTPWR_LOCK_1:
  case PTPWR_LOCK_2:

    state = PTPWR_LOCK;
    break;

   case PTPWR_LOCKED:

    state = PTPWR_LOCKED;
    break;

   case PTPWR_M_CALIBRATE:
   case PTPWR_M_CALIBRATE_1:
   case PTPWR_M_CALIBRATE_2:

     state = PTPWR_M_CALIBRATE;
     break;

   case PTPWR_S_CALIBRATE:
   case PTPWR_S_CALIBRATE_1:
   case PTPWR_S_CALIBRATE_2:

     state = PTPWR_S_CALIBRATE;
     break;

   case PTPWR_CAL_COMPLETED:

     state = PTPWR_CAL_COMPLETED;
     break;

    default:

     state = ptpClock->wrPortState;
     break;
  }
#endif
  return state;
}


#ifndef NEW_SINGLE_WRFSM
///////////////////////////// OLD FSMS  //////////////////////
/*
 * ***********  WR SLAVE FSM  *****************
 */


/* handle actions and events for 'wrPortState' */
void doWRSlaveState(RunTimeOpts *rtOpts, PtpClock *ptpClock)
{
  /*
   * here WR Slave FSM is implemented, please note that the implementation is
   * "unusual" (or nasty). there are substates to include the cases when
   * HW is not cooperative (returns: "no, I don't want to do that" and
   * we need to ask HW again, again and again, therefore substates
   *
   */


  UInteger8 state;
  uint64_t delta;

  switch(ptpClock->wrPortState)
  {

  case PTPWR_IDLE:

    //do nothing

    break;

  /**********************************  PRESENT  ***************************************************************************/
  case PTPWR_PRESENT:
    /*
     * message S_PRESENT sent to Master while entering state (toWRSlaveState())
     * here we wait for the answer from the Master asking us to LOCK
     */
    handle(rtOpts, ptpClock);

    if(ptpClock->msgTmpManagementId == LOCK)
    {
      toWRSlaveState(PTPWR_LOCK, rtOpts, ptpClock);
      // management message used, so clean tmp
      ptpClock->msgTmpManagementId = NULL_MANAGEMENT;
    }

    break;
  /**********************************  LOCK  ***************************************************************************/
  case PTPWR_LOCK:
     /*
      * Lockig slave's freq. to Master's. we need substates here to accommodate HW exceptions
      */

      //substate 0  	- locking_enable failed when called while entering this state (toWRSlaveState()) so we
      //		  we need to try again

        if( ptpd_netif_locking_enable(ptpClock->wrNodeMode, ptpClock->netPath.ifaceName) == PTPD_NETIF_OK)
	    ptpClock->wrPortState = PTPWR_LOCK_1; //success, go ahead

	break; //failed, try again

      //substate 1 	- polling HW
      case PTPWR_LOCK_1:

	 if(ptpd_netif_locking_poll(ptpClock->wrNodeMode, ptpClock->netPath.ifaceName) == PTPD_NETIF_READY)
	    ptpClock->wrPortState = PTPWR_LOCK_2; //next level achieved

	 break; //try again

      //substate 2 	- somehow, HW disagree to disable locking, so try again, and again...until timeout
      case PTPWR_LOCK_2:
	  if(ptpd_netif_locking_disable(ptpClock->wrNodeMode, ptpClock->netPath.ifaceName) == PTPD_NETIF_OK);
	    toWRSlaveState(PTPWR_LOCKED, rtOpts, ptpClock);
	  break;

  /**********************************  LOCKED  ***************************************************************************/
  case PTPWR_LOCKED:
	   /*
	    * message LOCKED was send while entering the state (toWRSlaveState())
	    * now we wait for instructions from the Master
	    */

	   handle(rtOpts, ptpClock);

	   if(ptpClock->msgTmpManagementId == MASTER_CALIBRATE)
	     toWRSlaveState(PTPWR_M_CALIBRATE, rtOpts, ptpClock);
	   else if(ptpClock->msgTmpManagementId == MASTER_CALIBRATED)
	   {
	      if(!ptpClock->isCalibrated)
		toWRSlaveState(PTPWR_S_CALIBRATE, rtOpts, ptpClock);
	      else
	      {
		issueWRManagement(SLAVE_CALIBRATED,rtOpts, ptpClock);
		toWRSlaveState(PTPWR_CAL_COMPLETED, rtOpts, ptpClock);
	      }
	   }
	   break;

  /**********************************  M_CALIBRATE  ***************************************************************************/
  case PTPWR_M_CALIBRATE:
      /*
       * While entering the state (toWRSlaveState()) we tried to enable calibtation pattern
       * if failed, we repeate, and than wait for the
       */
      //substate 0	- While entering the state (toWRSlaveState()) we tried to enable calibtation pattern
      //		  if failed, we repeat

	  if( ptpd_netif_calibration_pattern_enable( 	ptpClock->netPath.ifaceName, \
							ptpClock->otherNodeCalibrationPeriod, \
							ptpClock->otherNodeCalibrationPattern, \
							ptpClock->otherNodeCalibrationPatternLen) == PTPD_NETIF_OK)
	    ptpClock->wrPortState = PTPWR_M_CALIBRATE_1; //go to substate 1
	  else
	    break;   //try again

      //substate 1	- waiting for instruction from the master
      case PTPWR_M_CALIBRATE_1 :

	    handle(rtOpts, ptpClock);

	    //TODO: theortically, we need to calibrate for specific time (calibration period)
	    if(ptpClock->msgTmpManagementId == MASTER_CALIBRATED /* || timeout */)
	      ptpClock->wrPortState = PTPWR_M_CALIBRATE_2;
	    else
	      break; // try again

      //substate 2	- so the master finished, so we try to disable pattern, repeat if failed
      case PTPWR_M_CALIBRATE_2 :

	     if(ptpd_netif_calibration_pattern_disable(ptpClock->netPath.ifaceName) != PTPD_NETIF_OK)
		break; // try again

	     if(!ptpClock->isCalibrated)
	     {
		toWRSlaveState(PTPWR_S_CALIBRATE, rtOpts, ptpClock);
	     }
	     else
	     {
		issueWRManagement(SLAVE_CALIBRATED,rtOpts, ptpClock);
		toWRSlaveState(PTPWR_CAL_COMPLETED, rtOpts, ptpClock);
	     }

	     break;

  /**********************************  S_CALIBRATE  ***************************************************************************/
  case PTPWR_S_CALIBRATE:
	//substate 0	- first attempt to start calibration was while entering state (toWRSlaveState())
	//		  here we repeat if faild before

	    if(ptpd_netif_calibrating_enable(PTPD_NETIF_RX, ptpClock->netPath.ifaceName) == PTPD_NETIF_OK)
	    {
	      //reset timeout [??????????//]
	      timerStart(PTPWR_S_CALIBRATE,(float)ptpClock->wrTimeouts[PTPWR_S_CALIBRATE]/1000,ptpClock->wrtimer);
	      issueWRManagement(SLAVE_CALIBRATE,rtOpts, ptpClock);
	      ptpClock->wrPortState = PTPWR_S_CALIBRATE_1;
	    }
	    else
	      break; //try again

	//substate 1	- waiting for HW to finish measurement
	case PTPWR_S_CALIBRATE_1:

	    if(ptpd_netif_calibrating_poll(PTPD_NETIF_RX, ptpClock->netPath.ifaceName,&delta) == PTPD_NETIF_READY)
	    {
	      DBG("PTPWR_S_CALIBRATE_1: delta = 0x%x\n",delta);
	      ptpClock->deltaRx.scaledPicoseconds.msb = 0xFFFFFFFF & (delta >> 16);
	      ptpClock->deltaRx.scaledPicoseconds.lsb = 0xFFFFFFFF & (delta << 16);
	      DBG("scaledPicoseconds.msb = 0x%x\n",ptpClock->deltaRx.scaledPicoseconds.msb);
	      DBG("scaledPicoseconds.lsb = 0x%x\n",ptpClock->deltaRx.scaledPicoseconds.lsb);

	      ptpClock->wrPortState = PTPWR_S_CALIBRATE_2;
	    }
	    else
	      break; //try again

	//substate 2	- trying to disable calibration
	case PTPWR_S_CALIBRATE_2:

	    if( ptpd_netif_calibrating_disable(PTPD_NETIF_RX, ptpClock->netPath.ifaceName) != PTPD_NETIF_OK)
	      break; // try again

	    issueWRManagement(SLAVE_CALIBRATED,rtOpts, ptpClock);
	    toWRSlaveState(PTPWR_CAL_COMPLETED, rtOpts, ptpClock);
	    ptpClock->isCalibrated= TRUE;
    break;
  /**********************************  CAL_COMPLETED  ***************************************************************************/
  case PTPWR_CAL_COMPLETED:
	    /*
	     * While entering the state, we sent WR_MODE_ON to the Master and set isWRmode TRUE and assuming that Master
	     * is calibrated, set grandmaster to isWRmode and isCalibrated, see (toWRSlaveState())
	     */
	    toWRSlaveState(PTPWR_IDLE, rtOpts, ptpClock);
	    toState(PTP_SLAVE, rtOpts, ptpClock);
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
void toWRSlaveState(UInteger8 enteringState, RunTimeOpts *rtOpts, PtpClock *ptpClock)
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
  case PTPWR_IDLE:
    break;

  case PTPWR_PRESENT:
    break;

  case PTPWR_LOCK:
  case PTPWR_LOCK_1:
  case PTPWR_LOCK_2:
    break;

  case PTPWR_LOCKED:
     break;

   case PTPWR_M_CALIBRATE:
   case PTPWR_M_CALIBRATE_1:
   case PTPWR_M_CALIBRATE_2:
     break;

   case PTPWR_S_CALIBRATE:
   case PTPWR_S_CALIBRATE_1:
   case PTPWR_S_CALIBRATE_2:
     break;

   case PTPWR_CAL_COMPLETED:

     DBG("\n\n\n  ====== FINISHED WHITE RABBIT **SLAVE**  CALIBRATION  =====\n\n\n");

     break;

  default:

    break;
  }



  /* entering state tasks */
  /*No need of PRE_MASTER state because of only ordinary clock implementation*/

  switch(enteringState)
  {
  case PTPWR_IDLE:
    /* no substates here*/
    DBG("state PTPWR_IDLE\n");

    ptpClock->wrPortState = PTPWR_IDLE;
    break;

  case PTPWR_PRESENT:
    /* no substates here*/
    DBG("state WR_PRESENT\n");
    DBG("\n\n\n  ====== START WHITE RABBIT **SLAVE**  CALIBRATION  =====\n\n\n");

    /*send message to the Master to enforce entering UNCALIBRATED state*/
    issueWRManagement(SLAVE_PRESENT,rtOpts, ptpClock);

    ptpClock->wrPortState = PTPWR_PRESENT;
    break;

  case PTPWR_LOCK:
    /* LOCK state implements 3 substates:
     * 0 - enable locking
     * 1 - locking enabled, polling
     * 2 - locked, disabling locking
     */
    DBG("state WR_LOCK\n");

    if( ptpd_netif_locking_enable(ptpClock->wrNodeMode, ptpClock->netPath.ifaceName) == PTPD_NETIF_OK)
      ptpClock->wrPortState = PTPWR_LOCK_1; //go to substate 1
    else
      ptpClock->wrPortState = PTPWR_LOCK;   //stay in substate 0, try again
    break;

  case PTPWR_LOCKED:
    /* no substates here*/
    DBG("state WR_LOCKED\n");

    /* say Master that you are locked */
    issueWRManagement(LOCKED,rtOpts, ptpClock);

    ptpClock->wrPortState = PTPWR_LOCKED;
    break;

  case PTPWR_M_CALIBRATE:
    /* M_CALIBRATE state implements 3 substates:
     * 0 - enable pattern
     * 1 - pattern enabled, polling
     * 2 - MASTER_CALIBRATED received, disabling pattern
     */
    DBG("state WR_M_CALIBRATE\n");

    if( ptpd_netif_calibration_pattern_enable( ptpClock->netPath.ifaceName, \
					   ptpClock->otherNodeCalibrationPeriod, \
					   ptpClock->otherNodeCalibrationPattern, \
					   ptpClock->otherNodeCalibrationPatternLen) == PTPD_NETIF_OK)
      ptpClock->wrPortState = PTPWR_M_CALIBRATE_1; //go to substate 1
    else
      ptpClock->wrPortState = PTPWR_M_CALIBRATE;   //try again

    break;

   case PTPWR_S_CALIBRATE:
    /* S_CALIBRATE state implements 3 substates:
     * 0 - enable calibration
     * 1 - calibration enabled, polling
     * 2 - HW finished calibration, disable calibration
     */
    DBG("state WR_S_CALIBRATE\n");

    //turn on calibration when entering state
    if(ptpd_netif_calibrating_enable(PTPD_NETIF_RX, ptpClock->netPath.ifaceName) == PTPD_NETIF_OK)
    {
      //successfully enabled calibration, inform master
      issueWRManagement(SLAVE_CALIBRATE,rtOpts, ptpClock);
      ptpClock->wrPortState = PTPWR_S_CALIBRATE_1; // go to substate 1
    }
    else
      //crap, probably calibration module busy with
      //calibrating other port, repeat attempt to enable calibration
      ptpClock->wrPortState = PTPWR_S_CALIBRATE;

    break;


  case PTPWR_CAL_COMPLETED:
    DBG("state WR_CAL_COMPLETED\n");

     ptpClock->isWRmode = TRUE;

    issueWRManagement(WR_MODE_ON,rtOpts, ptpClock);

    /*Assume that Master is calibrated and in WR mode, it will be verified with the next Annonce msg*/
    ptpClock->grandmasterIsWRmode     = TRUE;
    ptpClock->grandmasterIsCalibrated = TRUE;

    ptpClock->wrPortState = PTPWR_CAL_COMPLETED;
    break;


  default:
    DBG("to unrecognized state\n");
    break;
  }


}

/*
 * ***********  WR MASTER FSM  *****************
 */


/* handle actions and events for 'wrPortState' */
void doWRMasterState(RunTimeOpts *rtOpts, PtpClock *ptpClock)
{
  UInteger8 state;

  uint64_t delta;

  switch(ptpClock->wrPortState)
  {

  /**********************************  IDLE  ***************************************************************************/
  case PTPWR_IDLE:

      //do nothing

      break;
  /**********************************  LOCK  ***************************************************************************/
  case PTPWR_LOCK:
      /*
       * LOCK message sent to slave while entrin the state, now wait for LOCKED msg from the slave
       */
      handle(rtOpts, ptpClock);

      if(ptpClock->msgTmpManagementId == LOCKED)
	toWRMasterState(PTPWR_LOCKED, rtOpts, ptpClock);

      break;
  /**********************************  LOCKED  ***************************************************************************/
  case PTPWR_LOCKED:
      /*
       *  If calibration needed, go to state M_CALIBRATE,
       *  if no need of calibration, tell slave your deltas and
       *  go to slave calibration
       */

      /* we are not calibrated and need calibration*/
      if(!ptpClock->isCalibrated)
      {

	//issueWRManagement(MASTER_CALIBRATE,rtOpts,ptpClock);//put it to "enter calib_m state"
	toWRMasterState(PTPWR_M_CALIBRATE, rtOpts, ptpClock);
	break;
      }
      else
      {
	issueWRManagement(MASTER_CALIBRATED,rtOpts,ptpClock);
	toWRMasterState(PTPWR_S_CALIBRATE, rtOpts, ptpClock);
	break;
      }

      break;
  /**********************************  M_CALIBRATE  ***************************************************************************/
  case PTPWR_M_CALIBRATE:
      /*
       * while entering this state, calibration was attempted to be staretd,
       * in this state master Rx delta is calibrated
       */
	//substate 0	- first attempt to start calibration was while entering state (toWRMasterState())
	//		  here we repeat if faild before

	      if(ptpd_netif_calibrating_enable(PTPD_NETIF_RX, ptpClock->netPath.ifaceName) == PTPD_NETIF_OK)
	      {
		//successfully enabled calibration, inform master
		issueWRManagement(MASTER_CALIBRATE,rtOpts,ptpClock);
		ptpClock->wrPortState = PTPWR_M_CALIBRATE_1; // go to substate 1
	      }
	      else
		break; //try again

	//substate 1	- waiting for HW to finish measurement
	case PTPWR_M_CALIBRATE_1:


	      if(ptpd_netif_calibrating_poll(PTPD_NETIF_RX, ptpClock->netPath.ifaceName,&delta) == PTPD_NETIF_READY)
	      {
		  DBG("delta = 0x%x\n",delta);
		  ptpClock->deltaRx.scaledPicoseconds.msb = 0xFFFFFFFF & (delta >> 16);
		  ptpClock->deltaRx.scaledPicoseconds.lsb = 0xFFFFFFFF & (delta << 16);
		  DBG("scaledPicoseconds.msb = 0x%x\n",ptpClock->deltaRx.scaledPicoseconds.msb);
		  DBG("scaledPicoseconds.lsb = 0x%x\n",ptpClock->deltaRx.scaledPicoseconds.lsb);

		  ptpClock->wrPortState = PTPWR_M_CALIBRATE_2;
	      }
	      else
		break; // try again

	//substate 2	- trying to disable calibration
	case PTPWR_M_CALIBRATE_2:

	      if( ptpd_netif_calibrating_disable(PTPD_NETIF_RX, ptpClock->netPath.ifaceName) != PTPD_NETIF_OK)
		break; // try again

	      issueWRManagement(MASTER_CALIBRATED,rtOpts,ptpClock);
	      ptpClock->isCalibrated= TRUE;
	      toWRMasterState(PTPWR_S_CALIBRATE, rtOpts, ptpClock);

	      break;
  /**********************************  S_CALIBRATE  ***************************************************************************/
  case PTPWR_S_CALIBRATE:
      /*
       * here we listen to Slave's wishes,
       * Slave may say that it's calibrated or may request calibration pattern
       */
     //substate 0	- wait for msg from the slave

	  handle(rtOpts, ptpClock);

	  if(ptpClock->msgTmpManagementId == SLAVE_CALIBRATE /* || timeout */)
	  {
	      ptpClock->wrPortState = PTPWR_S_CALIBRATE_1; //go to substate 1 and repeate enabling
	  }
	  else if(ptpClock->msgTmpManagementId == SLAVE_CALIBRATED /* || timeout */)
	  {
	    if(ptpd_netif_calibration_pattern_disable(ptpClock->netPath.ifaceName) == PTPD_NETIF_OK )
	    {
	      toWRMasterState(PTPWR_CAL_COMPLETED, rtOpts, ptpClock);
	      break;
	    }
	    else
	    {
	      ptpClock->wrPortState = PTPWR_S_CALIBRATE_3; //go to substate 2
	      break; // try again
	    }
	  }
	  else
	    break; // try again

     //substate 1	- try to enable pattern
	case PTPWR_S_CALIBRATE_1:

	   if(ptpd_netif_calibration_pattern_enable( ptpClock->netPath.ifaceName, \
						     ptpClock->otherNodeCalibrationPeriod, \
						     ptpClock->otherNodeCalibrationPattern, \
						     ptpClock->otherNodeCalibrationPatternLen )== PTPD_NETIF_OK)
	   {
	      ptpClock->wrPortState = PTPWR_S_CALIBRATE_2; //go to substate 2
	   }
	   else
	      break; //try again

     //substate 2	- wait for CALIBRATED msg from slave
	case PTPWR_S_CALIBRATE_2:

	  handle(rtOpts, ptpClock);

	  if(ptpClock->msgTmpManagementId == SLAVE_CALIBRATED /* || timeout */)
	  {
	      ptpClock->wrPortState = PTPWR_S_CALIBRATE_3; //go to substate 2
	  }
	  else
	    break; // try again

     //substate 3	- try disabling calibration pattern
	case PTPWR_S_CALIBRATE_3:

	  if(ptpd_netif_calibration_pattern_disable(ptpClock->netPath.ifaceName) == PTPD_NETIF_OK )
	  {
	    toWRMasterState(PTPWR_CAL_COMPLETED, rtOpts, ptpClock);
	  }
	  break;  // by again

  /**********************************  COMPLETED  ***************************************************************************/
  case PTPWR_CAL_COMPLETED:
    /*
     * we wait for info from Slave, it should confirm that everything went perfectly fine
     */
    handle(rtOpts, ptpClock);

    if(ptpClock->msgTmpManagementId == WR_MODE_ON)
    {
      toWRMasterState(PTPWR_IDLE, rtOpts, ptpClock);
      toState(PTP_MASTER, rtOpts, ptpClock);
      ptpClock->isWRmode = TRUE;
      issueAnnounce(rtOpts, ptpClock);
    }

    break;

  /**********************************  PRESENT or default********************************************************************/
  case PTPWR_PRESENT:
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
void toWRMasterState(UInteger8 enteringState, RunTimeOpts *rtOpts, PtpClock *ptpClock)
{

  /*
   * this (exitingState) have to do with substates and timeouts,
   * if we exit one of substates, the value of exiting state
   * which is inputed into wrTimetoutManage() needs to be of
   * the main state since we calculate timeouts for main states
   */
  UInteger8 exitingState = returnCurrentWRMainState(ptpClock);

  /****** WR TIMEOUT STAFF ************
   * turn off timer for exitingState
   * turn on  timer for enteringState
   */
  wrTimetoutManage(enteringState,exitingState,rtOpts,ptpClock);

  /* leaving state tasks */
  switch(ptpClock->wrPortState)
  {
  case PTPWR_IDLE:
    break;

  case PTPWR_PRESENT:
    break;

  case PTPWR_LOCK:
  case PTPWR_LOCK_1:
  case PTPWR_LOCK_2:
    break;

   case PTPWR_LOCKED:
    break;

   case PTPWR_M_CALIBRATE:
   case PTPWR_M_CALIBRATE_1:
   case PTPWR_M_CALIBRATE_2:
     break;

   case PTPWR_S_CALIBRATE:
   case PTPWR_S_CALIBRATE_1:
   case PTPWR_S_CALIBRATE_2:
     break;

   case PTPWR_CAL_COMPLETED:

     DBG("\n\n\n  ====== FINISHED WHITE RABBIT **MASTER**  CALIBRATION  =====\n\n\n");

     break;

  default:

    break;
  }




  /* entering state tasks */
  /*No need of PRE_MASTER state because of only ordinary clock implementation*/
  switch(enteringState)
  {
  case PTPWR_IDLE:
    /* no substates here*/
    DBG("state PTPWR_IDLE\n");

    ptpClock->wrPortState = PTPWR_IDLE;
    break;

  case PTPWR_LOCK:
     /* no substates here*/
    DBG("state WR_LOCK\n");

    //DBG("\n\n\n  ====== START WHITE RABBIT **MASTER**  CALIBRATION  =====\n\n\n");
    printf("\n\n\n  ====== START WHITE RABBIT **MASTER**  CALIBRATION  =====\n\n\n");

    //ptpd_netif_locking_enable(ptpClock->wrNodeMode, ptpClock->netPath.ifaceName);
    issueWRManagement(LOCK,rtOpts,ptpClock);

    ptpClock->wrPortState = PTPWR_LOCK;
    break;

  case PTPWR_LOCKED:
     /* no substates here*/
    DBG("state WR_LOCKED\n");

    ptpClock->wrPortState = PTPWR_LOCKED;
    break;

  case PTPWR_M_CALIBRATE:
    /* M_CALIBRATE state implements 3 substates:
     * 0 - enable calibration
     * 1 - calibration enabled, polling
     * 2 - HW finished calibration, disable calibration
     */
    DBG("state WR_M_CALIBRATE\n");

    if(ptpd_netif_calibrating_enable(PTPD_NETIF_RX, ptpClock->netPath.ifaceName) == PTPD_NETIF_OK)
    {
      //successfully enabled calibration, inform master
      issueWRManagement(MASTER_CALIBRATE,rtOpts,ptpClock);
      ptpClock->wrPortState = PTPWR_M_CALIBRATE_1; // go to substate 1
    }
    else
      //crap, probably calibration module busy with
      //calibrating other port, repeat attempt to enable calibration
      ptpClock->wrPortState = PTPWR_M_CALIBRATE;

    break;

   case PTPWR_S_CALIBRATE:
    /* S_CALIBRATE state implements 3 substates:
     * 0 - wait for msg from WR Slave (CALIBRATE or CALIBRATED)
     * 1 - pattern enabled,
     * 2 - wait for CALIBRATED
     * 3 - disabling pattern
     */
    DBG("state WR_S_CALIBRATE\n");


    ptpClock->wrPortState = PTPWR_S_CALIBRATE;
    break;


  case PTPWR_CAL_COMPLETED:
    DBG("state WR_CAL_COMPLETED\n");

    ptpClock->wrPortState = PTPWR_CAL_COMPLETED;
    break;

  case PTPWR_PRESENT:
  default:
    DBG("to unrecognized state\n");
    break;
  }


}

#endif /* NEW_SINGLE_WRFSM  */

/*
polls HW for TX timestamp
@return
    TRUE  	- if timestamp read,
    FALSE	- otherwise
*/
Boolean getWRtxTimestamp(RunTimeOpts *rtOpts, PtpClock *ptpClock)
{
/*

    if(ptpClock->pending_Synch_tx_ts)
    {
     if(ptpd_netif_poll_tx_timestamp(ptpClock->netPath.wrSock, ptpClock->synch_tx_tag, &(ptpClock->synch_tx_ts)) == PTPD_NETIF_OK)
     {
       DBG("\tsync msg: TX timestamp[tx_ts_tag = %d ]: %s \n",ptpClock->synch_tx_tag, format_wr_timestamp(ptpClock->synch_tx_ts));
       issueFollowup(rtOpts,ptpClock);     //sends   current_tx_ts
       ptpClock->pending_Synch_tx_ts = FALSE;
     }
    }


    if(ptpClock->pending_DelayReq_tx_ts )
    {
     if(ptpd_netif_poll_tx_timestamp(ptpClock->netPath.wrSock, ptpClock->delayReq_tx_tag, &(ptpClock->delayReq_tx_ts)) == PTPD_NETIF_OK)
     {
       	Timestamp tmp;
	tmp.secondsField.msb     = ptpClock->delayReq_tx_ts.seconds.hi;
	tmp.secondsField.lsb     = ptpClock->delayReq_tx_ts.seconds.lo;
	tmp.nanosecondsField     = ptpClock->delayReq_tx_ts.nanoseconds;

	DBG("\tdelayReq msg: TX timestamp[tx_ts_tag = %d ]: %s \n",ptpClock->delayReq_tx_tag, format_wr_timestamp(ptpClock->delayReq_tx_ts));
	toInternalTime(&ptpClock->delay_req_send_time, &tmp);

	ptpClock->pending_DelayReq_tx_ts = FALSE;
     }
   }


   if(!rtOpts->E2E_mode && ptpClock->pending_PDelayReq_tx_ts)
   {
     if(ptpd_netif_poll_tx_timestamp(ptpClock->netPath.wrSock, ptpClock->pDelayReq_tx_tag, &(ptpClock->pDelayReq_tx_ts)) == PTPD_NETIF_OK)
     {
       	Timestamp tmp;
	tmp.secondsField.msb     = ptpClock->pDelayReq_tx_ts.seconds.hi;
	tmp.secondsField.lsb     = ptpClock->pDelayReq_tx_ts.seconds.lo;
	tmp.nanosecondsField     = ptpClock->pDelayReq_tx_ts.nanoseconds;


	DBG("\tpDelayReq msg: TX timestamp[tx_ts_tag = %d ]: %s \n",ptpClock->pDelayReq_tx_tag, format_wr_timestamp(ptpClock->pDelayReq_tx_ts));
	toInternalTime(&ptpClock->pdelay_req_send_time, &tmp);

	ptpClock->pending_PDelayReq_tx_ts =  FALSE;
     }
   }


   if(!rtOpts->E2E_mode && ptpClock->pending_PDelayResp_tx_ts)
   {
     if(ptpd_netif_poll_tx_timestamp(ptpClock->netPath.wrSock, ptpClock->pDelayResp_tx_tag, &(ptpClock->pDelayResp_tx_ts)) == PTPD_NETIF_OK)
     {
       	Timestamp tmp;
	tmp.secondsField.msb     = ptpClock->pDelayResp_tx_ts.seconds.hi;
	tmp.secondsField.lsb     = ptpClock->pDelayResp_tx_ts.seconds.lo;
	tmp.nanosecondsField     = ptpClock->pDelayResp_tx_ts.nanoseconds;


	DBG("\tpDelayReq msg: TX timestamp[tx_ts_tag = %d ]: %s \n",ptpClock->pDelayResp_tx_tag, format_wr_timestamp(ptpClock->pDelayResp_tx_ts));
	toInternalTime(&ptpClock->pdelay_resp_send_time, &tmp);

	ptpClock->pending_PDelayResp_tx_ts =  FALSE;
	issuePDelayRespFollowUp(time,&ptpClock->PdelayReqHeader,rtOpts,ptpClock);
      }
    }

   if(ptpClock->pending_Synch_tx_ts       || \
      ptpClock->pending_DelayReq_tx_ts    || \
      ptpClock->pending_PDelayReq_tx_ts   || \
      ptpClock->pending_PDelayResp_tx_ts     \
     )
    return TRUE; //ptpClock->pending_tx_ts
   else
    return FALSE; //ptpClock->pending_tx_ts
*/
return TRUE;
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
#ifdef NEW_SINGLE_WRFSM
  if(exitingState != WRS_IDLE)
    timerStop(exitingState,ptpClock->wrtimer);

  /*start timer in the state you are entering (except IDLE) */
  if(enteringState != WRS_IDLE)
    timerStart(enteringState,(float)ptpClock->wrTimeouts[enteringState]/1000,ptpClock->wrtimer);
#else
  if(exitingState != PTPWR_IDLE)
    timerStop(exitingState,ptpClock->wrtimer);

  /*start timer in the state you are entering (except IDLE) */
  if(enteringState != PTPWR_IDLE)
    timerStart(enteringState,(float)ptpClock->wrTimeouts[enteringState]/1000,ptpClock->wrtimer);

#endif

}
/*
this function checks if wr timer has expired for a current WR state

*/
void wrTimerExpired(UInteger8 currentState, RunTimeOpts *rtOpts, PtpClock *ptpClock, Enumeration8 wrNodeMode)
{

  if(timerExpired(currentState,ptpClock->wrtimer,ptpClock->portIdentity.portNumber ))
  {
      if (ptpClock->currentWRstateCnt < WR_DEFAULT_STATE_REPEAT )
      {
	DBG("WR_Slave_TIMEOUT: state[= %d] timeout, repeat state\n", currentState);
#ifdef NEW_SINGLE_WRFSM
	toWRState(currentState, rtOpts, ptpClock);
#else
	toWRSlaveState(currentState, rtOpts, ptpClock);
#endif
      }
      else
      {
	DBG("WR_Slave_TIMEOUT: state[=%d] timeout, repeated %d times, going to Standard PTP\n", currentState,ptpClock->currentWRstateCnt );
	ptpClock->isWRmode = FALSE;
#ifdef NEW_SINGLE_WRFSM
        toWRState(WRS_IDLE, rtOpts, ptpClock);
#else
	toWRSlaveState(PTPWR_IDLE, rtOpts, ptpClock);
#endif
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
  uint64_t deltaTx, deltaRx;
  int i;
  int ret;
  /*
   * check if Rx & Tx delays known
   * on this interface, this would mean
   * that the demon was restarted
   * or deterministic HW used
   *
   * otherwise, calibrate Rx
   */

  /* if( ptpd_netif_read_calibration_data(ifaceName, &deltaTx, &deltaRx) == PTPD_NETIF_OK)
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
  char buf[64];
  int i;
  for(i = 0; i < 32; i++)
    if((bits >> i) & 0x1)
      buf[31 - i] = '1';
    else
      buf[31 - i] = '0';

  buf[i]='\0';

  return strdup(buf);
}


/*prints RAW timestamp*/
char *format_wr_timestamp(wr_timestamp_t ts)
{
  char buf[64];

  snprintf(buf,64, "sec: %lld nsec: %lld ", (uint64_t)ts.utc,(uint32_t) ts.nsec);

  return strdup(buf);
}




#ifdef NEW_SINGLE_WRFSM
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


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


  UInteger8 state;
  uint64_t delta;

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
    handle(rtOpts, ptpClock);
    if(ptpClock->msgTmpManagementId == LOCK)
    {


      toWRState(WRS_S_LOCK, rtOpts, ptpClock);
      // management message used, so clean tmp
      ptpClock->msgTmpManagementId = NULL_MANAGEMENT;
    }

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

	  if(ptpClock->msgTmpManagementId == LOCKED)
	    toWRState(WRS_REQ_CALIBRATION, rtOpts, ptpClock);


	  break;
  /**********************************  LOCKED  ***************************************************************************/
  case WRS_LOCKED:


	   handle(rtOpts, ptpClock);

	   if(ptpClock->msgTmpManagementId == CALIBRATE)
	     toWRState(WRS_RESP_CALIB_REQ, rtOpts, ptpClock);

	   break;

  /**********************************  S_CALIBRATE  ***************************************************************************/
  case WRS_REQ_CALIBRATION:
	//substate 0	- first attempt to start calibration was while entering state (toWRSlaveState())
	//		  here we repeat if faild before

	    if(ptpd_netif_calibrating_enable(PTPD_NETIF_RX, ptpClock->netPath.ifaceName) == PTPD_NETIF_OK)
	    {
	      //reset timeout [??????????//]
	      timerStart(WRS_REQ_CALIBRATION,(float)ptpClock->wrTimeouts[WRS_REQ_CALIBRATION]/1000,ptpClock->wrtimer);
	      issueWRManagement(CALIBRATE,rtOpts, ptpClock);
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

	    issueWRManagement(CALIBRATED,rtOpts, ptpClock);
	    toWRState(WRS_CALIBRATED, rtOpts, ptpClock);
	    ptpClock->isCalibrated= TRUE;



    break;
  /**********************************  CAL_COMPLETED  ***************************************************************************/
  case WRS_CALIBRATED:
	    handle(rtOpts, ptpClock);

	    if(ptpClock->msgTmpManagementId == CALIBRATE && ptpClock->wrNodeMode == WR_MASTER)
	      toWRState(WRS_RESP_CALIB_REQ, rtOpts, ptpClock);

	    if(ptpClock->msgTmpManagementId == WR_MODE_ON && ptpClock->wrNodeMode == WR_SLAVE)
	      toWRState(WRS_WR_LINK_ON, rtOpts, ptpClock);

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

	    if(ptpClock->msgTmpManagementId == CALIBRATED /* || timeout */)
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
	      DBG("DUPA !!!\n");

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

     DBG("\n\n\n\
     ======         FINISHED SUCCESSFULLY             ===== \n\
     ====== ****** WHITE RABBIT LINK SETUP ***********===== \n\
     ====== HAVE FUN WITH SUB-NANOSECOND PRECISION !! =====\n\n\n");

     printf("====== ****** WHITE RABBIT LINK SETUP ***********===== \n");

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
    DBG("\n\n\n\
    ======        STARTING (SLAVE)....               ===== \n\
    ====== ****** WHITE RABBIT LINK SETUP ***********=====\n\n\n");

    printf("======        STARTING (SLAVE)....               ===== \n");

    /*send message to the Master to enforce entering UNCALIBRATED state*/
    issueWRManagement(SLAVE_PRESENT,rtOpts, ptpClock);

    ptpClock->wrPortState = WRS_PRESENT;
    break;

  case WRS_S_LOCK:
    /* LOCK state implements 3 substates:
     * 0 - enable locking
     * 1 - locking enabled, polling
     * 2 - locked, disabling locking
     */
    DBG("state WR_LOCK\n");

    if( ptpd_netif_locking_enable(ptpClock->wrNodeMode, ptpClock->netPath.ifaceName) == PTPD_NETIF_OK)
      ptpClock->wrPortState = WRS_S_LOCK_1; //go to substate 1
    else
     ptpClock->wrPortState = WRS_S_LOCK;   //stay in substate 0, try again
    break;

  case WRS_LOCKED:
    /* no substates here*/
    DBG("state WR_LOCKED\n");

    /* say Master that you are locked */
    issueWRManagement(LOCKED,rtOpts, ptpClock);

    ptpClock->wrPortState = WRS_LOCKED;
    break;

  case WRS_M_LOCK:
    /* no substates here*/
    DBG("state WRS_M_LOCK\n");
    DBG("\n\n\n  \
    ======        STARTING (MASTER)....              ===== \n \
    ====== ****** WHITE RABBIT LINK SETUP ***********=====\n\n\n");

    printf("======        STARTING (MASTER)....               ===== \n");

    issueWRManagement(LOCK,rtOpts, ptpClock);

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
      issueWRManagement(CALIBRATE,rtOpts, ptpClock);
      ptpClock->wrPortState = WRS_REQ_CALIBRATION_2; // go to substate 1
      break;
    }

    //turn on calibration when entering state
    if(ptpd_netif_calibrating_enable(PTPD_NETIF_RX, ptpClock->netPath.ifaceName) == PTPD_NETIF_OK)
    {
      //successfully enabled calibration, inform master
      issueWRManagement(CALIBRATE,rtOpts, ptpClock);
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
      issueWRManagement(WR_MODE_ON,rtOpts, ptpClock);

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


#endif /* NEW_SINGLE_WRFSM */
