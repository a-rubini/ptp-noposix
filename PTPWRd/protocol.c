/* protocol.c */

#include "ptpd.h"

#include "ptpd_netif.h"


Boolean doInit(RunTimeOpts*,PtpClock*);
void doState(RunTimeOpts*,PtpClock*);
void toState(UInteger8,RunTimeOpts*,PtpClock*);

void handle(RunTimeOpts*,PtpClock*);
void handleAnnounce(MsgHeader*,Octet*,ssize_t,Boolean,RunTimeOpts*,PtpClock*);
void handleSync(MsgHeader*,Octet*,ssize_t,TimeInternal*,Boolean,RunTimeOpts*,PtpClock*);
void handleDelayReq(MsgHeader*,Octet*,ssize_t,Boolean,RunTimeOpts*,PtpClock*);
void handleManagement(MsgHeader*,Octet*,ssize_t,Boolean,RunTimeOpts*,PtpClock*);
void issueAnnounce(RunTimeOpts*,PtpClock*);
void issueSync(RunTimeOpts*,PtpClock*);
void issueFollowup(RunTimeOpts*,PtpClock*);
void issueDelayReq(RunTimeOpts*,PtpClock*);
void issueWRManagement(Enumeration16 wr_managementId,RunTimeOpts*,PtpClock*);
void addForeign(Octet*,MsgHeader*,PtpClock*);



void handlePDelayReq(MsgHeader*,Octet*,ssize_t,TimeInternal*,Boolean,RunTimeOpts*,PtpClock*);
void handlePDelayResp(MsgHeader*,Octet*,TimeInternal* ,ssize_t,Boolean,RunTimeOpts*,PtpClock*);
void handleDelayResp(MsgHeader*,Octet*,ssize_t,Boolean,RunTimeOpts*,PtpClock*);
void handlePDelayRespFollowUp(MsgHeader*,Octet*,ssize_t,Boolean,RunTimeOpts*,PtpClock*);
void handleSignaling(MsgHeader*,Octet*,ssize_t,Boolean,RunTimeOpts*,PtpClock*);
void issuePDelayReq(RunTimeOpts*,PtpClock*);
void issuePDelayResp(TimeInternal*,MsgHeader*,RunTimeOpts*,PtpClock*);
void issueDelayResp(MsgHeader*,RunTimeOpts*,PtpClock*);
void issuePDelayRespFollowUp(TimeInternal*,MsgHeader*,RunTimeOpts*,PtpClock*);
void issueManagement(MsgHeader*,MsgManagement*,RunTimeOpts*,PtpClock*);

/* The code used pow(2, ...) but we don't want floating point here (ARub) */
static inline unsigned long pow_2(int exp)
{
	return 1 << exp;
}





//added by ML
/*
 * implementation of multi-port daemon
 * each port independant
 * at the moment:
 * - not much tested
 * TODO:
 * - only one port is allowed to calibrate at a time, need to implement some synch of that
 * - test
 */
void multiProtocol(RunTimeOpts *rtOpts, PtpClock *ptpClock)
 {

  int           i;
  PtpClock *    currentPtpClockData;

  currentPtpClockData = ptpClock;

  for (i=0; i < rtOpts->portNumber; i++)
  {
     DBGV("multiPortProtocol: initializing port %d\n", (i+1));
     toState(PTP_INITIALIZING, rtOpts, currentPtpClockData);
     if(!doInit(rtOpts, currentPtpClockData))
     {
       // doInit Failed!  Exit
       DBG("\n--------------------------------------------------\n---------------- port %d failed to doInit()-----------------------------\n--------------------------------\n",(i+1));
       //return;
     }
     currentPtpClockData++;
  }

  for(;;)
  {
    currentPtpClockData = ptpClock;

    for (i=0; i < rtOpts->portNumber; i++)
    {
      /*
      fixme:

      if(currentPtpClockData->wrNodeMode == WR_MASTER && currentPtpClockData->portIdentity.portNumber  == 7 || \
	 currentPtpClockData->wrNodeMode == WR_SLAVE  && currentPtpClockData->portIdentity.portNumber  == 10  )
      */
      do
      {
      /*
       * perform calibration for a give port
       * we can only calibrate one port at a time (HW limitation)
       * we want the calibration to be performed as quick as possible
       * and without disturbance, so don't perform doState for other ports
       * for the time of calibration
       */

	if(currentPtpClockData->portState != PTP_INITIALIZING)
	  doState(rtOpts, currentPtpClockData);
	else if(!doInit(rtOpts, currentPtpClockData))
	  return;

	if(currentPtpClockData->message_activity)
	  DBGV("activity\n");
	else
	  DBGV("no activity\n");

	#ifdef IRQ_LESS_TIMER
	  do_irq_less_timing(currentPtpClockData);
	#endif
      }
      while(currentPtpClockData->portState == PTP_UNCALIBRATED);

      currentPtpClockData++;
    }

  }



 }

/* loop forever. doState() has a switch for the actions and events to be
   checked for 'port_state'. the actions and events may or may not change
   'port_state' by calling toState(), but once they are done we loop around
   again and perform the actions required for the new 'port_state'. */
void protocol(RunTimeOpts *rtOpts, PtpClock *ptpClock)
{
  DBG("event POWERUP\n");

  toState(PTP_INITIALIZING, rtOpts, ptpClock);

  for(;;)
  {
//		fprintf(stderr, ".");
    ptpd_handle_wripc();

    if(ptpClock->portState != PTP_INITIALIZING)
      doState(rtOpts, ptpClock);
    else if(!doInit(rtOpts, ptpClock))
      return;


    if(ptpClock->message_activity)
      DBGV("activity\n");
    else
      DBGV("no activity\n");

    #ifdef IRQ_LESS_TIMER
    do_irq_less_timing(ptpClock);
    #endif


  }
}


/*
 * perform actions required when leaving 'port_state' and entering 'state'
 */
void toState(UInteger8 state, RunTimeOpts *rtOpts, PtpClock *ptpClock)
{

  ptpClock->message_activity = TRUE;

  /**
    leaving state tasks
  **/
  switch(ptpClock->portState)
  {
  case PTP_MASTER:
    timerStop(SYNC_INTERVAL_TIMER, ptpClock->itimer);
    timerStop(ANNOUNCE_INTERVAL_TIMER, ptpClock->itimer);
    timerStop(PDELAYREQ_INTERVAL_TIMER, ptpClock->itimer);

    break;

  case PTP_SLAVE:
    timerStop(ANNOUNCE_RECEIPT_TIMER, ptpClock->itimer);

    if (rtOpts->E2E_mode)
      timerStop(DELAYREQ_INTERVAL_TIMER, ptpClock->itimer);
    else
      timerStop(PDELAYREQ_INTERVAL_TIMER, ptpClock->itimer);

    //    initClock(rtOpts, ptpClock);
    wr_servo_init(ptpClock);

    break;

   case PTP_PASSIVE:
    timerStop(PDELAYREQ_INTERVAL_TIMER, ptpClock->itimer);
    timerStop(ANNOUNCE_RECEIPT_TIMER, ptpClock->itimer);
    break;

   case PTP_LISTENING:
    timerStop(ANNOUNCE_RECEIPT_TIMER, ptpClock->itimer);
    break;

  default:
    break;
  }

  /**
      entering state tasks
  **/

  switch(state)
  {
  case PTP_INITIALIZING:
    DBG("state PTP_INITIALIZING\n");
    ptpClock->portState = PTP_INITIALIZING;
    break;

  case PTP_FAULTY:
    DBG("state PTP_FAULTY\n");
    ptpClock->portState = PTP_FAULTY;
    break;

  case PTP_DISABLED:
    DBG("state PTP_DISABLED\n");
    ptpClock->portState = PTP_DISABLED;
    break;

  case PTP_LISTENING:
    DBG("state PTP_LISTENING\n");
    timerStart(ANNOUNCE_RECEIPT_TIMER, (ptpClock->announceReceiptTimeout)
	       * (pow_2(ptpClock->logAnnounceInterval)), ptpClock->itimer);
    ptpClock->portState = PTP_LISTENING;
    break;

   case PTP_MASTER:
    DBG("state PTP_MASTER\n");
    timerStart(SYNC_INTERVAL_TIMER,
	       pow_2(ptpClock->logSyncInterval), ptpClock->itimer);
    DBG("SYNC INTERVAL TIMER : %f \n",pow_int(2, ptpClock->logSyncInterval));
    timerStart(ANNOUNCE_INTERVAL_TIMER,
	       pow_2(ptpClock->logAnnounceInterval), ptpClock->itimer);
    timerStart(PDELAYREQ_INTERVAL_TIMER,
	       pow_2(ptpClock->logMinPdelayReqInterval), ptpClock->itimer);
    ptpClock->portState = PTP_MASTER;
    break;


  case PTP_PASSIVE:
    DBG("state PTP_PASSIVE\n");
    timerStart(PDELAYREQ_INTERVAL_TIMER,
	       pow_2(ptpClock->logMinPdelayReqInterval), ptpClock->itimer);
    timerStart(ANNOUNCE_RECEIPT_TIMER, (ptpClock->announceReceiptTimeout) *
	       (pow_2(ptpClock->logAnnounceInterval)), ptpClock->itimer);
    ptpClock->portState = PTP_PASSIVE;
    break;

  case PTP_UNCALIBRATED:


    /*********** White Rabbit SLAVE*************
     *
     * here we have case of slave which
     * detectes that calibration is needed
     */
    if( ptpClock->wrNodeMode            == WR_SLAVE  && \
        ptpClock->grandmasterWrNodeMode == WR_MASTER && \
        (ptpClock->grandmasterIsWRmode  == FALSE     || \
         ptpClock->isWRmode             == FALSE     ))
    {
      DBG("state PTP_UNCALIBRATED\n");
#ifdef NEW_SINGLE_WRFSM
      toWRState(WRS_PRESENT, rtOpts, ptpClock);
#else
      toWRSlaveState(PTPWR_PRESENT, rtOpts, ptpClock);
#endif
      ptpClock->portState = PTP_UNCALIBRATED;
      break;
    }

    /*********** White Rabbit MASTER *************
     *
     * her we have case of master which
     * was forced to enter UNCALIBRATED state
     *
     */
    if(ptpClock->wrNodeMode == WR_MASTER)
    {
      DBG("state PTP_UNCALIBRATED\n");
#ifdef NEW_SINGLE_WRFSM
      toWRState(WRS_M_LOCK, rtOpts, ptpClock);
#else
      toWRMasterState(PTPWR_LOCK, rtOpts, ptpClock);
#endif
      ptpClock->portState = PTP_UNCALIBRATED;
      break;
    }

    /* Standard PTP, go straight to SLAVE */
    DBG("state PTP_SLAVE\n");
    ptpClock->portState = PTP_SLAVE;
    break;


  case PTP_SLAVE:
    DBG("state PTP_SLAVE\n");
    wr_servo_init(ptpClock);

    ptpClock->waitingForFollow = FALSE;

    ptpClock->pdelay_req_send_time.seconds = 0;
    ptpClock->pdelay_req_send_time.nanoseconds = 0;
    ptpClock->pdelay_req_receive_time.seconds = 0;
    ptpClock->pdelay_req_receive_time.nanoseconds = 0;
    ptpClock->pdelay_resp_send_time.seconds = 0;
    ptpClock->pdelay_resp_send_time.nanoseconds = 0;
    ptpClock->pdelay_resp_receive_time.seconds = 0;
    ptpClock->pdelay_resp_receive_time.nanoseconds = 0;

    timerStart(ANNOUNCE_RECEIPT_TIMER,(ptpClock->announceReceiptTimeout) *
	       (pow_2(ptpClock->logAnnounceInterval)), ptpClock->itimer);

    if (rtOpts->E2E_mode)
      timerStart(DELAYREQ_INTERVAL_TIMER,
		 pow_2(ptpClock->logMinDelayReqInterval), ptpClock->itimer);
    else
      timerStart(PDELAYREQ_INTERVAL_TIMER,
		 pow_2(ptpClock->logMinPdelayReqInterval), ptpClock->itimer);

    ptpClock->portState = PTP_SLAVE;
    break;

  default:
    DBG("to unrecognized state\n");
    break;
  }

  if(rtOpts->displayStats)
    displayStats(rtOpts, ptpClock);
}


