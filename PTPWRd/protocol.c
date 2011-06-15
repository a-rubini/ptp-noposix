/* protocol.c */

#include "ptpd.h"

#include "ptpd_netif.h"


Boolean doInit(RunTimeOpts*,PtpPortDS*);
void doState(RunTimeOpts*,PtpPortDS*);
void toState(UInteger8,RunTimeOpts*,PtpPortDS*);

void handle(RunTimeOpts*,PtpPortDS*);
void handleAnnounce(MsgHeader*,Octet*,ssize_t,Boolean,RunTimeOpts*,PtpPortDS*);
void handleSync(MsgHeader*,Octet*,ssize_t,TimeInternal*,Boolean,RunTimeOpts*,PtpPortDS*);
void handleDelayReq(MsgHeader*,Octet*,ssize_t,Boolean,RunTimeOpts*,PtpPortDS*);
void handleManagement(MsgHeader*,Octet*,ssize_t,Boolean,RunTimeOpts*,PtpPortDS*);
void issueAnnounce(RunTimeOpts*,PtpPortDS*);
void issueSync(RunTimeOpts*,PtpPortDS*);
void issueFollowup(RunTimeOpts*,PtpPortDS*);
void issueDelayReq(RunTimeOpts*,PtpPortDS*);
#ifdef WRPTPv2
void issueWRSignalingMsg(Enumeration16,RunTimeOpts*,PtpPortDS*);
#else
void issueWRManagement(Enumeration16 wr_managementId,RunTimeOpts*,PtpPortDS*);
#endif
void addForeign(Octet*,MsgHeader*,PtpPortDS*);

void handleSignaling(MsgHeader*,Octet*,ssize_t,Boolean,RunTimeOpts*,PtpPortDS*);
void issueManagement(MsgHeader*,MsgManagement*,RunTimeOpts*,PtpPortDS*);

Boolean msgIsFromCurrentParent(MsgHeader *, PtpPortDS *);


#ifndef WR_MODE_ONLY
void handlePDelayReq(MsgHeader*,Octet*,ssize_t,TimeInternal*,Boolean,RunTimeOpts*,PtpPortDS*);
void handlePDelayResp(MsgHeader*,Octet*,TimeInternal* ,ssize_t,Boolean,RunTimeOpts*,PtpPortDS*);
void handleDelayResp(MsgHeader*,Octet*,ssize_t,Boolean,RunTimeOpts*,PtpPortDS*);
void handlePDelayRespFollowUp(MsgHeader*,Octet*,ssize_t,Boolean,RunTimeOpts*,PtpPortDS*);
void issuePDelayReq(RunTimeOpts*,PtpPortDS*);
void issuePDelayResp(TimeInternal*,MsgHeader*,RunTimeOpts*,PtpPortDS*);
void issueDelayResp(MsgHeader*,RunTimeOpts*,PtpPortDS*);
void issuePDelayRespFollowUp(TimeInternal*,MsgHeader*,RunTimeOpts*,PtpPortDS*);
#endif



/* The code used pow(2, ...) but we don't want floating point here (ARub) */
static inline unsigned long pow_2(int exp)
{
	return 1 << exp;
}



#ifndef WRPC_EXTRA_SLIM

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
void multiProtocol(RunTimeOpts *rtOpts, PtpPortDS *ptpPortDS)
 {

  int           i;
  PtpPortDS *    currentPtpPortDSData;

  currentPtpPortDSData = ptpPortDS;

  for (i=0; i < rtOpts->portNumber; i++)
  {
     DBGV("multiPortProtocol: initializing port %d\n", (i+1));
     toState(PTP_INITIALIZING, rtOpts, currentPtpPortDSData);
     if(!doInit(rtOpts, currentPtpPortDSData))
     {
       // doInit Failed!  Exit
       DBG("\n--------------------------------------------------\n---------------- port %d failed to doInit()-----------------------------\n--------------------------------\n",(i+1));
       //return;
     }
     currentPtpPortDSData++;
  }

  for(;;)
  {
    currentPtpPortDSData = ptpPortDS;

    for (i=0; i < rtOpts->portNumber; i++)
    {

      do
      {
      /*
       * perform calibration for a give port
       * we can only calibrate one port at a time (HW limitation)
       * we want the calibration to be performed as quick as possible
       * and without disturbance, so don't perform doState for other ports
       * for the time of calibration
       */

	if(currentPtpPortDSData->portState != PTP_INITIALIZING)
	  doState(rtOpts, currentPtpPortDSData);
	else if(!doInit(rtOpts, currentPtpPortDSData))
	  return;

	if(currentPtpPortDSData->message_activity)
	  DBGV("activity\n");
	else
	  DBGV("no activity\n");

      }
      while(currentPtpPortDSData->portState == PTP_UNCALIBRATED);

      currentPtpPortDSData++;
    }
    

  }



 }

#endif

/* loop forever. doState() has a switch for the actions and events to be
   checked for 'port_state'. the actions and events may or may not change
   'port_state' by calling toState(), but once they are done we loop around
   again and perform the actions required for the new 'port_state'. */
void protocol(RunTimeOpts *rtOpts, PtpPortDS *ptpPortDS)
{
  DBG("event POWERUP\n");
  
  ptpPortDS->ptpClockDS->globalStateDecisionEvent = FALSE;
  
  toState(PTP_INITIALIZING, rtOpts, ptpPortDS);

  for(;;)
  {
    ptpd_handle_wripc();

    if(ptpPortDS->portState != PTP_INITIALIZING)
      doState(rtOpts, ptpPortDS);
    else if(!doInit(rtOpts, ptpPortDS))
      return;

    /* Handle Best Master Clock Algorithm globally */
    if(globalBestForeignMastersUpdate(ptpPortDS))
      ptpPortDS->ptpClockDS->globalStateDecisionEvent = TRUE;
    else
      ptpPortDS->ptpClockDS->globalStateDecisionEvent = FALSE;
    

  }
}


/*
 * perform actions required when leaving 'port_state' and entering 'state'
 */
void toState(UInteger8 state, RunTimeOpts *rtOpts, PtpPortDS *ptpPortDS)
{

#ifdef WRPTPv2	
  /*
   * kind-of non-pre-emption of WR FSM is
   * implemented by banning change of PTP state
   * if WR state is different then WRS_IDLE.
   */
  if(ptpPortDS->wrPortState !=WRS_IDLE)
      return ;
#endif  
  ptpPortDS->message_activity = TRUE;

  /**
    leaving state tasks
  **/
  switch(ptpPortDS->portState)
  {
  case PTP_MASTER:
    DBG("PTP_FSM .... exiting PTP_MASTER\n");
    
    timerStop(&ptpPortDS->timers.sync);
    timerStop(&ptpPortDS->timers.announceInterval);
    timerStop(&ptpPortDS->timers.pdelayReq);

    break;

  case PTP_SLAVE:
    DBG("PTP_FSM .... exiting PTP_SLAVE\n");
    timerStop(&ptpPortDS->timers.announceReceipt);

    if (rtOpts->E2E_mode)
      timerStop(&ptpPortDS->timers.delayReq);
    else
      timerStop(&ptpPortDS->timers.pdelayReq);

    wr_servo_init(ptpPortDS);
    break;

   case PTP_PASSIVE:
     DBG("PTP_FSM .... exiting PTP_PASSIVE\n");
     timerStop(&ptpPortDS->timers.pdelayReq);
     timerStop(&ptpPortDS->timers.announceReceipt);
     break;
      
  case PTP_LISTENING:
    DBG("PTP_FSM .... exiting PTP_LISTENING\n");
    timerStop(&ptpPortDS->timers.announceReceipt);
      
    break;
  case PTP_UNCALIBRATED:
    /*
     * add here transition to WR_IDLE state of WR FSM
     * this is to accommodate the fact that PTP FSM can, by itself
     * want to leave the state (timeout, or BMC) before WR FSM finishes
     *
     * toWRstate(IDLE, ...);
     *
     */
  default:
    break;
  }

  /**
      entering state tasks
  **/

  switch(state)
  {
  case PTP_INITIALIZING:
    DBG("PTP_FSM .... entering PTP_INITIALIZING\n");
    ptpPortDS->portState = PTP_INITIALIZING;
    break;

  case PTP_FAULTY:
    DBG("PTP_FSM .... entering PTP_FAULTY\n");
    ptpPortDS->portState = PTP_FAULTY;
    break;

  case PTP_DISABLED:
    DBG("PTP_FSM .... entering  PTP_DISABLED\n");
    ptpPortDS->portState = PTP_DISABLED;
    break;

  case PTP_LISTENING:
    DBG("PTP_FSM .... entering  PTP_LISTENING\n");

    timerStart(&ptpPortDS->timers.announceReceipt, 
	       ptpPortDS->announceReceiptTimeout * 1000 * (pow_2(ptpPortDS->logAnnounceInterval)));
    ptpPortDS->portState = PTP_LISTENING;
    break;

   case PTP_MASTER:
    DBG("PTP_FSM .... entering  PTP_MASTER\n");

    timerStart(&ptpPortDS->timers.sync, 
	       1000 * pow_2(ptpPortDS->logSyncInterval));
    timerStart(&ptpPortDS->timers.announceInterval, 
	       1000 * pow_2(ptpPortDS->logAnnounceInterval));
    timerStart(&ptpPortDS->timers.pdelayReq, 
	       1000 * pow_2(ptpPortDS->logMinPdelayReqInterval));

    ptpPortDS->portState = PTP_MASTER;
    break;


  case PTP_PASSIVE:
    DBG("PTP_FSM .... entering  PTP_PASSIVE\n");

    timerStart(&ptpPortDS->timers.pdelayReq, 
	      1000 * pow_2(ptpPortDS->logMinPdelayReqInterval));

    timerStart(&ptpPortDS->timers.announceInterval, 
	       ptpPortDS->announceReceiptTimeout * 1000 * pow_2(ptpPortDS->logAnnounceInterval));

    ptpPortDS->portState = PTP_PASSIVE;
    break;

  case PTP_UNCALIBRATED:
    DBG("PTP_FSM .... entering  PTP_UNCALIBRATED\n");

    /*********** White Rabbit SLAVE*************
     *
     * here we have case of slave which
     * detectes that calibration is needed
     */
    // WRPTPv2: we might not need it TODO: investigate
    
#ifdef WRPTPv2

   /********* evaluating candidate for WR Slave **********
    *
    * First we check whether the port is WR Slave enabled
    * and the parentPort is WR Master enabled
    *****************************************************/
    if( ptpPortDS->wrMode            == WR_SLAVE   &&
       (ptpPortDS->wrConfig          == WR_S_ONLY  || \
	ptpPortDS->wrConfig          == WR_M_AND_S)&& \
	(ptpPortDS->parentWrConfig   == WR_M_ONLY  || \
	ptpPortDS->parentWrConfig    == WR_M_AND_S))
    {
        /* now we check whether WR Link Setup is needed */
	if(ptpPortDS->parentWrModeON  == FALSE     || \
	   ptpPortDS->wrModeON        == FALSE     )
      {

	toWRState(WRS_PRESENT, rtOpts, ptpPortDS);
	ptpPortDS->portState = PTP_UNCALIBRATED;
	DBG("PTP_FSM .... entering PTP_UNCALIBRATED ( WR_SLAVE )\n");
	break;
      }
    }
    else
    {// one of the ports on the link is not WR-enabled
      ptpPortDS->wrMode = NON_WR;
    }
    
#else
    if( ptpPortDS->wrMode            == WR_SLAVE  && \
        ptpPortDS->parentWrNodeMode == WR_MASTER && \
        (ptpPortDS->parentWrModeON  == FALSE     || \
         ptpPortDS->wrModeON             == FALSE     ))
    {
          
      
# ifdef NEW_SINGLE_WRFSM
      toWRState(WRS_PRESENT, rtOpts, ptpPortDS);
# else
      toWRSlaveState(PTPWR_PRESENT, rtOpts, ptpPortDS);

      ptpPortDS->portState = PTP_UNCALIBRATED;
      break;
    }
# endif      
#endif 

#ifndef WRPTPv2
// moved to PTP Master state


    /*********** White Rabbit MASTER *************
     *
     * her we have case of master which
     * was forced to enter UNCALIBRATED state
     *
     */
    
    if(ptpPortDS->wrMode == WR_MASTER)
    {
      DBG("PTP_FSM .... entering PTP_UNCALIBRATED ( WR_MASTER )\n");
      
# ifdef NEW_SINGLE_WRFSM
      toWRState(WRS_M_LOCK, rtOpts, ptpPortDS);
# else
      toWRMasterState(PTPWR_LOCK, rtOpts, ptpPortDS);
# endif

      ptpPortDS->portState = PTP_UNCALIBRATED;
      break;
    }
#endif
    /* Standard PTP, go straight to SLAVE */
    DBG("PTP_FSM .... entering PTP_SLAVE ( failed to enter PTP_UNCALIBRATED )\n");
    ptpPortDS->portState  = PTP_SLAVE;
   
    break;


  case PTP_SLAVE:
    DBG("PTP_FSM .... entering PTP_SLAVE\n");
    wr_servo_init(ptpPortDS);

    ptpPortDS->waitingForFollow = FALSE;

    ptpPortDS->pdelay_req_send_time.seconds = 0;
    ptpPortDS->pdelay_req_send_time.nanoseconds = 0;
    ptpPortDS->pdelay_req_receive_time.seconds = 0;
    ptpPortDS->pdelay_req_receive_time.nanoseconds = 0;
    ptpPortDS->pdelay_resp_send_time.seconds = 0;
    ptpPortDS->pdelay_resp_send_time.nanoseconds = 0;
    ptpPortDS->pdelay_resp_receive_time.seconds = 0;
    ptpPortDS->pdelay_resp_receive_time.nanoseconds = 0;

    timerStart(&ptpPortDS->timers.announceReceipt,
	       (ptpPortDS->announceReceiptTimeout) * 1000 * pow_2(ptpPortDS->logAnnounceInterval));

    if (rtOpts->E2E_mode)
      timerStart(&ptpPortDS->timers.delayReq,
		 1000 * pow_2(ptpPortDS->logMinDelayReqInterval));
    else
      timerStart(&ptpPortDS->timers.pdelayReq,
		 1000 * pow_2(ptpPortDS->logMinPdelayReqInterval));

    ptpPortDS->portState = PTP_SLAVE;
    break;

  default:
    DBG("PTP_FSM .... entering  unrecognized state\n");
    break;
  }

  if(rtOpts->displayStats)
    displayStats(rtOpts, ptpPortDS);
}


void send_test(PtpPortDS *clock)
{
  for(;;)
    {
      char buf[64];
      wr_timestamp_t ts;

      netSendEvent(buf, 48, &clock->netPath, &ts);
      netSendGeneral(buf, 64, &clock->netPath);
      sleep(1);
      netSendGeneral(buf, 64, &clock->netPath);
      sleep(1);
    }
}



/*
 here WR adds initWRCalibration()
 */
Boolean doInit(RunTimeOpts *rtOpts, PtpPortDS *ptpPortDS)
{

  DBG("Initialization: manufacturerIdentity: %s\n", MANUFACTURER_ID);

  /* initialize networking */

  netShutdown(&ptpPortDS->netPath);

#ifndef WRPTPv2
  ptpPortDS->wrConfig = rtOpts->wrConfig;
#endif

  /* network init */
  if(!netInit(&ptpPortDS->netPath, rtOpts, ptpPortDS))
  {
    ERROR("failed to initialize network\n");
    toState(PTP_FAULTY, rtOpts, ptpPortDS);
    return FALSE;
  }

  //   send_test(ptpPortDS);

  /* all the protocol (PTP + WRPTP) initialization */
  initDataPort(rtOpts, ptpPortDS);

#ifdef WRPTPv2
  /* 
   * attempt autodetection only if non wr config is set, 
   * otherwise, the configured setting is forced
   */
  if(ptpPortDS->wrConfig == WR_MODE_AUTO)
    autoDetectPortWrConfig(&ptpPortDS->netPath, ptpPortDS); //TODO handle error
  else
    DBG("wrConfig .............. FORCED configuration\n")  ;
#endif

  /* Create the timers (standard PTP only, the WR ones are created in another function) */
  timerInit(&ptpPortDS->timers.sync, "Sync");
  timerInit(&ptpPortDS->timers.delayReq, "DelayReq");
  timerInit(&ptpPortDS->timers.pdelayReq, "PDelayReq");
  timerInit(&ptpPortDS->timers.announceReceipt, "AnnReceipt");
  timerInit(&ptpPortDS->timers.announceInterval, "AnnInterval");

  initClock(rtOpts, ptpPortDS);
  m1(ptpPortDS);
  msgPackHeader(ptpPortDS->msgObuf, ptpPortDS);

#ifdef WRPTPv2
  if(ptpPortDS->wrConfig != NON_WR)
#else
  if(ptpPortDS->wrMode != NON_WR)
#endif    
  {
    initWRcalibration(ptpPortDS->netPath.ifaceName, ptpPortDS);
    // TODO: set appropriately classes if slaveOnly or masterOnly
  }

  toState(PTP_LISTENING, rtOpts, ptpPortDS);

  return TRUE;
}

/*
 handle actions and events for 'port_state'
 here WR adds:
 -
 */