void send_test(PtpClock *clock)
{
  for(;;)
    {
      char buf[64];
      wr_timestamp_t ts;

      netSendEvent(buf, 48, &clock->netPath, &ts);
      netSendGeneral(buf, 64, &clock->netPath);
      ptpd_wrap_sleep(1);
      netSendGeneral(buf, 64, &clock->netPath);
      ptpd_wrap_sleep(1);
    }
}


/*
 here WR adds initWRCalibration()
 */
Boolean doInit(RunTimeOpts *rtOpts, PtpClock *ptpClock)
{

  DBG("full initialization\n");
  DBG("manufacturerIdentity: %s\n", MANUFACTURER_ID);


  /* initialize networking */

  netShutdown(&ptpClock->netPath);

  if(!netInit(&ptpClock->netPath, rtOpts, ptpClock))
  {
    ERROR("failed to initialize network\n");
    toState(PTP_FAULTY, rtOpts, ptpClock);
    return FALSE;
  }

  //   send_test(ptpClock);

  /* initialize other stuff */
  initData(rtOpts, ptpClock);

  initTimer(ptpClock);

  initClock(rtOpts, ptpClock);
  m1(ptpClock);
  msgPackHeader(ptpClock->msgObuf, ptpClock);


  if(ptpClock->wrNodeMode != NON_WR)
    initWRcalibration(ptpClock->netPath.ifaceName, ptpClock);

  toState(PTP_LISTENING, rtOpts, ptpClock);

  return TRUE;
}

/*
 handle actions and events for 'port_state'
 here WR adds:
 -
 */
void doState(RunTimeOpts *rtOpts, PtpClock *ptpClock)
{

  //DBG("ptpClock->isWRmode = %d\n",ptpClock->isWRmode);
  //DBG("ptpClock->isCalibrated = %d\n",ptpClock->isCalibrated);


  UInteger8 state;
  Boolean linkUP;
  linkUP = isPortUp(&ptpClock->netPath);

  if(  linkUP == FALSE )
  {
    ptpClock->isWRmode = FALSE;
    ptpClock->isCalibrated = FALSE;
    ptpClock->record_update = TRUE;
  }

  ptpClock->message_activity = FALSE;

  switch(ptpClock->portState)
  {
  case PTP_LISTENING:
  case PTP_PASSIVE:
  case PTP_SLAVE:

  case PTP_MASTER:
  /*State decision Event*/
    if(ptpClock->record_update)
    {
      DBGV("event STATE_DECISION_EVENT\n");
      ptpClock->record_update = FALSE;

      state = bmc(ptpClock->foreign, rtOpts, ptpClock);

      /*
       * WR: transition through UNCALIBRATED state implemented
       */
      if(state != ptpClock->portState)
      {
	if(state               == PTP_SLAVE        && (
	   ptpClock->portState == PTP_LISTENING    ||
	   ptpClock->portState == PTP_PRE_MASTER   ||
	   ptpClock->portState == PTP_MASTER       ||
	   ptpClock->portState == PTP_PASSIVE      ||
	   ptpClock->portState == PTP_UNCALIBRATED )
	  )
	{
	  /* implementation of PTP_UNCALIBRATED state
	   * as transcient state between sth and SLAVE
	   * as specified in PTP state machine: Figure 23, 78p
	   */
	  toState(PTP_UNCALIBRATED, rtOpts, ptpClock);
	}
	else
	{
	  /* */
	  toState(state, rtOpts, ptpClock);
	}

      }
    }
    break;

  default:
    break;
  }

  switch(ptpClock->portState)
  {
  case PTP_FAULTY:
    /* imaginary troubleshooting */

    DBG("event FAULT_CLEARED\n");

    toState(PTP_INITIALIZING, rtOpts, ptpClock);
    return;

  case PTP_UNCALIBRATED:

    /*
     * WR:
     * Running WR FSMS
     */
#ifdef NEW_SINGLE_WRFSM
    if(ptpClock->wrNodeMode == WR_SLAVE || ptpClock->wrNodeMode == WR_MASTER)
    {
      doWRState(rtOpts, ptpClock);
    }
    else
    {
      toState(PTP_SLAVE, rtOpts, ptpClock);
    }
#else
    if(ptpClock->wrNodeMode == WR_SLAVE)
    {
      doWRSlaveState(rtOpts, ptpClock);
    }
    else if(ptpClock->wrNodeMode == WR_MASTER)
    {
      doWRMasterState(rtOpts, ptpClock);
    }
    else
    {
      toState(PTP_SLAVE, rtOpts, ptpClock);
    }
#endif
    // just in case, null management ID
    ptpClock->msgTmpManagementId =  NULL_MANAGEMENT;

    break;
  case PTP_LISTENING:
  case PTP_PASSIVE:

  case PTP_SLAVE:

    handle(rtOpts, ptpClock);


    if(timerExpired(ANNOUNCE_RECEIPT_TIMER, ptpClock->itimer,ptpClock->portIdentity.portNumber) || linkUP == FALSE)
   {
      DBGV("event ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES\n");
      ptpClock->number_foreign_records = 0;
      ptpClock->foreign_record_i = 0;

      if(!ptpClock->slaveOnly && ptpClock->clockQuality.clockClass != 255  )
      {
        m1(ptpClock);
        toState(PTP_MASTER, rtOpts, ptpClock);
      }
      else if(ptpClock->portState != PTP_LISTENING)
        toState(PTP_LISTENING, rtOpts, ptpClock);
    }



    if (rtOpts->E2E_mode)
    {
	if(timerExpired(DELAYREQ_INTERVAL_TIMER,ptpClock->itimer,ptpClock->portIdentity.portNumber))
	    {
	      DBG("event DELAYREQ_INTERVAL_TIMEOUT_EXPIRES\n");
	      issueDelayReq(rtOpts,ptpClock);
	    }
    }
    else
    {
	if(timerExpired(PDELAYREQ_INTERVAL_TIMER,ptpClock->itimer,ptpClock->portIdentity.portNumber))
	    {
	      DBG("\n\n\n\n\nevent PDELAYREQ_INTERVAL_TIMEOUT_EXPIRES\n\n\n\n\n\n");
	      issuePDelayReq(rtOpts,ptpClock);
	    }
    }

    /*
     * WR: read hardware TX timestamp if any pending
     */
//    if(ptpClock->pending_tx_ts == TRUE)
//      ptpClock->pending_tx_ts = getWRtxTimestamp(rtOpts,ptpClock);

    break;

  case PTP_MASTER:

    handle(rtOpts, ptpClock);

    if(timerExpired(SYNC_INTERVAL_TIMER, ptpClock->itimer,ptpClock->portIdentity.portNumber))
    {

      DBGV("event SYNC_INTERVAL_TIMEOUT_EXPIRES\n");
      issueSync(rtOpts, ptpClock);
      issueFollowup(rtOpts,ptpClock);
    }

    if(timerExpired(ANNOUNCE_INTERVAL_TIMER, ptpClock->itimer,ptpClock->portIdentity.portNumber))
    {

      DBGV("event ANNOUNCE_INTERVAL_TIMEOUT_EXPIRES\n");
      issueAnnounce(rtOpts, ptpClock);
    }

    if (!rtOpts->E2E_mode)
    {
	if(timerExpired(PDELAYREQ_INTERVAL_TIMER,ptpClock->itimer,ptpClock->portIdentity.portNumber))
	{
		DBG("event PDELAYREQ_INTERVAL_TIMEOUT_EXPIRES\n");
		issuePDelayReq(rtOpts,ptpClock);
		}
    }

    /*
     * WR: read hardware TX timestamp if any pending
     */
    if(ptpClock->pending_tx_ts == TRUE)
      ptpClock->pending_tx_ts = getWRtxTimestamp(rtOpts,ptpClock);

    if(ptpClock->slaveOnly || ptpClock->clockQuality.clockClass == 255)
    {
      toState(PTP_LISTENING, rtOpts, ptpClock);
    }

    break;

  case PTP_DISABLED:
    handle(rtOpts, ptpClock);
    break;

  default:
    DBG("(doState) do unrecognized state\n");
    break;
  }


}


/*
check and handle received messages
*/
void handle(RunTimeOpts *rtOpts, PtpClock *ptpClock)
{

  ssize_t length;
  Boolean isFromSelf;
  TimeInternal time = { 0, 0 };

#if 0
  /*
   * WR: TO BE implemented, would be nice
   */
   if(!ptpClock->message_activity)
   {
     ret = netSelect(0, &ptpClock->netPath);

     if(ret < 0)
     {
       PERROR("failed to poll sockets, ret = %d\n", ret);
       toState(PTP_FAULTY, rtOpts, ptpClock);
       return;
     }
     else if(!ret)
     {
       DBGV("handle: nothing, ret= %d\n",ret);
       return;
     }
     /* else length > 0 */
   }
#endif

  DBGV("handle: something\n");

  /*
   * WR:
   * in White Rabbit event and general message are received in the same
   * way, no difference, any of the functions (netRecvEvent and netRecvGeneral
   * can receive any event, need to clean things later
   */


  length = netRecvMsg(ptpClock->msgIbuf, &ptpClock->netPath, &(ptpClock->current_rx_ts));
  if(length < 0)
  {
    PERROR("failed to receive on the event socket, len = %d",(int)length);
    toState(PTP_FAULTY, rtOpts, ptpClock);
    return;
  }
  else if(!length)
  {
    //DBG("WR: no message received\n");
    return;
  }

  ptpClock->message_activity = TRUE;

  if(length < HEADER_LENGTH)
  {
    ERROR("message shorter than header length\n");
    toState(PTP_FAULTY, rtOpts, ptpClock);
    return;
  }

  // !!!!!!!!!!!!!!!!!
  /*
   * WR: to comply with old PTP daemon work flow
   * this should go to netRecvMsg
   */
  time.seconds = ptpClock->current_rx_ts.utc;
  time.nanoseconds = ptpClock->current_rx_ts.nsec;
  // !!!!!!!!!!!!!!!!!

  msgUnpackHeader(ptpClock->msgIbuf, &ptpClock->msgTmpHeader);

  if(ptpClock->msgTmpHeader.versionPTP != ptpClock->versionNumber)
  {
    DBG("ignore version %d message\n", ptpClock->msgTmpHeader.versionPTP);
    return;
  }

  if(ptpClock->msgTmpHeader.domainNumber != ptpClock->domainNumber)
  {
    DBG("ignore message from domainNumber %d\n", ptpClock->msgTmpHeader.domainNumber);
    return;
  }

  /*Spec 9.5.2.2*/
  isFromSelf = (ptpClock->portIdentity.portNumber == ptpClock->msgTmpHeader.sourcePortIdentity.portNumber
    && !memcmp(ptpClock->msgTmpHeader.sourcePortIdentity.clockIdentity, ptpClock->portIdentity.clockIdentity, CLOCK_IDENTITY_LENGTH));

  /* subtract the inbound latency adjustment if it is not a loop back and the
     time stamp seems reasonable */
  if(!isFromSelf && time.seconds > 0)
    subTime(&time, &time, &rtOpts->inboundLatency);


  switch(ptpClock->msgTmpHeader.messageType)
  {

  case ANNOUNCE:
    handleAnnounce(&ptpClock->msgTmpHeader, ptpClock->msgIbuf, length, isFromSelf, rtOpts, ptpClock);
    break;

  case SYNC:
    handleSync(&ptpClock->msgTmpHeader, ptpClock->msgIbuf, length, &time, isFromSelf, rtOpts, ptpClock);
    break;

  case FOLLOW_UP:
    handleFollowUp(&ptpClock->msgTmpHeader, ptpClock->msgIbuf, length, isFromSelf, rtOpts, ptpClock);
    break;

  case DELAY_REQ:
	handleDelayReq(&ptpClock->msgTmpHeader, ptpClock->msgIbuf, length, isFromSelf, rtOpts, ptpClock);
    break;

  case PDELAY_REQ:
    handlePDelayReq(&ptpClock->msgTmpHeader, ptpClock->msgIbuf, length, &time, isFromSelf, rtOpts, ptpClock);
    break;

  case DELAY_RESP:
	handleDelayResp(&ptpClock->msgTmpHeader, ptpClock->msgIbuf, length, isFromSelf, rtOpts, ptpClock);
    break;

  case PDELAY_RESP:
    handlePDelayResp(&ptpClock->msgTmpHeader, ptpClock->msgIbuf,&time, length, isFromSelf, rtOpts, ptpClock);
    break;

  case PDELAY_RESP_FOLLOW_UP:
    handlePDelayRespFollowUp(&ptpClock->msgTmpHeader, ptpClock->msgIbuf, length, isFromSelf, rtOpts, ptpClock);
    break;

  case MANAGEMENT:
    handleManagement(&ptpClock->msgTmpHeader, ptpClock->msgIbuf, length, isFromSelf, rtOpts, ptpClock);
    break;

  case SIGNALING:
    handleSignaling(&ptpClock->msgTmpHeader, ptpClock->msgIbuf, length, isFromSelf, rtOpts, ptpClock);
    break;

   default:
    DBG("handle: unrecognized message\n");
    break;
  }
  /*
   * WR debigging
   */
  if(length > 0)
  {
    DBG("\tRX HW_timestamp %s [ret=%d]\n", format_wr_timestamp(ptpClock->current_rx_ts), length);
/*    DBG("\tRX SW_timestamp : \n\t\t sec.msb = %ld \n\t\t sec.lsb = %lld \n\t\t nanosec = %lld\n", \
	(unsigned      long)ptpClock->current_rx_ts.seconds.hi,\
	(unsigned long long)ptpClock->current_rx_ts.seconds.lo,\
	(unsigned long long)ptpClock->current_rx_ts.nanoseconds); */
  }
}

/*spec 9.5.3*/
void handleAnnounce(MsgHeader *header, Octet *msgIbuf, ssize_t length, Boolean isFromSelf, RunTimeOpts *rtOpts, PtpClock *ptpClock)
{
	Boolean isFromCurrentParent = FALSE;



	//DBGV("HandleAnnounce : Announce message received : \n");

	if(length < ANNOUNCE_LENGTH)
	{
	ERROR("short Announce message\n");
	toState(PTP_FAULTY, rtOpts, ptpClock);
	return;
	 }

	/*
	 * WR debigging
	 */
	if(length > ANNOUNCE_LENGTH)
	  DBG("handle Announce msg, message from another White Rabbit node\n");
	else
	  DBG("handle Announce msg, standard PTP\n");

	switch(ptpClock->portState )
	{
		case PTP_INITIALIZING:
		case PTP_FAULTY:
		case PTP_DISABLED:

			DBGV("Handleannounce : disreguard \n");
			return;

		case PTP_UNCALIBRATED:

		  /*White Rabbit */
		  if(ptpClock->wrNodeMode != NON_WR)
		  {
		    DBGV("Handleannounce WR mode: disregaurd messages other than management \n");
		    return;
		  }

		case PTP_SLAVE:

		if (isFromSelf)
		{
			DBGV("HandleAnnounce : Ignore message from self \n");
			return;
		}

		ptpClock->record_update = TRUE; // Valid announce message is received : BMC algorithm will be executed



		isFromCurrentParent = !memcmp(ptpClock->parentPortIdentity.clockIdentity,header->sourcePortIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH)
							  && (ptpClock->parentPortIdentity.portNumber == header->sourcePortIdentity.portNumber);

	   switch (isFromCurrentParent)
	   {
			case TRUE:

			msgUnpackAnnounce(ptpClock->msgIbuf,&ptpClock->msgTmp.announce,header);

			if(ptpClock->msgTmp.announce.wr_flags != NON_WR)
			  DBG("handle Announce msg, message from another White Rabbit node [wr_flag != NON_WR]\n");

			s1(header,&ptpClock->msgTmp.announce,ptpClock);

			/*Reset Timer handling Announce receipt timeout*/
			timerStart(ANNOUNCE_RECEIPT_TIMER,
				   (ptpClock->announceReceiptTimeout) *
				   (pow_2(ptpClock->logAnnounceInterval)),
				   ptpClock->itimer);
			break;

			case FALSE:

			/*addForeign takes care of AnnounceUnpacking*/
			addForeign(ptpClock->msgIbuf,header,ptpClock);

			/*Reset Timer handling Announce receipt timeout*/
			timerStart(ANNOUNCE_RECEIPT_TIMER,
				   (ptpClock->announceReceiptTimeout) *
				   (pow_2(ptpClock->logAnnounceInterval)),
				   ptpClock->itimer);
			break;

			default:
			DBGV("HandleAnnounce : (isFromCurrentParent) strange value ! \n");
			return;

	   } //switch on (isFromCurrentParrent)
	   break;

	   case PTP_MASTER:
	   default :

	   if (isFromSelf)
	   {
			DBG("\tHandleAnnounce : Ignore message from self \n");
			return;
		}

		DBGV("Announce message from another foreign master");
		addForeign(ptpClock->msgIbuf,header,ptpClock);
		ptpClock->record_update = TRUE;
		break;

	}//switch on (port_state)

}