void doState(RunTimeOpts *rtOpts, PtpPortDS *ptpPortDS)
{
	UInteger8 state;
	Boolean linkUP;

	//DBG("DoState : %d\n", ptpPortDS->portState);

	linkUP = isPortUp(&ptpPortDS->netPath);

	if(  linkUP == FALSE )
	{
		ptpPortDS->wrModeON = FALSE;
		ptpPortDS->calibrated = FALSE;
		//ptpPortDS->record_update = TRUE;
		if(ptpPortDS->wrMode == WR_MASTER)
		   ptpPortDS->wrMode = NON_WR;
		

		/* if the link goes down, go to FAULTY state immediately */

		//toState(PTP_FAULTY, rtOpts, ptpPortDS);
		//return;
	}

	ptpPortDS->message_activity = FALSE;

	switch(ptpPortDS->portState)
	{
	case PTP_LISTENING:
	case PTP_PASSIVE:
	case PTP_SLAVE:
	case PTP_MASTER:
		/*State decision Event*/
#ifdef WRPTPv2
		/*kind-of-non-WRFSM-preemption implementation*/
		//if(ptpPortDS->record_update && ptpPortDS->wrPortState == WRS_IDLE)
		if(ptpPortDS->ptpClockDS->globalStateDecisionEvent == TRUE && ptpPortDS->wrPortState == WRS_IDLE)
#else
		if(ptpPortDS->record_update)  // make this global clockwise !!!!
#endif		  
		  
		{
			DBGV("event STATE_DECISION_EVENT\n");
			ptpPortDS->record_update = FALSE;

			state = bmc(ptpPortDS->foreign, rtOpts, ptpPortDS);

			/*
			* WR: transition through UNCALIBRATED state implemented
			*/
			if(state != ptpPortDS->portState)
			{
				if(state == PTP_SLAVE        && (
				   ptpPortDS->portState == PTP_LISTENING    || 
				   ptpPortDS->portState == PTP_PRE_MASTER   ||
				   ptpPortDS->portState == PTP_MASTER       ||
				   ptpPortDS->portState == PTP_PASSIVE      ||
				   ptpPortDS->portState == PTP_UNCALIBRATED ))
				{
					/* implementation of PTP_UNCALIBRATED state
					* as transcient state between sth and SLAVE
					* as specified in PTP state machine: Figure 23, 78p
					*/
					/*
					if((ptpPortDS->wrConfig          == WR_S_ONLY  || \
					    ptpPortDS->wrConfig          == WR_M_AND_S)&& \
					   (ptpPortDS->parentWrConfig    == WR_M_ONLY  || \
					    ptpPortDS->parentWrConfig    == WR_M_AND_S))
					{
					  DBG("wrMode <= WR_SLAVE\n");
					  ptpPortDS->wrMode  = WR_SLAVE;
					}
					*/
					/* Candidate for WR Slave */
					ptpPortDS->wrMode  = WR_SLAVE;
					DBG("recommended state = PTP_SLAVE, current state = PTP_MASTER\n");
					DBG("recommended wrMode = WR_SLAVE\n");
					toState(PTP_UNCALIBRATED, rtOpts, ptpPortDS);

				}
				else
				{
					/* */
					toState(state, rtOpts, ptpPortDS);
				}

			}else
			{
				 /***** SYNCHRONIZATION_FAULT detection ****
				  * here we have a mechanims to enforce WR LINK SETUP (so-colled WR 
				  * calibration).
				  * It's enough if we "turn off" WRmode in the WR Slave or WR Master
		 		  *
		 		  * WR Master: the info will be transferred with Annouce MSG 
				  *            (wr_flags), and the WR Slave will verify **here**  
				  *            that re-synch is needed,
				  *
		 		  * WR Slave: the need for re-synch, indicated by wrModeON==FALSE,  
		 		  *           will be evaluated here
		 		  */
		 
				  if(ptpPortDS->portState	    == PTP_SLAVE && \
				     ptpPortDS->wrMode               == WR_SLAVE  && \
		   		    (ptpPortDS->parentWrModeON       == FALSE     || \
		   		     ptpPortDS->wrModeON             == FALSE     ))
		 		  {
				      DBG("event SYNCHRONIZATION_FAULT : go to UNCALIBRATED\n");
				      if(ptpPortDS->parentWrModeON  == FALSE)
					DBG("parent node left White Rabbit Mode- WR Master-forced");
					DBG(" re-synchronization\n");
				      if(ptpPortDS->wrModeON             == FALSE)
					DBG("this node left White Rabbit Mode - WR Slave-forced ");
					DBG("re-synchronization\n");
				      
				      toState(PTP_UNCALIBRATED, rtOpts, ptpPortDS);

				   }
			}
			
		}
		break;

	default:
		break;
	}

	switch(ptpPortDS->portState)
	{
	case PTP_FAULTY:
		/* imaginary troubleshooting */

		DBG("event FAULT_CLEARED\n");

		toState(PTP_INITIALIZING, rtOpts, ptpPortDS);
		return;

	case PTP_UNCALIBRATED:
		DBGV("state: PTP_UNCALIBRATED\n");

		/* Execute WR protocol state machine */
		
#ifdef WRPTPv2	
		if(ptpPortDS->wrMode == WR_SLAVE)
			/* handling messages inside: handle()*/
			doWRState(rtOpts, ptpPortDS);
		else
			toState(PTP_SLAVE, rtOpts, ptpPortDS);
		
		ptpPortDS->msgTmpWrMessageID = NULL_WR_TLV;
#else
		if(ptpPortDS->wrMode == WR_SLAVE || ptpPortDS->wrMode == WR_MASTER)
			/* handling messages inside: handle()*/
			doWRState(rtOpts, ptpPortDS);
		else
			toState(PTP_SLAVE, rtOpts, ptpPortDS);
		
		ptpPortDS->msgTmpManagementId =  NULL_MANAGEMENT;
#endif		
		break;

	case PTP_LISTENING:
	case PTP_PASSIVE:
	case PTP_SLAVE:

		//if( linkUP == TRUE)
		  handle(rtOpts, ptpPortDS);

		if(timerExpired(&ptpPortDS->timers.announceReceipt) || linkUP == FALSE)
		{
			DBGV("event ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES\n");
			ptpPortDS->number_foreign_records = 0;
			ptpPortDS->foreign_record_i = 0;
			ptpPortDS->wrMode = NON_WR;

			if(!ptpPortDS->ptpClockDS->slaveOnly && ptpPortDS->ptpClockDS->clockQuality.clockClass != 255  )
			{
				m1(ptpPortDS);
				toState(PTP_MASTER, rtOpts, ptpPortDS);
			}
			else if (ptpPortDS->portState != PTP_LISTENING) //???/
				toState(PTP_LISTENING, rtOpts, ptpPortDS);
		}

		if (rtOpts->E2E_mode)
		{
			if(timerExpired(&ptpPortDS->timers.delayReq))
			{
				DBG("DELAYREQ_INTERVAL_TIMEOUT_EXPIRED\n");
				issueDelayReq(rtOpts,ptpPortDS);
			}
		}
		else
		{
			if(timerExpired(&ptpPortDS->timers.pdelayReq))
			{
				DBG("PDELAYREQ_INTERVAL_TIMEOUT_EXPIRED\n");
				issuePDelayReq(rtOpts,ptpPortDS);
			}
		}
		break;

	case PTP_MASTER:

		//if( linkUP == TRUE)
#ifdef WRPTPv2		
		if(ptpPortDS->wrMode == WR_MASTER  && ptpPortDS->wrPortState != WRS_IDLE)
		{
			/* handling messages inside: handle()*/
			doWRState(rtOpts, ptpPortDS);
			ptpPortDS->msgTmpWrMessageID = NULL_WR_TLV;
			break;
		}
		else
			handle(rtOpts, ptpPortDS);
#else		  
		handle(rtOpts, ptpPortDS);
#endif	  

		if(timerExpired(&ptpPortDS->timers.sync))
		{

			DBGV("event SYNC_INTERVAL_TIMEOUT_EXPIRES\n"); 
			issueSync(rtOpts, ptpPortDS);
			issueFollowup(rtOpts,ptpPortDS);
		}

		if(timerExpired(&ptpPortDS->timers.announceInterval))
		{

			DBGV("event ANNOUNCE_INTERVAL_TIMEOUT_EXPIRES\n");
			issueAnnounce(rtOpts, ptpPortDS);
		}

		if (!rtOpts->E2E_mode)
		{
			if(timerExpired(&ptpPortDS->timers.pdelayReq))
			{
				DBG("event PDELAYREQ_INTERVAL_TIMEOUT_EXPIRES\n");
				issuePDelayReq(rtOpts,ptpPortDS);
			}
		}

		if(ptpPortDS->ptpClockDS->slaveOnly || ptpPortDS->ptpClockDS->clockQuality.clockClass == 255)
			toState(PTP_LISTENING, rtOpts, ptpPortDS);

		break;

	case PTP_DISABLED:
		//if( linkUP == TRUE)
		  handle(rtOpts, ptpPortDS);
		break;

	default:
		DBG("(doState) do unrecognized state\n");
		break;
	}
}