void handleSync(MsgHeader *header, Octet *msgIbuf, ssize_t length, TimeInternal *time, Boolean isFromSelf, RunTimeOpts *rtOpts, PtpClock *ptpClock)
{
	TimeInternal OriginTimestamp;
	TimeInternal correctionField;

	Boolean isFromCurrentParent = FALSE;

	if(length < SYNC_LENGTH)
	{
	ERROR("short Sync message\n");
	toState(PTP_FAULTY, rtOpts, ptpClock);
	return;
	 }

	switch(ptpClock->portState )
	{
		case PTP_INITIALIZING:
		case PTP_FAULTY:
		case PTP_DISABLED:

			DBGV("HandleSync : disreguard \n");
			return;

		case PTP_UNCALIBRATED:

		  /*White Rabbit */
		  if(ptpClock->wrNodeMode != NON_WR)
		  {
		    DBGV("Handleannounce WR mode: disregaurd messages other than management \n");
		    return;
		  }

		case PTP_SLAVE:

			if (isFromSelf)
			{
				DBGV("HandleSync: Ignore message from self \n");
				return;
			}

			isFromCurrentParent = !memcmp(ptpClock->parentPortIdentity.clockIdentity,header->sourcePortIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH)
							  && (ptpClock->parentPortIdentity.portNumber == header->sourcePortIdentity.portNumber);

			if (isFromCurrentParent)
			{
				/*
				 * WR: HW timestamps
				 */
				ptpClock->sync_receive_time.seconds = ptpClock->current_rx_ts.utc;
				ptpClock->sync_receive_time.nanoseconds = ptpClock->current_rx_ts.nsec;
				ptpClock->sync_receive_time.phase = ptpClock->current_rx_ts.phase;

				if ((header->flagField[0] & 0x02) == TWO_STEP_FLAG)
				{
					DBG("handle Sync msg, two step clock\n");
					ptpClock->waitingForFollow = TRUE;
					ptpClock->recvSyncSequenceId = header->sequenceId;

					/*Save correctionField of Sync message to local variable*/
					integer64_to_internalTime(header->correctionfield,&correctionField);

					/* remeber correction field */
					ptpClock->lastSyncCorrectionField.seconds = correctionField.seconds;
					ptpClock->lastSyncCorrectionField.nanoseconds = correctionField.nanoseconds;

					break;
				}
				else
				{
					DBG("\n\nBAD !!!!!!!! we don't use this; handle Sync msg, one step clock\n\n\n");
					msgUnpackSync(ptpClock->msgIbuf,&ptpClock->msgTmp.sync);
					integer64_to_internalTime(ptpClock->msgTmpHeader.correctionfield,&correctionField);
					timeInternal_display(&correctionField);
					ptpClock->waitingForFollow = FALSE;
					toInternalTime(&OriginTimestamp,&ptpClock->msgTmp.sync.originTimestamp);
					updateOffset(&OriginTimestamp,&ptpClock->sync_receive_time,&ptpClock->ofm_filt,rtOpts,ptpClock,&correctionField);
					updateClock(rtOpts,ptpClock);

					break;
				}

			}
			break;


		case PTP_MASTER:
		default :

/* This is not how we do it in WR */
#if 0
			if (!isFromSelf)
			{
				DBG("HandleSync: Sync message received from another Master  \n");
				break;
			}

			else
			{
				/*Add latency*/
				addTime(time,time,&rtOpts->outboundLatency);
				DBG("HandleSync: Sync message received from self\n");
				issueFollowup(time,rtOpts,ptpClock);
				break;
			}
#endif
		  break;
	}
}


void handleFollowUp(MsgHeader *header, Octet *msgIbuf, ssize_t length, Boolean isFromSelf, RunTimeOpts *rtOpts, PtpClock *ptpClock)
{

	DBGV("Handlefollowup : Follow up message received \n");

	TimeInternal preciseOriginTimestamp;
	TimeInternal correctionField;
	Boolean isFromCurrentParent = FALSE;

	 if(length < FOLLOW_UP_LENGTH)
	 {
	ERROR("short Follow up message\n");
	toState(PTP_FAULTY, rtOpts, ptpClock);
	return;
	 }

	 if (isFromSelf)
	 {
		DBGV("Handlefollowup : Ignore message from self \n");
		return;
	 }

	 switch(ptpClock->portState )
	{
		case PTP_INITIALIZING:
		case PTP_FAULTY:
		case PTP_DISABLED:
		case PTP_LISTENING:

			DBGV("Handfollowup : disreguard \n");
			return;

		case PTP_UNCALIBRATED:

		  /*White Rabbit */
		  if(ptpClock->wrNodeMode != NON_WR)
		  {
		    DBGV("Handleannounce WR mode: disregaurd messages other than management \n");
		    return;
		  }

		case PTP_SLAVE:

//		isFromCurrentParent = !memcmp(ptpClock->parentPortIdentity.clockIdentity,header->sourcePortIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH)
//							  && (ptpClock->parentPortIdentity.portNumber == header->sourcePortIdentity.portNumber);

		DBG("comparing parent and source portID: \n");

		printf("clockIdentity: %d and %d\n", \
		      ptpClock->parentPortIdentity.portNumber, \
		      header->sourcePortIdentity.portNumber);


		if ((memcmp(ptpClock->parentPortIdentity.clockIdentity,header->sourcePortIdentity.clockIdentity, CLOCK_IDENTITY_LENGTH) == 0 )
			&& (ptpClock->parentPortIdentity.portNumber == header->sourcePortIdentity.portNumber))
		    isFromCurrentParent = TRUE;

		if (isFromCurrentParent)
		{
			if (ptpClock->waitingForFollow)
			{
				if ((ptpClock->recvSyncSequenceId == header->sequenceId))
				{

						msgUnpackFollowUp(ptpClock->msgIbuf,&ptpClock->msgTmp.follow);

						DBG("handle FollowUP msg, succedded: \n\t\t sec.msb = %ld \n\t\t sec.lsb = %lld \n\t\t nanosec = %lld\n",\
						(unsigned      long)ptpClock->msgTmp.follow.preciseOriginTimestamp.secondsField.msb,\
						(unsigned long long)ptpClock->msgTmp.follow.preciseOriginTimestamp.secondsField.lsb,\
						(unsigned long long)ptpClock->msgTmp.follow.preciseOriginTimestamp.nanosecondsField);

						ptpClock->waitingForFollow = FALSE;

						/*copy the precise followUP info to local variable*/
						toInternalTime(&preciseOriginTimestamp,&ptpClock->msgTmp.follow.preciseOriginTimestamp);

						preciseOriginTimestamp.phase = 0;

						/*get correction field form the header to local variable*/
						integer64_to_internalTime(ptpClock->msgTmpHeader.correctionfield,&correctionField);

						/*add to correctionField last sych correction (?)*/
						addTime(&correctionField,&correctionField,&ptpClock->lastSyncCorrectionField);

						DBG("\n\n ------------------- calculate after receiving FollowUP msg ---------------\n");

						wr_servo_got_sync(ptpClock, preciseOriginTimestamp, ptpClock->sync_receive_time);


						DBG("\n -------------------------------finish calculation ------------------------\n\n");

						issueDelayReq(rtOpts,ptpClock);

						break;
				}
				else DBG("handle FollowUp msg, SequenceID doesn't match with last Sync message \n");
				//else DBGV("SequenceID doesn't match with last Sync message \n");

			}
			else DBG("handle FollowUp msg, Slave was not waiting a follow up message \n");
			//else DBGV("Slave was not waiting a follow up message \n");
		}
		else DBG("handle FollowUp msg, Follow up message is not from current parent \n");
		//else DBGV("Follow up message is not from current parent \n");

		case PTP_MASTER:
			DBGV("Follow up message received from another master \n");
			break;

		default:
		DBG("do unrecognized state\n");
		break;
	}//Switch on (port_state)

}


static Integer32 phase_to_cf_units(Integer32 phase)
{
  return (Integer32) ((double)phase / 1000.0 * 65536.0);
}

void handleDelayReq(MsgHeader *header,Octet *msgIbuf,ssize_t length,Boolean isFromSelf,RunTimeOpts *rtOpts,PtpClock *ptpClock)
{




if (rtOpts->E2E_mode)
{
	DBGV("delayReq message received : \n");

		 if(length < DELAY_REQ_LENGTH)
		 {
		ERROR("short DelayReq message\n");
		toState(PTP_FAULTY, rtOpts, ptpClock);
		return;
		 }

		switch(ptpClock->portState )
		{
			case PTP_INITIALIZING:
			case PTP_FAULTY:
			case PTP_DISABLED:
			case PTP_UNCALIBRATED:
			case PTP_LISTENING:
				DBGV("HandledelayReq : disreguard \n");
				return;

			case PTP_SLAVE:

				if (isFromSelf)
				{
/* WR: not this way in White Rabbit*/
 #if 0
					/* Get sending timestamp from IP stack with So_TIMESTAMP*/
					ptpClock->delay_req_send_time.seconds = time->seconds;
					ptpClock->delay_req_send_time.nanoseconds = time->nanoseconds;

					/*Add latency*/
					addTime(&ptpClock->delay_req_send_time,&ptpClock->delay_req_send_time,&rtOpts->outboundLatency);
#endif
					break;
				}

			break;

			case PTP_MASTER:

				DBG("handle DelayReq msg, succedded\n");
				msgUnpackHeader(ptpClock->msgIbuf,&ptpClock->delayReqHeader);


				//FIXME: do this, but properly
				ptpClock->delayReqHeader.correctionfield.msb = 0;
				ptpClock->delayReqHeader.correctionfield.lsb = phase_to_cf_units(ptpClock->current_rx_ts.phase);

				issueDelayResp(&ptpClock->delayReqHeader,rtOpts,ptpClock);
				break;

			default:
			DBG("do unrecognized state\n");
			break;
		}
}


else //(Peer to Peer mode)
{
	ERROR("Delay messages are disreguard in Peer to Peer mode \n");
}

}



void handleDelayResp(MsgHeader *header,Octet *msgIbuf,ssize_t length,Boolean isFromSelf,RunTimeOpts *rtOpts,PtpClock *ptpClock)
{

if (rtOpts->E2E_mode)
{

	Boolean isFromCurrentParent = FALSE;
	TimeInternal requestReceiptTimestamp;
	TimeInternal correctionField;

	DBGV("delayResp message received : \n");

		if(length < DELAY_RESP_LENGTH)
		{
		ERROR("short DelayResp message\n");
		toState(PTP_FAULTY, rtOpts, ptpClock);
		return;
		}

		switch(ptpClock->portState )
		{
			case PTP_INITIALIZING:
			case PTP_FAULTY:
			case PTP_DISABLED:
			case PTP_UNCALIBRATED:
			case PTP_LISTENING:


				DBGV("HandledelayResp : disreguard \n");
				return;

			case PTP_SLAVE:


				msgUnpackDelayResp(ptpClock->msgIbuf,&ptpClock->msgTmp.resp);

				DBG("handle DelayResp msg, succedded: \n\t\t sec.msb = %ld \n\t\t sec.lsb = %lld \n\t\t nanosec = %lld\n",\
				(unsigned      long)ptpClock->msgTmp.resp.receiveTimestamp.secondsField.msb,\
				(unsigned long long)ptpClock->msgTmp.resp.receiveTimestamp.secondsField.lsb,\
				(unsigned long long)ptpClock->msgTmp.resp.receiveTimestamp.nanosecondsField);


//				isFromCurrentParent = !memcmp(ptpClock->parentPortIdentity.clockIdentity,header->sourcePortIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH)
//								  && (ptpClock->parentPortIdentity.portNumber == header->sourcePortIdentity.portNumber);

				if ((memcmp(ptpClock->parentPortIdentity.clockIdentity,header->sourcePortIdentity.clockIdentity, CLOCK_IDENTITY_LENGTH) == 0 )
					&& (ptpClock->parentPortIdentity.portNumber == header->sourcePortIdentity.portNumber))
				    isFromCurrentParent = TRUE;


//			  if (!   ((ptpClock->sentDelayReqSequenceId == header->sequenceId)
//				   && (!memcmp(ptpClock->portIdentity.clockIdentity,ptpClock->msgTmp.resp.requestingPortIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH))
//				   && ( ptpClock->portIdentity.portNumber == ptpClock->msgTmp.resp.requestingPortIdentity.portNumber))
//				   && isFromCurrentParent)

				if ((memcmp(ptpClock->portIdentity.clockIdentity, ptpClock->msgTmp.resp.requestingPortIdentity.clockIdentity, CLOCK_IDENTITY_LENGTH) == 0)
				   && ((ptpClock->sentDelayReqSequenceId - 1) == header->sequenceId)
				   && (ptpClock->portIdentity.portNumber == ptpClock->msgTmp.resp.requestingPortIdentity.portNumber)
				   && isFromCurrentParent)
				{
					/* copy timestamp to local variable */
					//toInternalTime(&requestReceiptTimestamp,&ptpClock->msgTmp.presp.requestReceiptTimestamp);
					toInternalTime(&requestReceiptTimestamp, &ptpClock->msgTmp.resp.receiveTimestamp);


					/* t_4 */
					ptpClock->delay_req_receive_time.seconds = requestReceiptTimestamp.seconds;
					ptpClock->delay_req_receive_time.nanoseconds = requestReceiptTimestamp.nanoseconds;
					ptpClock->delay_req_receive_time.phase = requestReceiptTimestamp.phase;

					/* coppy correctionField from header->cF to local variable (correctionField) */
					integer64_to_internalTime(header->correctionfield,&correctionField);

					printf("\n\n ------------------- calculate after receiving DelayResp msg ---------------\n");

					wr_servo_got_delay(ptpClock, header->correctionfield.lsb);
					wr_servo_update(ptpClock);


					printf("\n\n --------------------------- finish calculateion------- ---------------------\n");

					ptpClock->logMinDelayReqInterval = header->logMessageInterval;

				}


				else
				{
					DBGV("HandledelayResp : delayResp doesn't match with the delayReq. \n");
					break;
				}

		}


}

else //(Peer to Peer mode)
{
	ERROR("Delay messages are disreguard in Peer to Peer mode \n");
}

}



void handlePDelayReq(MsgHeader *header, Octet *msgIbuf, ssize_t length, TimeInternal *time, Boolean isFromSelf, RunTimeOpts *rtOpts, PtpClock *ptpClock)
{

  if (!rtOpts->E2E_mode)
   {

	DBGV("PdelayReq message received : \n");

	 if(length < PDELAY_REQ_LENGTH)
	 {
	ERROR("short PDelayReq message\n");
	toState(PTP_FAULTY, rtOpts, ptpClock);
	return;
	 }

	switch(ptpClock->portState )
	{
		case PTP_INITIALIZING:
		case PTP_FAULTY:
		case PTP_DISABLED:
		case PTP_UNCALIBRATED:
		case PTP_LISTENING:
			DBGV("HandlePdelayReq : disreguard \n");
			return;

		case PTP_SLAVE:
		case PTP_MASTER:
		case PTP_PASSIVE:

		if (isFromSelf)
		{
			/* Get sending timestamp from IP stack with So_TIMESTAMP*/
			ptpClock->pdelay_req_send_time.seconds = time->seconds;
			ptpClock->pdelay_req_send_time.nanoseconds = time->nanoseconds;

			/*Add latency*/
			addTime(&ptpClock->pdelay_req_send_time,&ptpClock->pdelay_req_send_time,&rtOpts->outboundLatency);
			break;
		}
		else
		{
			DBG("handle PDelayReq msg, succedded\n");
			msgUnpackHeader(ptpClock->msgIbuf,&ptpClock->PdelayReqHeader);
			issuePDelayResp(time,header,rtOpts,ptpClock);
			break;
		}

		default:
		DBG("do unrecognized state\n");
		break;
	}
   }

  else //(End to End mode..)
  {
	  ERROR("Peer Delay messages are disreguard in End to End mode \n");
  }
}