/*
check and handle received messages
*/
void handle(RunTimeOpts *rtOpts, PtpPortDS *ptpPortDS)
{

	ssize_t length;
	Boolean isFromSelf;
	TimeInternal time = { 0, 0 };
	
#if 0
	/*
	* TODO: implement netSelect()
	*/
	if(!ptpPortDS->message_activity)
	{
		ret = netSelect(0, &ptpPortDS->netPath);

		if(ret < 0)
		{
			PERROR("failed to poll sockets, ret = %d\n", ret);
			toState(PTP_FAULTY, rtOpts, ptpPortDS);
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


	/* In White Rabbit event and general message are received in the same
	* way, no difference, any of the functions (netRecvEvent and netRecvGeneral
	* can receive any event, need to clean things later */

	length = netRecvMsg(ptpPortDS->msgIbuf, &ptpPortDS->netPath, &ptpPortDS->current_rx_ts);

	if(length < 0)
	{
		PERROR("Failed to receive on the event socket, len = %d\n", (int)length);
		DBG("Failed to receive on the event socket, len = %d\n", (int)length);
		toState(PTP_FAULTY, rtOpts, ptpPortDS);
		return;
	}
	else if(!length) /* nothing received, just exit */
	{
	  //DBG(" nothing received, just exit, len = %d\n", (int)length);
		return;
	}

	ptpPortDS->message_activity = TRUE;

	if(length < HEADER_LENGTH)
	{
		ERROR("Message shorter than header length\n");
		DBG("Message shorter than header length\n");
		toState(PTP_FAULTY, rtOpts, ptpPortDS);
		return;
	}


	// !!!!!!!!!!!!!!!!!
	/*
	* WR: to comply with old PTP daemon work flow
	* this should go to netRecvMsg
	*/
	time.seconds = ptpPortDS->current_rx_ts.utc;
	time.nanoseconds = ptpPortDS->current_rx_ts.nsec;
	// !!!!!!!!!!!!!!!!!

	msgUnpackHeader(ptpPortDS->msgIbuf, &ptpPortDS->msgTmpHeader);

	if(ptpPortDS->msgTmpHeader.versionPTP != ptpPortDS->versionNumber)
	{
		DBG("Ignored version %d message\n", ptpPortDS->msgTmpHeader.versionPTP);
		return;
	}

	if(ptpPortDS->msgTmpHeader.domainNumber != ptpPortDS->ptpClockDS->domainNumber)
	{
		DBG("Ignored message from domainNumber %d\n", ptpPortDS->msgTmpHeader.domainNumber);
		return;
	}

	/*Spec 9.5.2.2*/

	isFromSelf = (ptpPortDS->portIdentity.portNumber == ptpPortDS->msgTmpHeader.sourcePortIdentity.portNumber
		      && !memcmp(ptpPortDS->msgTmpHeader.sourcePortIdentity.clockIdentity, ptpPortDS->portIdentity.clockIdentity, CLOCK_IDENTITY_LENGTH));

	/* subtract the inbound latency adjustment if it is not a loop back and the
	time stamp seems reasonable */
  
	if(!isFromSelf && time.seconds > 0)
		subTime(&time, &time, &rtOpts->inboundLatency);

	//DBG("handle: messageType = 0x%x => ",ptpPortDS->msgTmpHeader.messageType);
  
	switch(ptpPortDS->msgTmpHeader.messageType)
	{
	case ANNOUNCE:
		handleAnnounce(&ptpPortDS->msgTmpHeader, ptpPortDS->msgIbuf, length, isFromSelf, rtOpts, ptpPortDS);
		break;

	case SYNC:
		handleSync(&ptpPortDS->msgTmpHeader, ptpPortDS->msgIbuf, length, &time, isFromSelf, rtOpts, ptpPortDS);
		break;

	case FOLLOW_UP:
		handleFollowUp(&ptpPortDS->msgTmpHeader, ptpPortDS->msgIbuf, length, isFromSelf, rtOpts, ptpPortDS);
		break;

	case DELAY_REQ:
		DBG("handle ..... DELAY_REQ\n");
		handleDelayReq(&ptpPortDS->msgTmpHeader, ptpPortDS->msgIbuf, length, isFromSelf, rtOpts, ptpPortDS);
		break;

#ifndef WR_MODE_ONLY
	case PDELAY_REQ:
		handlePDelayReq(&ptpPortDS->msgTmpHeader, ptpPortDS->msgIbuf, length, &time, isFromSelf, rtOpts, ptpPortDS);
		break;

	case PDELAY_RESP:
		DBG("handle ..... PDELAY_RESP\n");
		handlePDelayResp(&ptpPortDS->msgTmpHeader, ptpPortDS->msgIbuf,&time, length, isFromSelf, rtOpts, ptpPortDS);
		break;

	case PDELAY_RESP_FOLLOW_UP:
		DBG("handle ..... PDELAY_RESP_FOLLOW_UP\n");
		handlePDelayRespFollowUp(&ptpPortDS->msgTmpHeader, ptpPortDS->msgIbuf, length, isFromSelf, rtOpts, ptpPortDS);
		break;
#endif

	case DELAY_RESP:
		handleDelayResp(&ptpPortDS->msgTmpHeader, ptpPortDS->msgIbuf, length, isFromSelf, rtOpts, ptpPortDS);
		break;


	case MANAGEMENT:
		DBG("MANAGEMENT\n");
		handleManagement(&ptpPortDS->msgTmpHeader, ptpPortDS->msgIbuf, length, isFromSelf, rtOpts, ptpPortDS);
		break;

	case SIGNALING:
		
		handleSignaling(&ptpPortDS->msgTmpHeader, ptpPortDS->msgIbuf, length, isFromSelf, rtOpts, ptpPortDS);
		break;

	default:
		DBG("handle: unrecognized message\n");
		break;
	}
}

Boolean msgIsFromCurrentParent(MsgHeader *header, PtpPortDS *ptpPortDS)
{
	if(!memcmp(ptpPortDS->ptpClockDS->parentPortIdentity.clockIdentity,header->sourcePortIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH)
	   && (ptpPortDS->ptpClockDS->parentPortIdentity.portNumber == header->sourcePortIdentity.portNumber))
		return TRUE;
	else
		return FALSE;
}
 
/*spec 9.5.3*/
void handleAnnounce(MsgHeader *header, Octet *msgIbuf, ssize_t length, Boolean isFromSelf, RunTimeOpts *rtOpts, PtpPortDS *ptpPortDS)
{
	if(length < ANNOUNCE_LENGTH)
	{
		ERROR("Too short Announce message\n");
		toState(PTP_FAULTY, rtOpts, ptpPortDS);
		return;
	}

	if(length > ANNOUNCE_LENGTH)
		DBG("handle ..... ANNOUNCE (WR ?): perhaps a message from another White Rabbit node\n");
	else
		DBG("handle ..... ANNOUNCE: standard PTP\n");

	switch(ptpPortDS->portState )
	{
	case PTP_INITIALIZING:
	case PTP_FAULTY:
	case PTP_DISABLED:
		return;

	case PTP_UNCALIBRATED:

		/* White Rabbit Extension */
		//TODO: maybe change to wrConfig
		if(ptpPortDS->wrMode != NON_WR)
		{
			DBGV("Handle Announce: drop messages other than management in WR mode\n");
			return;
		}

		/* notice the missing break here - that's how it should be - TW */

	case PTP_SLAVE:

		if (isFromSelf)
		{
			DBGV("HandleAnnounce : Ignore message from self \n");
			return;
		}

		/* Valid announce message has been received : BMC algorithm will be executed */
		ptpPortDS->record_update = TRUE; 

		if(msgIsFromCurrentParent(header, ptpPortDS))
		{
			msgUnpackAnnounce(ptpPortDS->msgIbuf,&ptpPortDS->msgTmp.announce,header);

			if(ptpPortDS->msgTmp.announce.wr_flags != NON_WR)
				DBG("handle ..... WR_ANNOUNCE:  message from another White Rabbit node [wr_flag != NON_WR]\n");
			
#ifdef WRPTPv2			
			/*******  bug fix ???? *****
			* the problem was that we update directly the data in portDS but later
			* we executed BMC which uses data of foreignMasters, this was not updated,
			* so, if there was change of date received from the parent, it was ignored.
			* Therefore, now we update the best foreignMaster (which is the parent) and
			* then let the BMC do the job of updating portDS
			*/
			msgUnpackHeader(ptpPortDS->msgIbuf,&ptpPortDS->foreign[ptpPortDS->foreign_record_best].header);
			msgUnpackAnnounce(ptpPortDS->msgIbuf,&ptpPortDS->foreign[ptpPortDS->foreign_record_best].announce,&ptpPortDS->foreign[ptpPortDS->foreign_record_best].header);
#else			
			s1(header,&ptpPortDS->msgTmp.announce,ptpPortDS);
#endif
			/*Reset Timer handling Announce receipt timeout*/
			timerStart(&ptpPortDS->timers.announceReceipt,
				   ptpPortDS->announceReceiptTimeout * 1000 *
				   (pow_2(ptpPortDS->logAnnounceInterval)));

		} else {

			/*addForeign takes care of AnnounceUnpacking*/
			addForeign(ptpPortDS->msgIbuf,header,ptpPortDS);

			/*Reset Timer handling Announce receipt timeout*/
			timerStart(&ptpPortDS->timers.announceReceipt,
				   ptpPortDS->announceReceiptTimeout * 1000 *
				   (pow_2(ptpPortDS->logAnnounceInterval)));
			
		} 
		break;

	case PTP_MASTER:
	default :

		if (isFromSelf)
		{
			DBG("handle ..... ANNOUNCE:  Ignore message from self \n");
			return;
		}

		DBGV("Announce message from another foreign master");
		addForeign(ptpPortDS->msgIbuf,header,ptpPortDS);
		ptpPortDS->record_update = TRUE;
		break;

	} /* switch (port_state) */
}

void handleSync(MsgHeader *header, Octet *msgIbuf, ssize_t length, TimeInternal *time, Boolean isFromSelf, RunTimeOpts *rtOpts, PtpPortDS *ptpPortDS)
{
	TimeInternal OriginTimestamp;
	TimeInternal correctionField;



	if(length < SYNC_LENGTH)
	{
		ERROR("Too short Sync message\n");
		toState(PTP_FAULTY, rtOpts, ptpPortDS);
		return;
	}

	switch(ptpPortDS->portState)
	{
		case PTP_INITIALIZING:
		case PTP_FAULTY:
		case PTP_DISABLED:
			return;

		case PTP_UNCALIBRATED:
			/* White Rabbit */
			if(ptpPortDS->wrMode != NON_WR)
			{
				DBGV("handle ..... SYNC   : disregaurd messages other than management \n");
				return;
			}

		case PTP_SLAVE:
			if (isFromSelf)
			{
				DBGV("HandleSync: Ignore message from self \n");
				return;
			}

			if (msgIsFromCurrentParent(header, ptpPortDS))
			{
				/*
				* WR: HW timestamps
				*/
				ptpPortDS->sync_receive_time.seconds = ptpPortDS->current_rx_ts.utc;
				ptpPortDS->sync_receive_time.nanoseconds = ptpPortDS->current_rx_ts.nsec;
				ptpPortDS->sync_receive_time.phase = ptpPortDS->current_rx_ts.phase;

				if ((header->flagField[0] & 0x02) == TWO_STEP_FLAG)
				{
					DBG("handle ..... SYNC    : two step clock mode\n");
					ptpPortDS->waitingForFollow = TRUE;
					ptpPortDS->recvSyncSequenceId = header->sequenceId;

					/*Save correctionField of Sync message to local variable*/
					integer64_to_internalTime(header->correctionfield,&correctionField);

					/* remeber correction field */
					ptpPortDS->lastSyncCorrectionField.seconds = correctionField.seconds;
					ptpPortDS->lastSyncCorrectionField.nanoseconds = correctionField.nanoseconds;

					break;
				}
				else
				{
					ERROR("BAD !!!!!!!! we don't use this; handle Sync msg, one step clock\n");
					msgUnpackSync(ptpPortDS->msgIbuf,&ptpPortDS->msgTmp.sync);
					integer64_to_internalTime(ptpPortDS->msgTmpHeader.correctionfield,&correctionField);
					timeInternal_display(&correctionField);
					ptpPortDS->waitingForFollow = FALSE;
					toInternalTime(&OriginTimestamp,&ptpPortDS->msgTmp.sync.originTimestamp);
					updateOffset(&OriginTimestamp,&ptpPortDS->sync_receive_time,&ptpPortDS->ofm_filt,rtOpts,ptpPortDS,&correctionField);
					updateClock(rtOpts,ptpPortDS);

					break;
				}

			}
			break;


	case PTP_MASTER:
	default :

/* WR switch never works in boundary clock mode */
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
				issueFollowup(time,rtOpts,ptpPortDS);
				break;
			}
#endif
		  break;
	}
}


void handleFollowUp(MsgHeader *header, Octet *msgIbuf, ssize_t length, Boolean isFromSelf, RunTimeOpts *rtOpts, PtpPortDS *ptpPortDS)
{
	TimeInternal preciseOriginTimestamp;
	TimeInternal correctionField;
	
	DBGV("HandleFollowup : Follow up message received \n");
	
	if(length < FOLLOW_UP_LENGTH)
	{
		ERROR("Too short FollowUp message\n");
		toState(PTP_FAULTY, rtOpts, ptpPortDS);
		return;
	}

	if (isFromSelf)
	{
		DBGV("Handlefollowup : Ignore message from self \n");
		return;
	}

	switch(ptpPortDS->portState )
	{
		case PTP_INITIALIZING:
		case PTP_FAULTY:
		case PTP_DISABLED:
		case PTP_LISTENING:

			DBGV("Handfollowup : disreguard \n");
			return;

		case PTP_UNCALIBRATED:

		  /*White Rabbit */
		  if(ptpPortDS->wrMode != NON_WR)
		  {
		    DBGV("Handleannounce WR mode: disregaurd messages other than management \n");
		    return;
		  }

		case PTP_SLAVE:

//		isFromCurrentParent = !memcmp(ptpPortDS->parentPortIdentity.clockIdentity,header->sourcePortIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH)
//							  && (ptpPortDS->parentPortIdentity.portNumber == header->sourcePortIdentity.portNumber);


		if (msgIsFromCurrentParent(header, ptpPortDS))
		{
			if (ptpPortDS->waitingForFollow)
			{
				if ((ptpPortDS->recvSyncSequenceId == header->sequenceId))
				{

					msgUnpackFollowUp(ptpPortDS->msgIbuf,&ptpPortDS->msgTmp.follow);

					DBG("handle ..... FOLLOW_UP:  , succedded [sec.msb = %ld sec.lsb = %lld nanosec = %lld]\n", \
					    (unsigned      long)ptpPortDS->msgTmp.follow.preciseOriginTimestamp.secondsField.msb, \
					    (unsigned long long)ptpPortDS->msgTmp.follow.preciseOriginTimestamp.secondsField.lsb, \
					    (unsigned long long)ptpPortDS->msgTmp.follow.preciseOriginTimestamp.nanosecondsField);

					
					DBGM("handle FollowUP msg, succedded: \n\t\t sec.msb = %ld \n\t\t sec.lsb = %lld \n\t\t nanosec = %lld\n", \
					    (unsigned      long)ptpPortDS->msgTmp.follow.preciseOriginTimestamp.secondsField.msb, \
					    (unsigned long long)ptpPortDS->msgTmp.follow.preciseOriginTimestamp.secondsField.lsb, \
					    (unsigned long long)ptpPortDS->msgTmp.follow.preciseOriginTimestamp.nanosecondsField);

					ptpPortDS->waitingForFollow = FALSE;
						
					/*copy the precise followUP info to local variable*/
					toInternalTime(&preciseOriginTimestamp,&ptpPortDS->msgTmp.follow.preciseOriginTimestamp);

					preciseOriginTimestamp.phase = 0;

						/*get correction field form the header to local variable*/
					integer64_to_internalTime(ptpPortDS->msgTmpHeader.correctionfield,&correctionField);

						/*add to correctionField last sych correction (?)*/
					addTime(&correctionField,&correctionField,&ptpPortDS->lastSyncCorrectionField);
					
					wr_servo_got_sync(ptpPortDS, preciseOriginTimestamp, ptpPortDS->sync_receive_time);
					

					issueDelayReq(rtOpts,ptpPortDS);

						break;
				}
				else DBG("handle ..... FOLLOW_UP:, SequenceID doesn't match with last Sync message \n");
				//else DBGV("SequenceID doesn't match with last Sync message \n");

			}
			else DBG("handle ..... FOLLOW_UP:, Slave was not waiting a follow up message \n");
			//else DBGV("Slave was not waiting a follow up message \n");
		}
		else DBG("handle ..... FOLLOW_UP:, Follow up message is not from current parent \n");
		//else DBGV("Follow up message is not from current parent \n");

		case PTP_MASTER:
			DBGV("handle ..... FOLLOW_UP: Follow up message received from another master \n");
			break;

		default:
		DBG("handle ..... FOLLOW_UP: do unrecognized state\n");
		break;
	}//Switch on (port_state)

}


static Integer32 phase_to_cf_units(Integer32 phase)
{
  return (Integer32) ((double)phase / 1000.0 * 65536.0);
}

void handleDelayReq(MsgHeader *header,Octet *msgIbuf,ssize_t length,Boolean isFromSelf,RunTimeOpts *rtOpts,PtpPortDS *ptpPortDS)
{
	if (!rtOpts->E2E_mode)
	{
		ERROR("Delay messages are disreguard in Peer to Peer mode \n");
		return;
	}

	DBGV("delayReq message received : \n");

	if(length < DELAY_REQ_LENGTH)
	{
		ERROR("short DelayReq message\n");
		toState(PTP_FAULTY, rtOpts, ptpPortDS);
		return;
	}

	switch(ptpPortDS->portState )
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
			ptpPortDS->delay_req_send_time.seconds = time->seconds;
			ptpPortDS->delay_req_send_time.nanoseconds = time->nanoseconds;
					
			/*Add latency*/
			addTime(&ptpPortDS->delay_req_send_time,&ptpPortDS->delay_req_send_time,&rtOpts->outboundLatency);
#endif
			break;
		}

		break;

	case PTP_MASTER:

		DBG("handle ..... DELAY_REQ:, succedded\n");
		msgUnpackHeader(ptpPortDS->msgIbuf,&ptpPortDS->delayReqHeader);
				
		/* FIXME: do this, but properly */
		ptpPortDS->delayReqHeader.correctionfield.msb = 0;
		ptpPortDS->delayReqHeader.correctionfield.lsb = phase_to_cf_units(ptpPortDS->current_rx_ts.phase);

		issueDelayResp(&ptpPortDS->delayReqHeader,rtOpts,ptpPortDS);
		break;

	default:
		DBG("handle ..... DELAY_REQdo unrecognized state\n");
		break;
	}
}