void handlePDelayResp(MsgHeader *header, Octet *msgIbuf, TimeInternal *time,ssize_t length, Boolean isFromSelf, RunTimeOpts *rtOpts, PtpClock *ptpClock)
{

if (!rtOpts->E2E_mode)
 {
	Boolean isFromCurrentParent = FALSE;
	TimeInternal requestReceiptTimestamp;
	TimeInternal correctionField;

	DBGV("PdelayResp message received : \n");

	 if(length < PDELAY_RESP_LENGTH)
	 {
	ERROR("short PDelayResp message\n");
	toState(PTP_FAULTY, rtOpts, ptpClock);
	return;
	 }

	switch(ptpClock->portState )
	{
		case PTP_INITIALIZING:
		case PTP_FAULTY:
		case PTP_DISABLED:
		case PTP_UNCALIBRATED:
		case PTP_LISTENING:

			DBGV("HandlePdelayResp : disreguard \n");
			return;

		case PTP_SLAVE:

		if (isFromSelf)
		{
		 addTime(time,time,&rtOpts->outboundLatency);
		 issuePDelayRespFollowUp(time,&ptpClock->PdelayReqHeader,rtOpts,ptpClock);
		 break;
		}

			msgUnpackPDelayResp(ptpClock->msgIbuf,&ptpClock->msgTmp.presp);

			DBG("handle PDelayResp msg, succedded [SLAVE]: \n\t\t sec.msb = %ld \n\t\t sec.lsb = %lld \n\t\t nanosec = %lld\n",\
			(unsigned      long)ptpClock->msgTmp.presp.requestReceiptTimestamp.secondsField.msb,\
			(unsigned long long)ptpClock->msgTmp.presp.requestReceiptTimestamp.secondsField.lsb,\
			(unsigned long long)ptpClock->msgTmp.presp.requestReceiptTimestamp.nanosecondsField);

			isFromCurrentParent = !memcmp(ptpClock->parentPortIdentity.clockIdentity,header->sourcePortIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH)
							  && (ptpClock->parentPortIdentity.portNumber == header->sourcePortIdentity.portNumber);

			if (!   ((ptpClock->sentPDelayReqSequenceId == header->sequenceId)
			   && (!memcmp(ptpClock->portIdentity.clockIdentity,ptpClock->msgTmp.presp.requestingPortIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH))
			   && ( ptpClock->portIdentity.portNumber == ptpClock->msgTmp.presp.requestingPortIdentity.portNumber)))

			{
				if ((header->flagField[0] & 0x02) == TWO_STEP_FLAG)
				{
					/*Store t4 (Fig 35)*/
					ptpClock->pdelay_resp_receive_time.seconds = time->seconds;
					ptpClock->pdelay_resp_receive_time.nanoseconds = time->nanoseconds;
//
					DBG("\n\n\ntime[two steps]: ptpClock->pdelay_resp_receive_time.seconds     = %ld\n",time->seconds);
					DBG("\n\n\ntime[two steps]: ptpClock->pdelay_resp_receive_time.nanoseconds = %ld\n\n\n",time->nanoseconds);

					/*store t2 (Fig 35)*/
					toInternalTime(&requestReceiptTimestamp,&ptpClock->msgTmp.presp.requestReceiptTimestamp);
					ptpClock->pdelay_req_receive_time.seconds = requestReceiptTimestamp.seconds;
					ptpClock->pdelay_req_receive_time.nanoseconds = requestReceiptTimestamp.nanoseconds;

					integer64_to_internalTime(header->correctionfield,&correctionField);
					ptpClock->lastPdelayRespCorrectionField.seconds = correctionField.seconds;
					ptpClock->lastPdelayRespCorrectionField.nanoseconds = correctionField.nanoseconds;
//
					break;
				}//Two Step Clock

				else //One step Clock
				{
					/*Store t4 (Fig 35)*/
					ptpClock->pdelay_resp_receive_time.seconds = time->seconds;
					ptpClock->pdelay_resp_receive_time.nanoseconds = time->nanoseconds;

					DBG("\n\n\ntime[one step]: ptpClock->pdelay_resp_receive_time.seconds     = %ld\n",time->seconds);
					DBG("\n\n\ntime[one step]: ptpClock->pdelay_resp_receive_time.nanoseconds = %ld\n\n\n",time->nanoseconds);

					integer64_to_internalTime(header->correctionfield,&correctionField);

					printf("\n\n ----------- calculate after receiving handlePDelayResp [one step] msg ---------\n");

					//					updatePeerDelay (&ptpClock->owd_filt,rtOpts,ptpClock,&correctionField,FALSE);

					printf("\n\n --------------------------- finish calculateion------- ---------------------\n");

					break;
				}

			}
			else
			{
				DBGV("HandlePdelayResp : Pdelayresp doesn't match with the PdelayReq. \n");
				break;
			}

		case PTP_MASTER:
			/*Loopback Timestamp*/
				if (isFromSelf)
				{
					/*Add latency*/
					addTime(time,time,&rtOpts->outboundLatency);

					issuePDelayRespFollowUp(time,&ptpClock->PdelayReqHeader,rtOpts,ptpClock);
					break;
				}


				msgUnpackPDelayResp(ptpClock->msgIbuf,&ptpClock->msgTmp.presp);

				DBG("handle PDelayResp msg, succedded [MASTER: \n\t\t sec.msb = %ld \n\t\t sec.lsb = %lld \n\t\t nanosec = %lld\n",\
				(unsigned      long)ptpClock->msgTmp.presp.requestReceiptTimestamp.secondsField.msb,\
				(unsigned long long)ptpClock->msgTmp.presp.requestReceiptTimestamp.secondsField.lsb,\
				(unsigned long long)ptpClock->msgTmp.presp.requestReceiptTimestamp.nanosecondsField);


				isFromCurrentParent = !memcmp(ptpClock->parentPortIdentity.clockIdentity,header->sourcePortIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH)
							  && (ptpClock->parentPortIdentity.portNumber == header->sourcePortIdentity.portNumber);

				if (!   ((ptpClock->sentPDelayReqSequenceId == header->sequenceId)
					&& (!memcmp(ptpClock->portIdentity.clockIdentity,ptpClock->msgTmp.presp.requestingPortIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH))
					&& ( ptpClock->portIdentity.portNumber == ptpClock->msgTmp.presp.requestingPortIdentity.portNumber)))

				{
					if ((header->flagField[0] & 0x02) == TWO_STEP_FLAG)
					{
						/*Store t4 (Fig 35)*/
						ptpClock->pdelay_resp_receive_time.seconds = time->seconds;
						ptpClock->pdelay_resp_receive_time.nanoseconds = time->nanoseconds;


						/*store t2 (Fig 35)*/
						toInternalTime(&requestReceiptTimestamp,&ptpClock->msgTmp.presp.requestReceiptTimestamp);
						ptpClock->pdelay_req_receive_time.seconds = requestReceiptTimestamp.seconds;
						ptpClock->pdelay_req_receive_time.nanoseconds = requestReceiptTimestamp.nanoseconds;

						integer64_to_internalTime(header->correctionfield,&correctionField);
						ptpClock->lastPdelayRespCorrectionField.seconds = correctionField.seconds;
						ptpClock->lastPdelayRespCorrectionField.nanoseconds = correctionField.nanoseconds;
						break;
					}//Two Step Clock

					else //One step Clock
					{
						/*Store t4 (Fig 35)*/
						ptpClock->pdelay_resp_receive_time.seconds = time->seconds;
						ptpClock->pdelay_resp_receive_time.nanoseconds = time->nanoseconds;

						integer64_to_internalTime(header->correctionfield,&correctionField);

						printf("\n\n ----------- calculate after receiving handlePDelayResp [one step] msg ---------\n");

						//						updatePeerDelay (&ptpClock->owd_filt,rtOpts,ptpClock,&correctionField,FALSE);

						printf("\n\n --------------------------- finish calculateion------- ---------------------\n");

						break;
					}

				}
		default:
		DBG("do unrecognized state\n");
		break;
		}
 }

else //(End to End mode..)
 {
	ERROR("Peer Delay messages are disreguard in End to End mode \n");
 }

}

void handlePDelayRespFollowUp(MsgHeader *header, Octet *msgIbuf, ssize_t length, Boolean isFromSelf, RunTimeOpts *rtOpts, PtpClock *ptpClock){

if (!rtOpts->E2E_mode)
 {
	TimeInternal responseOriginTimestamp;
	TimeInternal correctionField;

	DBGV("PdelayRespfollowup message received : \n");

	 if(length < PDELAY_RESP_FOLLOW_UP_LENGTH)
	 {
	ERROR("short PDelayRespfollowup message\n");
	toState(PTP_FAULTY, rtOpts, ptpClock);
	return;
	 }

	switch(ptpClock->portState )
	{
		case PTP_INITIALIZING:
		case PTP_FAULTY:
		case PTP_DISABLED:
		case PTP_UNCALIBRATED:
			DBGV("HandlePdelayResp : disreguard \n");
			return;

		case PTP_SLAVE:

		if (header->sequenceId == ptpClock->sentPDelayReqSequenceId-1)
		{
			msgUnpackPDelayRespFollowUp(ptpClock->msgIbuf,&ptpClock->msgTmp.prespfollow);

			DBG("handle handlePDelayRespFollowUp msg [MASTER], succedded: \n\t\t sec.msb = %ld \n\t\t sec.lsb = %lld \n\t\t nanosec = %lld\n",\
			(unsigned      long)ptpClock->msgTmp.prespfollow.responseOriginTimestamp.secondsField.msb,\
			(unsigned long long)ptpClock->msgTmp.prespfollow.responseOriginTimestamp.secondsField.lsb,\
			(unsigned long long)ptpClock->msgTmp.prespfollow.responseOriginTimestamp.nanosecondsField);


			toInternalTime(&responseOriginTimestamp,&ptpClock->msgTmp.prespfollow.responseOriginTimestamp);
			ptpClock->pdelay_resp_send_time.seconds = responseOriginTimestamp.seconds;
			ptpClock->pdelay_resp_send_time.nanoseconds = responseOriginTimestamp.nanoseconds;
		    integer64_to_internalTime(ptpClock->msgTmpHeader.correctionfield,&correctionField);
			addTime(&correctionField,&correctionField,&ptpClock->lastPdelayRespCorrectionField);

			DBG("\n\n ------------ calculate after receiving handlePDelayRespFollowUp msg --------\n");

			//			updatePeerDelay (&ptpClock->owd_filt,rtOpts,ptpClock,&correctionField,TRUE);

			DBG("\n -------------------------------finish calculation ------------------------\n\n");

			break;
		}

		case PTP_MASTER:

		if (header->sequenceId == ptpClock->sentPDelayReqSequenceId-1)
		{
			msgUnpackPDelayRespFollowUp(ptpClock->msgIbuf,&ptpClock->msgTmp.prespfollow);

			DBG("handle handlePDelayRespFollowUp msg [MASTER], succedded: \n\t\t sec.msb = %ld \n\t\t sec.lsb = %lld \n\t\t nanosec = %lld\n",\
			(unsigned      long)ptpClock->msgTmp.prespfollow.responseOriginTimestamp.secondsField.msb,\
			(unsigned long long)ptpClock->msgTmp.prespfollow.responseOriginTimestamp.secondsField.lsb,\
			(unsigned long long)ptpClock->msgTmp.prespfollow.responseOriginTimestamp.nanosecondsField);


			toInternalTime(&responseOriginTimestamp,&ptpClock->msgTmp.prespfollow.responseOriginTimestamp);
			ptpClock->pdelay_resp_send_time.seconds = responseOriginTimestamp.seconds;
			ptpClock->pdelay_resp_send_time.nanoseconds = responseOriginTimestamp.nanoseconds;
		    integer64_to_internalTime(ptpClock->msgTmpHeader.correctionfield,&correctionField);
			addTime(&correctionField,&correctionField,&ptpClock->lastPdelayRespCorrectionField);

			DBG("\n\n ------------ calculate after receiving handlePDelayRespFollowUp msg --------\n");

			updatePeerDelay (&ptpClock->owd_filt,rtOpts,ptpClock,&correctionField,TRUE);

			DBG("\n -------------------------------finish calculation ------------------------\n\n");
			break;
		}

		default:
			DBGV("Disregard PdelayRespFollowUp message  \n");
	}

}

else //(End to End mode..)
 {
	ERROR("Peer Delay messages are disreguard in End to End mode \n");
 }

}
/*
WR: custom White Rabbit management
*/
void handleManagement(MsgHeader *header, Octet *msgIbuf, ssize_t length, Boolean isFromSelf, RunTimeOpts *rtOpts, PtpClock *ptpClock)
{
  MsgManagement management;
  DBG("%s\n",__func__);

  if(!isFromSelf)
  {

    msgUnpackWRManagement(ptpClock->msgIbuf,&management,&(ptpClock->msgTmpManagementId),ptpClock);


#ifdef NEW_SINGLE_WRFSM
/*
 * NEW single FSM and messages
 */
    switch(ptpClock->msgTmpManagementId)
    {

      case CALIBRATE:

        DBG("\n\n\
	handle WR Management msg [CALIBRATE], succedded, \
	\n\t\tcalibrateSendPattern  = %32x\
	\n\t\tcalibrationPeriod     = %32lld us \
	\n\t\tcalibrationPattern    = %s \
	\n\t\tcalibrationPatternLen = %32d bits\n\n",\
        ptpClock->otherNodeCalibrationSendPattern, \
	(unsigned long long)ptpClock->otherNodeCalibrationPeriod, \
        printf_bits(ptpClock->otherNodeCalibrationPattern), \
        (unsigned)ptpClock->otherNodeCalibrationPatternLen);
        break;

      case CALIBRATED:

        DBG("\n\nhandle WR Management msg [CALIBRATED], succedded, params: \
	\n\t\tdeltaTx = %16lld \
	\n\t\tdeltaRx= %16lld\n\n", \
        ((unsigned long long)ptpClock->grandmasterDeltaTx.scaledPicoseconds.msb)<<32 | (unsigned long long)ptpClock->grandmasterDeltaTx.scaledPicoseconds.lsb, \
        ((unsigned long long)ptpClock->grandmasterDeltaRx.scaledPicoseconds.msb)<<32 | (unsigned long long)ptpClock->grandmasterDeltaRx.scaledPicoseconds.lsb);
        break;
      case SLAVE_PRESENT:
	DBG("\n\nhandle WR Management msg [SLAVE_PRESENT], succedded \n\n\n");
	break;

      case LOCK:
	DBG("\n\nhandle WR Management msg [LOCK], succedded \n\n");
	break;

      case LOCKED:

	DBG("\n\nhandle WR Management msg [LOCKED], succedded \n\n");
	break;

      case WR_MODE_ON:

	DBG("\n\nhandle WR Management msg [WR_LINK_ON], succedded \n\n");
	break;
      default:
	DBG("\n\nhandle WR Management msg [UNKNOWN], failed \n\n");
      break;
   }

#else
/*
 * OLD FSMs and messages
 */
    switch(ptpClock->msgTmpManagementId)
    {

      case MASTER_CALIBRATE:
      case SLAVE_CALIBRATE:

        DBG("\n\nhandle WR Management msg [CALIBRATE], succedded, \
	\n\t\tcalibrationPeriod     = %16lld us \
	\n\t\tcalibrationPattern    = %s \
	\n\t\tcalibrationPatternLen = %d bits\n\n",\
        (unsigned long long)ptpClock->otherNodeCalibrationPeriod, \
        printf_bits(ptpClock->otherNodeCalibrationPattern), \
        (unsigned)ptpClock->otherNodeCalibrationPatternLen);
        break;

      case MASTER_CALIBRATED:
      case SLAVE_CALIBRATED:

	DBG("\n\nhandle WR Management msg [CALIBRATED], succedded, params: \
	\n\t\tdeltaTx = %16lld \
	\n\t\tdeltaRx = %16lld\n\n", \
        ((unsigned long long)ptpClock->grandmasterDeltaTx.scaledPicoseconds.msb)<<32 | (unsigned long long)ptpClock->grandmasterDeltaTx.scaledPicoseconds.lsb, \
        ((unsigned long long)ptpClock->grandmasterDeltaRx.scaledPicoseconds.msb)<<32 | (unsigned long long)ptpClock->grandmasterDeltaRx.scaledPicoseconds.lsb);
        break;

      case SLAVE_PRESENT:
	DBG("\n\nhandle WR Management msg [SLAVE_PRESENT], succedded \n\n");
	break;

      case LOCK:

	DBG("\n\nhandle WR Management msg [LOCK], succedded \n\n");
	break;

      case LOCKED:

	DBG("\n\nhandle WR Management msg [LOCKED], succedded \n\n");
	break;

      case WR_MODE_ON:

	DBG("\n\nhandle WR Management msg [WR_MODE_ON], succedded \n\n");
	break;
      default:
	DBG("\n\nhandle WR Management msg [UNKNOWN], failed \n\n");
      break;
   }
#endif

  /*
   * here the master recognizes that it talks with WR slave
   * which identifies itself and the calibration is statrted
   * if the calibration is already being done, just ignore this
   */

    if(ptpClock->wrNodeMode        == WR_MASTER &&     \
      ptpClock->msgTmpManagementId == SLAVE_PRESENT && \
      ptpClock->portState          != PTP_UNCALIBRATED )
      {
        toState(PTP_UNCALIBRATED,rtOpts,ptpClock);
      }
  }
}


void handleSignaling(MsgHeader *header, Octet *msgIbuf, ssize_t length, Boolean isFromSelf, RunTimeOpts *rtOpts, PtpClock *ptpClock){}


/*Pack and send on general multicast ip adress an Announce message*/
void issueAnnounce(RunTimeOpts *rtOpts,PtpClock *ptpClock)
{


	msgPackAnnounce(ptpClock->msgObuf,ptpClock);

	UInteger16 announce_len;

	if (ptpClock->wrNodeMode != NON_WR)
	   announce_len = WR_ANNOUNCE_LENGTH;
	else
	   announce_len = ANNOUNCE_LENGTH;

	if (!netSendGeneral(ptpClock->msgObuf,announce_len,&ptpClock->netPath))
	{
		toState(PTP_FAULTY,rtOpts,ptpClock);
		DBGV("Announce message can't be sent -> FAULTY state \n");
		DBG("issue: Announce Msg, failed \n");
	}
	else
	{
		DBG("issue: Announce Msg, succedded \n");
		DBGV("Announce MSG sent ! \n");
		ptpClock->sentAnnounceSequenceId++;
	}
}



/*Pack and send on event multicast ip adress a Sync message*/
void issueSync(RunTimeOpts *rtOpts,PtpClock *ptpClock)
{




	Timestamp originTimestamp;
	TimeInternal internalTime;
	getTime(&internalTime);
	fromInternalTime(&internalTime,&originTimestamp);

	msgPackSync(ptpClock->msgObuf,&originTimestamp,ptpClock);


	if (!netSendEvent(ptpClock->msgObuf,SYNC_LENGTH,&ptpClock->netPath,&ptpClock->synch_tx_ts))
	{
		toState(PTP_FAULTY,rtOpts,ptpClock);
		ptpClock->pending_Synch_tx_ts = FALSE;
		DBGV("Sync message can't be sent -> FAULTY state \n");
		DBG("issue: Sync Msg, failed");
	}
	else
	{
		DBG("issue: Sync Msg, succedded  \n \t\t synch timestamp: %s\n", \
		format_wr_timestamp(ptpClock->synch_tx_ts));
		ptpClock->pending_tx_ts = TRUE;
		ptpClock->pending_Synch_tx_ts = TRUE;
		ptpClock->sentSyncSequenceId++;
	}



}





/*Pack and send on general multicast ip adress a FollowUp message*/
//void issueFollowup(TimeInternal *time,RunTimeOpts *rtOpts,PtpClock *ptpClock)
void issueFollowup(RunTimeOpts *rtOpts,PtpClock *ptpClock)
{


	msgPackFollowUp(ptpClock->msgObuf,ptpClock);

	if (!netSendGeneral(ptpClock->msgObuf,FOLLOW_UP_LENGTH,&ptpClock->netPath))
	{
		toState(PTP_FAULTY,rtOpts,ptpClock);
		DBGV("FollowUp message can't be sent -> FAULTY state \n");
		DBG("issue: FollowUp Msg, failed\n");
	}
	else
	{
		DBG("issue: FollowUp Msg, succedded [sending time of sync tx]: \n\t\t sec = %lld \n\t\t nanosec = %lld\n",\
		(unsigned long long)ptpClock->synch_tx_ts.utc,\
		(unsigned long long)ptpClock->synch_tx_ts.nsec);

	}
}


/*Pack and send on event multicast ip adress a DelayReq message*/
void issueDelayReq(RunTimeOpts *rtOpts,PtpClock *ptpClock)
{

	Timestamp originTimestamp;
	TimeInternal internalTime;
	getTime(&internalTime);
	fromInternalTime(&internalTime,&originTimestamp);

	msgPackDelayReq(ptpClock->msgObuf,&originTimestamp,ptpClock);

	if (!netSendEvent(ptpClock->msgObuf,DELAY_REQ_LENGTH,&ptpClock->netPath,&ptpClock->delayReq_tx_ts))
	{
		toState(PTP_FAULTY,rtOpts,ptpClock);
		//ptpClock->new_tx_tag_read = FALSE;
		ptpClock->pending_DelayReq_tx_ts = FALSE;
		DBGV("delayReq message can't be sent -> FAULTY state \n");
		DBG("issue: DelayReq Msg, failed\n");
	}
	else
	{
		DBG("issue: DelayReq Msg, succedded \n \t\t timestamp: %s\n",format_wr_timestamp(ptpClock->delayReq_tx_ts));
		ptpClock->sentDelayReqSequenceId++;
		ptpClock->pending_tx_ts = TRUE;
		ptpClock->pending_DelayReq_tx_ts = TRUE;
	}
}

/*Pack and send on event multicast ip adress a PDelayReq message*/
void issuePDelayReq(RunTimeOpts *rtOpts,PtpClock *ptpClock)
{

	Timestamp originTimestamp;
	TimeInternal internalTime;
	getTime(&internalTime);
	fromInternalTime(&internalTime,&originTimestamp);

	msgPackPDelayReq(ptpClock->msgObuf,&originTimestamp,ptpClock);

	if (!netSendPeerEvent(ptpClock->msgObuf,PDELAY_REQ_LENGTH,&ptpClock->netPath,&ptpClock->pDelayReq_tx_ts))
	{
		toState(PTP_FAULTY,rtOpts,ptpClock);
		ptpClock->pending_PDelayReq_tx_ts = FALSE;
		DBGV("PdelayReq message can't be sent -> FAULTY state \n");
		DBG("issue: PDelayReq Msg, failed\n");
	}
	else
	{
		DBG("issue: PDelayReq Msg, succedded \n");
		DBGV("PDelayReq MSG sent ! \n");
		ptpClock->pending_tx_ts = TRUE;
		ptpClock->pending_PDelayReq_tx_ts = TRUE;
		ptpClock->sentPDelayReqSequenceId++;
	}
}