void handleDelayResp(MsgHeader *header,Octet *msgIbuf,ssize_t length,Boolean isFromSelf,RunTimeOpts *rtOpts,PtpPortDS *ptpPortDS)
{
	Boolean isFromCurrentParent = FALSE;
	TimeInternal requestReceiptTimestamp;
	TimeInternal correctionField;

	if (!rtOpts->E2E_mode)
		return;
	
	DBGV("delayResp message received : \n");

	if(length < DELAY_RESP_LENGTH)
	{
		ERROR("Too short DelayResp message\n");
		toState(PTP_FAULTY, rtOpts, ptpPortDS);
		return;
	}

	switch(ptpPortDS->portState )
	{
	case PTP_INITIALIZING:
	case PTP_FAULTY:
	case PTP_DISABLED:
	case PTP_UNCALIBRATED:
	case PTP_LISTENING:

		
		DBGV("HandledelayResp : disregard \n");
		return;

	case PTP_SLAVE:


		msgUnpackDelayResp(ptpPortDS->msgIbuf,&ptpPortDS->msgTmp.resp);

		DBG("handle ..... DELAY_RESP, succedded: [sec.msb = %ld sec.lsb = %lld nanosec = %lld]\n", \
		    (unsigned      long)ptpPortDS->msgTmp.resp.receiveTimestamp.secondsField.msb, \
		    (unsigned long long)ptpPortDS->msgTmp.resp.receiveTimestamp.secondsField.lsb, \
		    (unsigned long long)ptpPortDS->msgTmp.resp.receiveTimestamp.nanosecondsField);		
		
		DBGM("handle ..... DELAY_RESP, succedded: \n\t\t sec.msb = %ld \n\t\t sec.lsb = %lld \n\t\t nanosec = %lld\n", \
		    (unsigned      long)ptpPortDS->msgTmp.resp.receiveTimestamp.secondsField.msb, \
		    (unsigned long long)ptpPortDS->msgTmp.resp.receiveTimestamp.secondsField.lsb, \
		    (unsigned long long)ptpPortDS->msgTmp.resp.receiveTimestamp.nanosecondsField);

		
		isFromCurrentParent = msgIsFromCurrentParent(header, ptpPortDS);
		
		/* what the f**k is this? */
		if ((memcmp(ptpPortDS->portIdentity.clockIdentity, ptpPortDS->msgTmp.resp.requestingPortIdentity.clockIdentity, CLOCK_IDENTITY_LENGTH) == 0)
		    && ((ptpPortDS->sentDelayReqSequenceId - 1) == header->sequenceId)
		    && (ptpPortDS->portIdentity.portNumber == ptpPortDS->msgTmp.resp.requestingPortIdentity.portNumber)
		    && isFromCurrentParent)
		{
			/* copy timestamp to local variable */
			toInternalTime(&requestReceiptTimestamp, &ptpPortDS->msgTmp.resp.receiveTimestamp);


			/* t_4 */
			ptpPortDS->delay_req_receive_time.seconds = requestReceiptTimestamp.seconds;
			ptpPortDS->delay_req_receive_time.nanoseconds = requestReceiptTimestamp.nanoseconds;
			ptpPortDS->delay_req_receive_time.phase = requestReceiptTimestamp.phase;

			/* coppy correctionField from header->cF to local variable (correctionField) */
			integer64_to_internalTime(header->correctionfield,&correctionField);

			wr_servo_got_delay(ptpPortDS, header->correctionfield.lsb);
			wr_servo_update(ptpPortDS);

			ptpPortDS->logMinDelayReqInterval = header->logMessageInterval;
			
		}
		break;
	}
}


#ifndef WR_MODE_ONLY
void handlePDelayReq(MsgHeader *header, Octet *msgIbuf, ssize_t length, TimeInternal *time, Boolean isFromSelf, RunTimeOpts *rtOpts, PtpPortDS *ptpPortDS)
{

  if (!rtOpts->E2E_mode)
   {

	DBGV("PdelayReq message received : \n");

	 if(length < PDELAY_REQ_LENGTH)
	 {
	ERROR("short PDelayReq message\n");
	toState(PTP_FAULTY, rtOpts, ptpPortDS);
	return;
	 }

	switch(ptpPortDS->portState )
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
			ptpPortDS->pdelay_req_send_time.seconds = time->seconds;
			ptpPortDS->pdelay_req_send_time.nanoseconds = time->nanoseconds;

			/*Add latency*/
			addTime(&ptpPortDS->pdelay_req_send_time,&ptpPortDS->pdelay_req_send_time,&rtOpts->outboundLatency);
			break;
		}
		else
		{
			DBG("handle PDelayReq msg, succedded\n");
			msgUnpackHeader(ptpPortDS->msgIbuf,&ptpPortDS->PdelayReqHeader);
			issuePDelayResp(time,header,rtOpts,ptpPortDS);
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

void handlePDelayResp(MsgHeader *header, Octet *msgIbuf, TimeInternal *time,ssize_t length, Boolean isFromSelf, RunTimeOpts *rtOpts, PtpPortDS *ptpPortDS)
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
	toState(PTP_FAULTY, rtOpts, ptpPortDS);
	return;
	 }

	switch(ptpPortDS->portState )
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
		 issuePDelayRespFollowUp(time,&ptpPortDS->PdelayReqHeader,rtOpts,ptpPortDS);
		 break;
		}

			msgUnpackPDelayResp(ptpPortDS->msgIbuf,&ptpPortDS->msgTmp.presp);

			DBG("handle PDelayResp msg, succedded [SLAVE sec.msb = %ld sec.lsb = %lld nanosec = %lld]\n",\
			(unsigned      long)ptpPortDS->msgTmp.presp.requestReceiptTimestamp.secondsField.msb,\
			(unsigned long long)ptpPortDS->msgTmp.presp.requestReceiptTimestamp.secondsField.lsb,\
			(unsigned long long)ptpPortDS->msgTmp.presp.requestReceiptTimestamp.nanosecondsField);			
			
			DBGM("handle PDelayResp msg, succedded [SLAVE]: \n\t\t sec.msb = %ld \n\t\t sec.lsb = %lld \n\t\t nanosec = %lld\n",\
			(unsigned      long)ptpPortDS->msgTmp.presp.requestReceiptTimestamp.secondsField.msb,\
			(unsigned long long)ptpPortDS->msgTmp.presp.requestReceiptTimestamp.secondsField.lsb,\
			(unsigned long long)ptpPortDS->msgTmp.presp.requestReceiptTimestamp.nanosecondsField);

			isFromCurrentParent = !memcmp(ptpPortDS->ptpClockDS->parentPortIdentity.clockIdentity,header->sourcePortIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH)
							  && (ptpPortDS->ptpClockDS->parentPortIdentity.portNumber == header->sourcePortIdentity.portNumber);

			if (!   ((ptpPortDS->sentPDelayReqSequenceId == header->sequenceId)
			   && (!memcmp(ptpPortDS->portIdentity.clockIdentity,ptpPortDS->msgTmp.presp.requestingPortIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH))
			   && ( ptpPortDS->portIdentity.portNumber == ptpPortDS->msgTmp.presp.requestingPortIdentity.portNumber)))

			{
				if ((header->flagField[0] & 0x02) == TWO_STEP_FLAG)
				{
					/*Store t4 (Fig 35)*/
					ptpPortDS->pdelay_resp_receive_time.seconds = time->seconds;
					ptpPortDS->pdelay_resp_receive_time.nanoseconds = time->nanoseconds;
//
					DBG("\n\n\ntime[two steps]: ptpPortDS->pdelay_resp_receive_time.seconds     = %ld\n",time->seconds);
					DBG("\n\n\ntime[two steps]: ptpPortDS->pdelay_resp_receive_time.nanoseconds = %ld\n\n\n",time->nanoseconds);

					/*store t2 (Fig 35)*/
					toInternalTime(&requestReceiptTimestamp,&ptpPortDS->msgTmp.presp.requestReceiptTimestamp);
					ptpPortDS->pdelay_req_receive_time.seconds = requestReceiptTimestamp.seconds;
					ptpPortDS->pdelay_req_receive_time.nanoseconds = requestReceiptTimestamp.nanoseconds;

					integer64_to_internalTime(header->correctionfield,&correctionField);
					ptpPortDS->lastPdelayRespCorrectionField.seconds = correctionField.seconds;
					ptpPortDS->lastPdelayRespCorrectionField.nanoseconds = correctionField.nanoseconds;
//
					break;
				}//Two Step Clock

				else //One step Clock
				{
					/*Store t4 (Fig 35)*/
					ptpPortDS->pdelay_resp_receive_time.seconds = time->seconds;
					ptpPortDS->pdelay_resp_receive_time.nanoseconds = time->nanoseconds;

					DBG("\n\n\ntime[one step]: ptpPortDS->pdelay_resp_receive_time.seconds     = %ld\n",time->seconds);
					DBG("\n\n\ntime[one step]: ptpPortDS->pdelay_resp_receive_time.nanoseconds = %ld\n\n\n",time->nanoseconds);

					integer64_to_internalTime(header->correctionfield,&correctionField);

					printf("\n\n ----------- calculate after receiving handlePDelayResp [one step] msg ---------\n");

					//					updatePeerDelay (&ptpPortDS->owd_filt,rtOpts,ptpPortDS,&correctionField,FALSE);

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

					issuePDelayRespFollowUp(time,&ptpPortDS->PdelayReqHeader,rtOpts,ptpPortDS);
					break;
				}


				msgUnpackPDelayResp(ptpPortDS->msgIbuf,&ptpPortDS->msgTmp.presp);

				DBG("handle PDelayResp msg, succedded [MASTER: sec.msb = %ld sec.lsb = %lld nanosec = %lld]\n",\
				(unsigned      long)ptpPortDS->msgTmp.presp.requestReceiptTimestamp.secondsField.msb,\
				(unsigned long long)ptpPortDS->msgTmp.presp.requestReceiptTimestamp.secondsField.lsb,\
				(unsigned long long)ptpPortDS->msgTmp.presp.requestReceiptTimestamp.nanosecondsField);				
				
				DBGM("handle PDelayResp msg, succedded [MASTER: \n\t\t sec.msb = %ld \n\t\t sec.lsb = %lld \n\t\t nanosec = %lld\n",\
				(unsigned      long)ptpPortDS->msgTmp.presp.requestReceiptTimestamp.secondsField.msb,\
				(unsigned long long)ptpPortDS->msgTmp.presp.requestReceiptTimestamp.secondsField.lsb,\
				(unsigned long long)ptpPortDS->msgTmp.presp.requestReceiptTimestamp.nanosecondsField);


				isFromCurrentParent = !memcmp(ptpPortDS->ptpClockDS->parentPortIdentity.clockIdentity,header->sourcePortIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH)
							  && (ptpPortDS->ptpClockDS->parentPortIdentity.portNumber == header->sourcePortIdentity.portNumber);

				if (!   ((ptpPortDS->sentPDelayReqSequenceId == header->sequenceId)
					&& (!memcmp(ptpPortDS->portIdentity.clockIdentity,ptpPortDS->msgTmp.presp.requestingPortIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH))
					&& ( ptpPortDS->portIdentity.portNumber == ptpPortDS->msgTmp.presp.requestingPortIdentity.portNumber)))

				{
					if ((header->flagField[0] & 0x02) == TWO_STEP_FLAG)
					{
						/*Store t4 (Fig 35)*/
						ptpPortDS->pdelay_resp_receive_time.seconds = time->seconds;
						ptpPortDS->pdelay_resp_receive_time.nanoseconds = time->nanoseconds;


						/*store t2 (Fig 35)*/
						toInternalTime(&requestReceiptTimestamp,&ptpPortDS->msgTmp.presp.requestReceiptTimestamp);
						ptpPortDS->pdelay_req_receive_time.seconds = requestReceiptTimestamp.seconds;
						ptpPortDS->pdelay_req_receive_time.nanoseconds = requestReceiptTimestamp.nanoseconds;

						integer64_to_internalTime(header->correctionfield,&correctionField);
						ptpPortDS->lastPdelayRespCorrectionField.seconds = correctionField.seconds;
						ptpPortDS->lastPdelayRespCorrectionField.nanoseconds = correctionField.nanoseconds;
						break;
					}//Two Step Clock

					else //One step Clock
					{
						/*Store t4 (Fig 35)*/
						ptpPortDS->pdelay_resp_receive_time.seconds = time->seconds;
						ptpPortDS->pdelay_resp_receive_time.nanoseconds = time->nanoseconds;

						integer64_to_internalTime(header->correctionfield,&correctionField);

						printf("\n\n ----------- calculate after receiving handlePDelayResp [one step] msg ---------\n");

						//						updatePeerDelay (&ptpPortDS->owd_filt,rtOpts,ptpPortDS,&correctionField,FALSE);

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

void handlePDelayRespFollowUp(MsgHeader *header, Octet *msgIbuf, ssize_t length, Boolean isFromSelf, RunTimeOpts *rtOpts, PtpPortDS *ptpPortDS){

if (!rtOpts->E2E_mode)
 {
	TimeInternal responseOriginTimestamp;
	TimeInternal correctionField;

	DBGV("PdelayRespfollowup message received : \n");

	 if(length < PDELAY_RESP_FOLLOW_UP_LENGTH)
	 {
	ERROR("short PDelayRespfollowup message\n");
	toState(PTP_FAULTY, rtOpts, ptpPortDS);
	return;
	 }

	switch(ptpPortDS->portState )
	{
		case PTP_INITIALIZING:
		case PTP_FAULTY:
		case PTP_DISABLED:
		case PTP_UNCALIBRATED:
			DBGV("HandlePdelayResp : disreguard \n");
			return;

		case PTP_SLAVE:

		if (header->sequenceId == ptpPortDS->sentPDelayReqSequenceId-1)
		{
			msgUnpackPDelayRespFollowUp(ptpPortDS->msgIbuf,&ptpPortDS->msgTmp.prespfollow);

			DBG("handle handlePDelayRespFollowUp msg [MASTER], succedded: \n\t\t sec.msb = %ld \n\t\t sec.lsb = %lld \n\t\t nanosec = %lld\n",\
			(unsigned      long)ptpPortDS->msgTmp.prespfollow.responseOriginTimestamp.secondsField.msb,\
			(unsigned long long)ptpPortDS->msgTmp.prespfollow.responseOriginTimestamp.secondsField.lsb,\
			(unsigned long long)ptpPortDS->msgTmp.prespfollow.responseOriginTimestamp.nanosecondsField);


			toInternalTime(&responseOriginTimestamp,&ptpPortDS->msgTmp.prespfollow.responseOriginTimestamp);
			ptpPortDS->pdelay_resp_send_time.seconds = responseOriginTimestamp.seconds;
			ptpPortDS->pdelay_resp_send_time.nanoseconds = responseOriginTimestamp.nanoseconds;
		    integer64_to_internalTime(ptpPortDS->msgTmpHeader.correctionfield,&correctionField);
			addTime(&correctionField,&correctionField,&ptpPortDS->lastPdelayRespCorrectionField);

			DBG("\n\n ------------ calculate after receiving handlePDelayRespFollowUp msg --------\n");

			//			updatePeerDelay (&ptpPortDS->owd_filt,rtOpts,ptpPortDS,&correctionField,TRUE);

			DBG("\n -------------------------------finish calculation ------------------------\n\n");

			break;
		}

		case PTP_MASTER:

		if (header->sequenceId == ptpPortDS->sentPDelayReqSequenceId-1)
		{
			msgUnpackPDelayRespFollowUp(ptpPortDS->msgIbuf,&ptpPortDS->msgTmp.prespfollow);

			DBG("handle handlePDelayRespFollowUp msg [MASTER], succedded: \n\t\t sec.msb = %ld \n\t\t sec.lsb = %lld \n\t\t nanosec = %lld\n",\
			(unsigned      long)ptpPortDS->msgTmp.prespfollow.responseOriginTimestamp.secondsField.msb,\
			(unsigned long long)ptpPortDS->msgTmp.prespfollow.responseOriginTimestamp.secondsField.lsb,\
			(unsigned long long)ptpPortDS->msgTmp.prespfollow.responseOriginTimestamp.nanosecondsField);


			toInternalTime(&responseOriginTimestamp,&ptpPortDS->msgTmp.prespfollow.responseOriginTimestamp);
			ptpPortDS->pdelay_resp_send_time.seconds = responseOriginTimestamp.seconds;
			ptpPortDS->pdelay_resp_send_time.nanoseconds = responseOriginTimestamp.nanoseconds;
		    integer64_to_internalTime(ptpPortDS->msgTmpHeader.correctionfield,&correctionField);
			addTime(&correctionField,&correctionField,&ptpPortDS->lastPdelayRespCorrectionField);

			DBG("\n\n ------------ calculate after receiving handlePDelayRespFollowUp msg --------\n");

			updatePeerDelay (&ptpPortDS->owd_filt,rtOpts,ptpPortDS,&correctionField,TRUE);

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

#endif // WR_MODE_ONLY

/*
WR: custom White Rabbit management
*/
void handleManagement(MsgHeader *header, Octet *msgIbuf, ssize_t length, Boolean isFromSelf, RunTimeOpts *rtOpts, PtpPortDS *ptpPortDS)
{
	MsgManagement management;

	if(isFromSelf)
		return;


	msgUnpackWRManagement(ptpPortDS->msgIbuf,&management,&(ptpPortDS->msgTmpManagementId),ptpPortDS);


	switch(ptpPortDS->msgTmpManagementId)
	{
#ifndef WRPTPv2
//we dont' use this staff !!!
	case CALIBRATE:

		DBG("WR Management msg [CALIBRATE]:	\
	\n\tcalibrateSendPattern  = %32x			\
	\n\tcalPeriod             = %32lld us\n",\
		    ptpPortDS->otherNodeCalSendPattern,	  \
		    (unsigned long long)ptpPortDS->otherNodeCalPeriod);
		break;

	case CALIBRATED:

		DBG("WR Management msg [CALIBRATED]: \
	\n\tdeltaTx = %16lld			     \
	\n\tdeltaRx = %16lld\n", 
		    ((unsigned long long)ptpPortDS->otherNodeDeltaTx.scaledPicoseconds.msb)<<32 | (unsigned long long)ptpPortDS->otherNodeDeltaTx.scaledPicoseconds.lsb, \
		    ((unsigned long long)ptpPortDS->otherNodeDeltaRx.scaledPicoseconds.msb)<<32 | (unsigned long long)ptpPortDS->otherNodeDeltaRx.scaledPicoseconds.lsb);
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
#else
	default:
		DBG("\n\nhandle Management msg : no support !!! \n\n");
		break;
#endif		
	}


#ifndef WRPTPv2
//we dont' use this staff !!!
	/*
	* here the master recognizes that it talks with WR slave
	* which identifies itself and the calibration is statrted
	* if the calibration is already being done, just ignore this
	*/
	if(ptpPortDS->wrMode        == WR_MASTER &&
	   ptpPortDS->msgTmpManagementId == SLAVE_PRESENT && 
	   ptpPortDS->portState          != PTP_UNCALIBRATED )
	  toState(PTP_UNCALIBRATED,rtOpts,ptpPortDS);
#endif	  

}


void handleSignaling(MsgHeader *header, Octet *msgIbuf, ssize_t length, Boolean isFromSelf, RunTimeOpts *rtOpts, PtpPortDS *ptpPortDS) 
{
#ifdef WRPTPv2  
	MsgSignaling signalingMsg;

	if(isFromSelf)
		return;


	msgUnpackWRSignalingMsg(ptpPortDS->msgIbuf,&signalingMsg,&(ptpPortDS->msgTmpWrMessageID),ptpPortDS);

	// a hack
	//ptpPortDS->msgTmpManagementId = ptpPortDS->msgTmpWrMessageID;

	switch(ptpPortDS->msgTmpWrMessageID)
	{

	case CALIBRATE:

		DBG("handle ..... WR_SIGNALING, [CALIBRATE]:	\
	\n\tcalibrateSendPattern  = %32x			\
	\n\tcalPeriod    	  = %32lld us\n",\
		    ptpPortDS->otherNodeCalSendPattern,	  \
		    (unsigned long long)ptpPortDS->otherNodeCalPeriod);
		break;

	case CALIBRATED:

		DBG("handle ..... WR_SIGNALING [CALIBRATED]: \
	\n\tdeltaTx = %16lld			     \
	\n\tdeltaRx = %16lld\n", 
		    ((unsigned long long)ptpPortDS->otherNodeDeltaTx.scaledPicoseconds.msb)<<32 | (unsigned long long)ptpPortDS->otherNodeDeltaTx.scaledPicoseconds.lsb, \
		    ((unsigned long long)ptpPortDS->otherNodeDeltaRx.scaledPicoseconds.msb)<<32 | (unsigned long long)ptpPortDS->otherNodeDeltaRx.scaledPicoseconds.lsb);
		break;

	case SLAVE_PRESENT:
		DBG("handle ..... WR_SIGNALING [SLAVE_PRESENT], succedded \n");
		break;
	      
	case LOCK:
		DBG("handle ..... WR_SIGNALING [LOCK], succedded \n");
		break;

	case LOCKED:

		DBG("handle ..... WR_SIGNALING [LOCKED], succedded \n");
		break;

	case WR_MODE_ON:

		DBG("handle ..... WR_SIGNALING [WR_LINK_ON], succedded \n");
		break;

	default:
		DBG("handle ..... WR_SIGNALING [UNKNOWN], failed \n");
		break;
	}

	/***************** TURN ON WR_MASTER mode ********************
	* here the master recognizes that it talks with WR slave
	* which identifies itself and the calibration is statrted
	* if the calibration is already being done, just ignore this
	*/

	/* here we deternime that a node should be WR_MASTER */
	if(ptpPortDS->portState         == PTP_MASTER    && \
	   ptpPortDS->msgTmpWrMessageID == SLAVE_PRESENT && \
	  (ptpPortDS->wrConfig      == WR_M_ONLY     || 
	   ptpPortDS->wrConfig      == WR_M_AND_S     ))
	{
	     ///////// fucken important !! /////////////
	     DBGWRFSM("wrMode <= WR_MASTER\n");
	     ptpPortDS->wrMode = WR_MASTER;
	     ///////////////////////////////////////////
	     toWRState(WRS_M_LOCK, rtOpts, ptpPortDS);

	     //toState(PTP_UNCALIBRATED,rtOpts,ptpPortDS);
	}

		

		
		
#endif /*WRPTPv2*/
}


/*Pack and send on general multicast ip adress an Announce message*/
void issueAnnounce(RunTimeOpts *rtOpts,PtpPortDS *ptpPortDS)
{
	UInteger16 announce_len;

	msgPackAnnounce(ptpPortDS->msgObuf,ptpPortDS);
#ifdef WRPTPv2
	if (ptpPortDS->wrConfig != NON_WR && ptpPortDS->wrConfig != WR_S_ONLY)
#else
	if (ptpPortDS->wrMode != NON_WR)
#endif
	
		announce_len = WR_ANNOUNCE_LENGTH;
	else
		announce_len = ANNOUNCE_LENGTH;

	if (!netSendGeneral(ptpPortDS->msgObuf,announce_len,&ptpPortDS->netPath))
	{
		toState(PTP_FAULTY,rtOpts,ptpPortDS);
		DBGV("Announce message can't be sent -> FAULTY state \n");
		DBG("issue  ..... ANNOUNCE : Announce Msg, failed \n");
	}
	else
	{
		if (ptpPortDS->wrMode != NON_WR)
		  DBG("issue  ..... WR ANNOUNCE : succedded \n");
		else
		  DBG("issue  ..... ANNOUNCE : succedded \n");
		DBGV("Announce MSG sent ! \n");
		ptpPortDS->sentAnnounceSequenceId++;
	}
}


/*Pack and send on event multicast ip adress a Sync message*/
void issueSync(RunTimeOpts *rtOpts,PtpPortDS *ptpPortDS)
{
	Timestamp originTimestamp;

	/* send a dummy origin timestamp */
	memset(&originTimestamp, 0, sizeof(originTimestamp));
	
	msgPackSync(ptpPortDS->msgObuf,&originTimestamp,ptpPortDS);
	

	if (!netSendEvent(ptpPortDS->msgObuf,SYNC_LENGTH,&ptpPortDS->netPath,&ptpPortDS->synch_tx_ts))
	{
		toState(PTP_FAULTY,rtOpts,ptpPortDS);
		ptpPortDS->pending_Synch_tx_ts = FALSE;
		DBGV("Sync message can't be sent -> FAULTY state \n");
		DBG("issue  ..... SYNC:   failed");
	}
	else
	{
		DBG("issue  ..... SYNC: succedded [synch timestamp: %s]\n", \
		format_wr_timestamp(ptpPortDS->synch_tx_ts));
		ptpPortDS->pending_tx_ts = TRUE;
		ptpPortDS->pending_Synch_tx_ts = TRUE;
		ptpPortDS->sentSyncSequenceId++;
	}



}





/*Pack and send on general multicast ip adress a FollowUp message*/
//void issueFollowup(TimeInternal *time,RunTimeOpts *rtOpts,PtpPortDS *ptpPortDS)
void issueFollowup(RunTimeOpts *rtOpts,PtpPortDS *ptpPortDS)
{


	msgPackFollowUp(ptpPortDS->msgObuf,ptpPortDS);

	if (!netSendGeneral(ptpPortDS->msgObuf,FOLLOW_UP_LENGTH,&ptpPortDS->netPath))
	{
		toState(PTP_FAULTY,rtOpts,ptpPortDS);
		DBGV("FollowUp message can't be sent -> FAULTY state \n");
		DBG("issue  ..... FOLLOW_UP: failed\n");
	}
	else
	{
		DBG("issue  ..... FOLLOW_UP: succedded [sending time of sync tx: sec = %lld  nanosec = %lld]\n",\
		(unsigned long long)ptpPortDS->synch_tx_ts.utc,\
		(unsigned long long)ptpPortDS->synch_tx_ts.nsec);

	}
}


/*Pack and send on event multicast ip adress a DelayReq message*/
void issueDelayReq(RunTimeOpts *rtOpts,PtpPortDS *ptpPortDS)
{

	Timestamp originTimestamp;

	memset(&originTimestamp, 0, sizeof(originTimestamp));
	msgPackDelayReq(ptpPortDS->msgObuf,&originTimestamp,ptpPortDS);

	if (!netSendEvent(ptpPortDS->msgObuf,DELAY_REQ_LENGTH,&ptpPortDS->netPath,&ptpPortDS->delayReq_tx_ts))
	{
		toState(PTP_FAULTY,rtOpts,ptpPortDS);
		// ptpPortDS->new_tx_tag_read = FALSE;
		// ptpPortDS->pending_DelayReq_tx_ts = FALSE;
		DBGV("delayReq message can't be sent -> FAULTY state \n");
		DBG("issue  ..... DELAY_REQ: failed\n");
	}
	else
	{
		DBG("issue  ..... DELAY_REQ: succedded [timestamp: %s]\n",format_wr_timestamp(ptpPortDS->delayReq_tx_ts));
		ptpPortDS->sentDelayReqSequenceId++;
		// ptpPortDS->pending_tx_ts = TRUE;
		// ptpPortDS->pending_DelayReq_tx_ts = TRUE;
	}
}

#ifndef WR_MODE_ONLY

/*Pack and send on event multicast ip adress a PDelayReq message*/
void issuePDelayReq(RunTimeOpts *rtOpts,PtpPortDS *ptpPortDS)
{

	Timestamp originTimestamp;

	memset(&originTimestamp, 0, sizeof(originTimestamp));
	msgPackPDelayReq(ptpPortDS->msgObuf,&originTimestamp,ptpPortDS);

	if (!netSendPeerEvent(ptpPortDS->msgObuf,PDELAY_REQ_LENGTH,&ptpPortDS->netPath,&ptpPortDS->pDelayReq_tx_ts))
	{
		toState(PTP_FAULTY,rtOpts,ptpPortDS);
// ptpPortDS->pending_PDelayReq_tx_ts = FALSE;
		DBGV("PdelayReq message can't be sent -> FAULTY state \n");
		DBG("issue  ..... PDELAY_REQ, failed\n");
	}
	else
	{
		DBG("issue  ..... PDELAY_REQ, succedded \n");
		DBGV("PDelayReq MSG sent ! \n");
		/*	ptpPortDS->pending_tx_ts = TRUE;
		ptpPortDS->pending_PDelayReq_tx_ts = TRUE; */
		ptpPortDS->sentPDelayReqSequenceId++;
	}
}

/*Pack and send on event multicast ip adress a PDelayResp message*/
void issuePDelayResp(TimeInternal *time,MsgHeader *header,RunTimeOpts *rtOpts,PtpPortDS *ptpPortDS)
{
	Timestamp requestReceiptTimestamp;
	
	fromInternalTime(time,&requestReceiptTimestamp);
	msgPackPDelayResp(ptpPortDS->msgObuf,header,&requestReceiptTimestamp,ptpPortDS);

	if (!netSendPeerEvent(ptpPortDS->msgObuf,PDELAY_RESP_LENGTH,&ptpPortDS->netPath,&ptpPortDS->pDelayResp_tx_ts))
	{
		toState(PTP_FAULTY,rtOpts,ptpPortDS);
		/*	ptpPortDS->pending_PDelayResp_tx_ts = FALSE; */
		DBGV("PdelayResp message can't be sent -> FAULTY state \n");
		DBG("issue  ..... PDELAY_RESP, failed\n");
	}
	else
	{
		/* ptpPortDS->pending_tx_ts = TRUE;
		ptpPortDS->pending_PDelayResp_tx_ts = TRUE; */
		DBG("issue  ..... PDELAY_RESP, succedded [sending PDelayReq receive time: sec = %lld nanosec = %lld]\n",\
		(unsigned long long)ptpPortDS->pdelay_req_receive_time.seconds,\
		(unsigned long long)ptpPortDS->pdelay_req_receive_time.nanoseconds);
	}
}

#endif


/*Pack and send on event multicast ip adress a DelayResp message*/
void issueDelayResp(MsgHeader *header,RunTimeOpts *rtOpts,PtpPortDS *ptpPortDS)
{
	msgPackDelayResp(ptpPortDS->msgObuf,header,ptpPortDS);

	if (!netSendGeneral(ptpPortDS->msgObuf,PDELAY_RESP_LENGTH,&ptpPortDS->netPath))
	{
		toState(PTP_FAULTY,rtOpts,ptpPortDS);
		DBGV("delayResp message can't be sent -> FAULTY state \n");
		DBG("issue  ..... DELAY_RESP, failed\n");
	}
	else
	{
		DBG("issue  ..... DELAY_RESP, succedded [sending DelayReq receive time]: sec = %lld nanosec = %lld]\n", \
		    (unsigned long long)ptpPortDS->delay_req_receive_time.seconds, \
		    (unsigned long long)ptpPortDS->delay_req_receive_time.nanoseconds);
	}
}


#ifndef WR_MODE_ONLY
void issuePDelayRespFollowUp(TimeInternal *time,MsgHeader *header,RunTimeOpts *rtOpts,PtpPortDS *ptpPortDS)
{


	Timestamp responseOriginTimestamp;
	fromInternalTime(time,&responseOriginTimestamp);

	msgPackPDelayRespFollowUp(ptpPortDS->msgObuf,header,&responseOriginTimestamp,ptpPortDS);

	if (!netSendPeerGeneral(ptpPortDS->msgObuf,PDELAY_RESP_FOLLOW_UP_LENGTH,&ptpPortDS->netPath))
	{
		toState(PTP_FAULTY,rtOpts,ptpPortDS);
		DBGV("PdelayRespFollowUp message can't be sent -> FAULTY state \n");
		DBG("issue  ..... PDELAY_RESP_FOLLOW_UP, failed\n");
	}
	else
	{
		DBG("issue  ..... PDELAY_RESP_FOLLOW_UP, succedded [sending time of pDelayResp tx]: \n\t\t sec = %ld \n\t\t  nanosec = %lld\n",\
		(unsigned long long)ptpPortDS->pDelayResp_tx_ts.utc,\
		(unsigned long long)ptpPortDS->pDelayResp_tx_ts.nsec);
	}
}
#endif

#ifndef WRPTPv2
// this function seems to be unsued
void issueManagement(MsgHeader *Header, MsgManagement *manage,RunTimeOpts *rtOpts,PtpPortDS *ptpPortDS)
{
	msgPackWRManagement(ptpPortDS->msgObuf,ptpPortDS, SLAVE_PRESENT);

	DBG("Issuing management NON-WR msg, managementId = 0x%x\n",SLAVE_PRESENT);
	if (!netSendGeneral(ptpPortDS->msgObuf,WR_MANAGEMENT_LENGTH,&ptpPortDS->netPath))
	{
		toState(PTP_FAULTY,rtOpts,ptpPortDS);
		DBGV("Management message can't be sent -> FAULTY state \n");
		DBG("issue: Management Msg, failed\n");
	}
	else
	{
		DBG("issue: Management Msg, succedded\n");
		DBGV("FOllowUp MSG sent ! \n");
	}
}
#endif

#ifdef WRPTPv2
void issueWRSignalingMsg(Enumeration16 wrMessageID,RunTimeOpts *rtOpts,PtpPortDS *ptpPortDS)
{
	UInteger16 len;
	
	len = msgPackWRSignalingMsg(ptpPortDS->msgObuf,ptpPortDS, wrMessageID);

	if (!netSendGeneral(ptpPortDS->msgObuf,len,&ptpPortDS->netPath))
	{
		toState(PTP_FAULTY,rtOpts,ptpPortDS);
		DBGV("Signaling message can't be sent -> FAULTY state \n");
		DBG("issue ...... WR_SIGNALING: failed \n");
		
	}
	else
	{
		switch(wrMessageID)
		{
		case CALIBRATE:
#ifdef WRPTPv2			  
			DBGWRFSM("issue ...... WR_SIGNALING [CALIBRATE], succedded, \
		  \n\t\tcalibrationSendPattern = %32x			\
		  \n\t\tcalPeriod    	       = %32lld us\n\n",	\
			    !ptpPortDS->calibrated,			\
			    (unsigned long long)ptpPortDS->calPeriod);
#else
			DBGWRFSM("issue ...... WR_SIGNALING [CALIBRATE], succedded, \
		  \n\t\tcalibrationSendPattern = %32x			\
		  \n\t\tcalPeriod    	       = %32lld us		\
		  \n\t\tcalibrationPattern     = %s			\
		  \n\t\tcalibrationPatternLen  = %32d bits\n\n",\
			    !ptpPortDS->calibrated,			\
			    (unsigned long long)ptpPortDS->calPeriod, \
			    printf_bits(ptpPortDS->calibrationPattern),	\
			    (unsigned)ptpPortDS->calibrationPatternLen);

#endif
			break;

		case CALIBRATED:
			DBGWRFSM("issue ...... WR_SIGNALINGg [CALIBRATED], succedded, params: \n  \t\tdeltaTx= %16lld \n \t\tdeltaRx= %16lld\n", \
			    ((unsigned long long)ptpPortDS->deltaTx.scaledPicoseconds.msb)<<32 | (unsigned long long)ptpPortDS->deltaTx.scaledPicoseconds.lsb, \
			    ((unsigned long long)ptpPortDS->deltaRx.scaledPicoseconds.msb)<<32 | (unsigned long long)ptpPortDS->deltaRx.scaledPicoseconds.lsb);
			break;
		case SLAVE_PRESENT:
			DBGWRFSM("issue ...... WR_SIGNALING [SLAVE_PRESENT], succedded, len = %d \n",len);
			break;
		case LOCK:
			DBGWRFSM("issue ...... WR_SIGNALING [LOCK], succedded\n");
			break;
		case LOCKED:
			DBGWRFSM("issue ...... WR_SIGNALING [LOCKED], succedded \n");
			break;
		case WR_MODE_ON:
			DBGWRFSM("issue ...... WR_SIGNALING [WR_MODE_ON], succedded \n");
			break;
		default:
			DBGWRFSM("issue ...... WR_SIGNALING [UNKNOWN], failed \n");
			break;
		}
	}
}
#else
/* WR: custom White Rabbit management messages */
void issueWRManagement(Enumeration16 wr_managementId,RunTimeOpts *rtOpts,PtpPortDS *ptpPortDS)
{
	UInteger16 len;

	len = msgPackWRManagement(ptpPortDS->msgObuf,ptpPortDS, wr_managementId);

	if (!netSendGeneral(ptpPortDS->msgObuf,len,&ptpPortDS->netPath))
	{
		toState(PTP_FAULTY,rtOpts,ptpPortDS);
		DBGV("Management message can't be sent -> FAULTY state \n");
		DBG("issue: WR Management Msg, failed \n");
	}
	else
	{
		switch(wr_managementId)
		{
		case CALIBRATE:
			DBG("\n\nissue WR Management msg [CALIBRATE], succedded, \
		  \n\t\tcalibrationSendPattern = %32x			\
		  \n\t\tcalPeriod      	       = %32lld us		\
		  \n\t\tcalibrationPattern     = %s			\
		  \n\t\tcalibrationPatternLen  = %32d bits\n\n",\
			    !ptpPortDS->calibrated,			\
			    (unsigned long long)ptpPortDS->calPeriod, \
			    printf_bits(ptpPortDS->calibrationPattern),	\
			    (unsigned)ptpPortDS->calibrationPatternLen);

			break;

		case CALIBRATED:
			DBG("\n\nissue WR Management msg [CALIBRATED], succedded, params: \n  \t\tdeltaTx= %16lld \n \t\tdeltaRx= %16lld\n\n", \
			    ((unsigned long long)ptpPortDS->deltaTx.scaledPicoseconds.msb)<<32 | (unsigned long long)ptpPortDS->deltaTx.scaledPicoseconds.lsb, \
			    ((unsigned long long)ptpPortDS->deltaRx.scaledPicoseconds.msb)<<32 | (unsigned long long)ptpPortDS->deltaRx.scaledPicoseconds.lsb);
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

#endif /*WRPTPv2*/
void addForeign(Octet *buf,MsgHeader *header,PtpPortDS *ptpPortDS)
{
	int i,j;
	Boolean found = FALSE;

	j = ptpPortDS->foreign_record_best;

	/*Check if Foreign master is already known*/
	for (i=0;i<ptpPortDS->number_foreign_records;i++)
	{
		if (!memcmp(header->sourcePortIdentity.clockIdentity,ptpPortDS->foreign[j].foreignMasterPortIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH)
		    && (header->sourcePortIdentity.portNumber == ptpPortDS->foreign[j].foreignMasterPortIdentity.portNumber))
		{
			/*Foreign Master is already in Foreignmaster data set*/
			ptpPortDS->foreign[j].foreignMasterAnnounceMessages++;
			found = TRUE;
			DBGV("addForeign : AnnounceMessage incremented \n");

			msgUnpackHeader(buf,&ptpPortDS->foreign[j].header);
			msgUnpackAnnounce(buf,&ptpPortDS->foreign[j].announce,&ptpPortDS->foreign[j].header);
			if(ptpPortDS->foreign[j].announce.wr_flags != NON_WR)
				DBG("addForeign .                        message from another White Rabbit node [wr_flag != NON_WR]\n");
			break;
		}

		j = (j+1)%ptpPortDS->number_foreign_records;
	}

	/*New Foreign Master*/
	if (!found)
	{
		if (ptpPortDS->number_foreign_records < ptpPortDS->max_foreign_records)
		{
			ptpPortDS->number_foreign_records++;
		}
		j = ptpPortDS->foreign_record_i;

		/*Copy new foreign master data set from Announce message*/
		memcpy(ptpPortDS->foreign[j].foreignMasterPortIdentity.clockIdentity,header->sourcePortIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH);
		ptpPortDS->foreign[j].foreignMasterPortIdentity.portNumber = header->sourcePortIdentity.portNumber;
		ptpPortDS->foreign[j].foreignMasterAnnounceMessages = 0;

		/*header and announce field of each Foreign Master are usefull to run Best Master Clock Algorithm*/
		msgUnpackHeader(buf,&ptpPortDS->foreign[j].header);
		msgUnpackAnnounce(buf,&ptpPortDS->foreign[j].announce,&ptpPortDS->foreign[j].header);
		if(ptpPortDS->foreign[j].announce.wr_flags != NON_WR)
			DBG("addForeign.. WR_ANNOUNCE message from another White Rabbit node [wr_flag != NON_WR]\n");

		DBGV("New foreign Master added \n");

		ptpPortDS->foreign_record_i = (ptpPortDS->foreign_record_i+1) % ptpPortDS->max_foreign_records;
#ifdef WRPTPv2
		ptpPortDS->foreign[j].receptionPortNumber =  ptpPortDS->portIdentity.portNumber;
		DBG("addForeign..: portIdentity.portNumber=%d\n",ptpPortDS->portIdentity.portNumber);
#endif
		
	}

}
Boolean globalBestForeignMastersUpdate(PtpPortDS *ptpPortDS)
{

	Integer16 i;
	Boolean returnValue = FALSE;
	
	for (i=0; i < ptpPortDS->ptpClockDS->numberPorts; i++)
	{
	    if(ptpPortDS[i].record_update)
	    {
	      ErBest(&ptpPortDS[i].foreign,ptpPortDS);
	      returnValue = TRUE;
	      DBG("GLOBAL UPDATE: updating Erbest on port=%d\n",ptpPortDS[i].portIdentity.portNumber);
	    }
	}
	if(returnValue)
	  EBest(ptpPortDS);
  
	
	return returnValue;

}