/*Pack and send on event multicast ip adress a PDelayResp message*/
void issuePDelayResp(TimeInternal *time,MsgHeader *header,RunTimeOpts *rtOpts,PtpClock *ptpClock)
{


	Timestamp requestReceiptTimestamp;
	fromInternalTime(time,&requestReceiptTimestamp);
	msgPackPDelayResp(ptpClock->msgObuf,header,&requestReceiptTimestamp,ptpClock);

	if (!netSendPeerEvent(ptpClock->msgObuf,PDELAY_RESP_LENGTH,&ptpClock->netPath,&ptpClock->pDelayResp_tx_ts))
	{
		toState(PTP_FAULTY,rtOpts,ptpClock);
		ptpClock->pending_PDelayResp_tx_ts = FALSE;
		DBGV("PdelayResp message can't be sent -> FAULTY state \n");
		DBG("issue: PDelayResp Msg, failed\n");
	}
	else
	{
		ptpClock->pending_tx_ts = TRUE;
		ptpClock->pending_PDelayResp_tx_ts = TRUE;
		DBG("issue: PDelayResp Msg, succedded [sending PDelayReq receive time]: \n\t\t sec = %lld \n\t\t nanosec = %lld\n",\
		(unsigned long long)ptpClock->pdelay_req_receive_time.seconds,\
		(unsigned long long)ptpClock->pdelay_req_receive_time.nanoseconds);
	}
}


/*Pack and send on event multicast ip adress a DelayResp message*/
void issueDelayResp(MsgHeader *header,RunTimeOpts *rtOpts,PtpClock *ptpClock)
{


	msgPackDelayResp(ptpClock->msgObuf,header,ptpClock);

	if (!netSendGeneral(ptpClock->msgObuf,PDELAY_RESP_LENGTH,&ptpClock->netPath))
	{
		toState(PTP_FAULTY,rtOpts,ptpClock);
		DBGV("delayResp message can't be sent -> FAULTY state \n");
		DBG("issue: DelayResp Msg, failed\n");
	}
	else
	{
		DBG("issue: DelayResp Msg, succedded [sending DelayReq receive time]: \n\t\t sec = %lld \n\t\t nanosec = %lld\n",\
		(unsigned long long)ptpClock->delay_req_receive_time.seconds,\
		(unsigned long long)ptpClock->delay_req_receive_time.nanoseconds);
	}
}



void issuePDelayRespFollowUp(TimeInternal *time,MsgHeader *header,RunTimeOpts *rtOpts,PtpClock *ptpClock)
{


	Timestamp responseOriginTimestamp;
	fromInternalTime(time,&responseOriginTimestamp);

	msgPackPDelayRespFollowUp(ptpClock->msgObuf,header,&responseOriginTimestamp,ptpClock);

	if (!netSendPeerGeneral(ptpClock->msgObuf,PDELAY_RESP_FOLLOW_UP_LENGTH,&ptpClock->netPath))
	{
		toState(PTP_FAULTY,rtOpts,ptpClock);
		DBGV("PdelayRespFollowUp message can't be sent -> FAULTY state \n");
		DBG("issue: PDelayFollowUp Msg, failed\n");
	}
	else
	{
		DBG("issue: PDelayRespFollowUp Msg, succedded [sending time of pDelayResp tx]: \n\t\t sec = %ld \n\t\t  nanosec = %lld\n",\
		(unsigned long long)ptpClock->pDelayResp_tx_ts.utc,\
		(unsigned long long)ptpClock->pDelayResp_tx_ts.nsec);
	}
}

void issueManagement(MsgHeader *Header, MsgManagement *manage,RunTimeOpts *rtOpts,PtpClock *ptpClock)
{



	msgPackWRManagement(ptpClock->msgObuf,ptpClock, SLAVE_PRESENT);
	DBG("Issuing management NON-WR msg, managementId = 0x%x\n",SLAVE_PRESENT);
	if (!netSendGeneral(ptpClock->msgObuf,WR_MANAGEMENT_LENGTH,&ptpClock->netPath))
	{
		toState(PTP_FAULTY,rtOpts,ptpClock);
		DBGV("Management message can't be sent -> FAULTY state \n");
		DBG("issue: Management Msg, failed\n");
	}
	else
	{
		DBG("issue: Management Msg, succedded\n");
		DBGV("FOllowUp MSG sent ! \n");
	}

}

/*
WR: custom White Rabbit management
*/
void issueWRManagement(Enumeration16 wr_managementId,RunTimeOpts *rtOpts,PtpClock *ptpClock)
{


	UInteger16 len;
	len = msgPackWRManagement(ptpClock->msgObuf,ptpClock, wr_managementId);

	if (!netSendGeneral(ptpClock->msgObuf,len,&ptpClock->netPath))
	{
		toState(PTP_FAULTY,rtOpts,ptpClock);
		DBGV("Management message can't be sent -> FAULTY state \n");
		DBG("issue: WR Management Msg, failed \n");
	}
	else
	{
	      switch(wr_managementId)
	      {
#ifdef NEW_SINGLE_WRFSM
		case CALIBRATE:
#else
		case MASTER_CALIBRATE:
		case SLAVE_CALIBRATE:
#endif
		  DBG("\n\nissue WR Management msg [CALIBRATE], succedded, \
		  \n\t\tcalibrationSendPattern = %32x \
		  \n\t\tcalibrationPeriod      = %32lld us \
		  \n\t\tcalibrationPattern     = %s \
		  \n\t\tcalibrationPatternLen  = %32d bits\n\n",\
		  !ptpClock->isCalibrated, \
		  (unsigned long long)ptpClock->calibrationPeriod, \
		  printf_bits(ptpClock->calibrationPattern), \
		  (unsigned)ptpClock->calibrationPatternLen);

		  break;
#ifdef NEW_SINGLE_WRFSM
		case CALIBRATED:
#else
		case MASTER_CALIBRATED:
		case SLAVE_CALIBRATED:
#endif
		  DBG("\n\nissue WR Management msg [CALIBRATED], succedded, params: \n  \t\tdeltaTx= %16lld \n \t\tdeltaRx= %16lld\n\n", \
		  ((unsigned long long)ptpClock->deltaTx.scaledPicoseconds.msb)<<32 | (unsigned long long)ptpClock->deltaTx.scaledPicoseconds.lsb, \
		  ((unsigned long long)ptpClock->deltaRx.scaledPicoseconds.msb)<<32 | (unsigned long long)ptpClock->deltaRx.scaledPicoseconds.lsb);
		  break;
		case SLAVE_PRESENT:
		  DBG("\n\nissue WR Management msg [SLAVE_PRESENT], succedded \n\n");
		  break;
		case LOCK:
		  DBG("\n\nissue WR Management msg [LOCK], succedded \n\n");
		  break;
		case LOCKED:
		  DBG("\n\nissue WR Management msg [LOCKED], succedded \n\n");
		  break;
		case WR_MODE_ON:
		  DBG("\n\nissue WR Management msg [WR_MODE_ON], succedded \n\n");
		  break;
		default:
		  DBG("\n\nissue WR Management msg [UNKNOWN], failed \n\n");
		break;
	    }
	}

}

void addForeign(Octet *buf,MsgHeader *header,PtpClock *ptpClock)
{
	int i,j;
	Boolean found = FALSE;

	j = ptpClock->foreign_record_best;

	/*Check if Foreign master is already known*/
	for (i=0;i<ptpClock->number_foreign_records;i++)
	{
		if (!memcmp(header->sourcePortIdentity.clockIdentity,ptpClock->foreign[j].foreignMasterPortIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH)
			&& (header->sourcePortIdentity.portNumber == ptpClock->foreign[j].foreignMasterPortIdentity.portNumber))
			{
				/*Foreign Master is already in Foreignmaster data set*/
				ptpClock->foreign[j].foreignMasterAnnounceMessages++;
				found = TRUE;
				DBGV("addForeign : AnnounceMessage incremented \n");

				msgUnpackHeader(buf,&ptpClock->foreign[j].header);
				msgUnpackAnnounce(buf,&ptpClock->foreign[j].announce,&ptpClock->foreign[j].header);
				if(ptpClock->foreign[j].announce.wr_flags != NON_WR)
				  DBG("handle Announce msg, message from another White Rabbit node [wr_flag != NON_WR]\n");
				break;
			}

	j = (j+1)%ptpClock->number_foreign_records;
	}

	/*New Foreign Master*/
	if (!found)
	{
		if (ptpClock->number_foreign_records < ptpClock->max_foreign_records)
		{
			ptpClock->number_foreign_records++;
		}
		j = ptpClock->foreign_record_i;

		/*Copy new foreign master data set from Announce message*/
		ptpd_wrap_memcpy(ptpClock->foreign[j].foreignMasterPortIdentity.clockIdentity,header->sourcePortIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH);
		ptpClock->foreign[j].foreignMasterPortIdentity.portNumber = header->sourcePortIdentity.portNumber;
		ptpClock->foreign[j].foreignMasterAnnounceMessages = 0;

		/*header and announce field of each Foreign Master are usefull to run Best Master Clock Algorithm*/
		msgUnpackHeader(buf,&ptpClock->foreign[j].header);
		msgUnpackAnnounce(buf,&ptpClock->foreign[j].announce,&ptpClock->foreign[j].header);
		if(ptpClock->foreign[j].announce.wr_flags != NON_WR)
		  DBG("handle Announce msg, message from another White Rabbit node [wr_flag != NON_WR]\n");

		DBGV("New foreign Master added \n");

		ptpClock->foreign_record_i = (ptpClock->foreign_record_i+1) % ptpClock->max_foreign_records;

	}

}
