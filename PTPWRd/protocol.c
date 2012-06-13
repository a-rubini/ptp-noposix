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
void issueWRSignalingMsg(Enumeration16,RunTimeOpts*,PtpPortDS*);
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


/*
 * loop forever. doState() has a switch for the actions and events to be
 * checked for 'port_state'. the actions and events may or may not change
 * 'port_state' by calling toState(), but once they are done we loop around
 * again and perform the actions required for the new 'port_state'.
 */

void singlePortLoop(RunTimeOpts *rtOpts, PtpPortDS *ptpPortDS, int portIndex)
{
           Boolean went_up = FALSE, went_down = FALSE, link_up;

            link_up = isPortUp(&ptpPortDS->netPath);

//	         PTPD_TRACE(TRACE_STARTUP, ptpPortDS, "8888: lnkstat %d %d\n", link_up, ptpPortDS->linkUP);
  

            if(link_up && !ptpPortDS->linkUP)
                went_up = TRUE;
            else if(!link_up && ptpPortDS->linkUP)
                went_down = TRUE;

            ptpPortDS->linkUP = link_up;

            if(went_up || ptpPortDS->doRestart)
            {
                toState(PTP_INITIALIZING, rtOpts, ptpPortDS);
                if(!doInit(rtOpts, ptpPortDS))
                    PTPD_TRACE(TRACE_ERROR, ptpPortDS,"Port %d failed to doInit()\n",(portIndex+1));

								if(ptpPortDS->wrConfig == WR_S_ONLY)
									clearForeignMasters(ptpPortDS);
								ptpPortDS->doRestart = FALSE;

                PTPD_TRACE(TRACE_STARTUP, ptpPortDS, "Port '%s' went up.\n", ptpPortDS->netPath.ifaceName);
            } else if(went_down) {
    						if(ptpPortDS->wrConfig == WR_S_ONLY)
									clearForeignMasters(ptpPortDS);
		
                PTPD_TRACE(TRACE_STARTUP, ptpPortDS, "Port '%s' went down.\n", ptpPortDS->netPath.ifaceName);
                if(ptpPortDS->wrMode == WR_S_ONLY)
                	wr_servo_reset();
                
            }
            if(link_up)
            {
            
                if(ptpPortDS->portState != PTP_INITIALIZING)
                    doState(rtOpts, ptpPortDS);
                else if(!doInit(rtOpts, ptpPortDS))
                    PTPD_TRACE(TRACE_ERROR, ptpPortDS,"Port %d failed to doInit()\n",(portIndex+1));

                //    return;
            }

 
}

void sharedPortsLoop(PtpPortDS *ptpPortDS)
{
         if(ptpPortDS->ptpClockDS->globalStateDecisionEvent)
        {
        PTPD_TRACE(TRACE_PROTO, ptpPortDS,"update secondary slaves\n");
        /* Do after State Decision Even in all the ports */
        if(globalSecondSlavesUpdate(ptpPortDS) == FALSE)
          PTPD_TRACE(TRACE_PROTO, ptpPortDS,"no secondary slaves\n");
        ptpPortDS->ptpClockDS->globalStateDecisionEvent = FALSE;
        }

        /* Handle Best Master Clock Algorithm globally */
        if(globalBestForeignMastersUpdate(ptpPortDS))
        {
        PTPD_TRACE(TRACE_PROTO, ptpPortDS,"Initiate global State Decision Event\n");
        ptpPortDS->ptpClockDS->globalStateDecisionEvent = TRUE;
        }
        else
        ptpPortDS->ptpClockDS->globalStateDecisionEvent = FALSE;

//        ptpd_handle_wripc();
        checkClockClassValidity(ptpPortDS->ptpClockDS);

}

#ifndef WRPC_EXTRA_SLIM

void multiProtocol(RunTimeOpts *rtOpts, PtpPortDS *ptpPortDS)
{
    int           i;
    PtpPortDS *    cur;

    rtOpts->wrConfig = WR_MODE_AUTO;

    for (i=0; i < rtOpts->portNumber; i++)
    {
        if(!netInit(&ptpPortDS[i].netPath, rtOpts, &ptpPortDS[i]))
        {
            PTPD_TRACE(TRACE_ERROR, NULL,"failed to initialize network\n");
            exit(-1);
        }

        ptpPortDS[i].linkUP = FALSE;
    }

    for(;;)
    {
      for (i=0; i < rtOpts->portNumber; i++)
        singlePortLoop(rtOpts, &ptpPortDS[i], i);

      sharedPortsLoop(ptpPortDS);

      ptpd_handle_wripc();
      usleep(1000);
    }

}

#endif

//////////// this function is obsoleted (we use multiProtocol for single and multiport) //////////
/* 
 * loop forever. doState() has a switch for the actions and events to be
 * checked for 'port_state'. the actions and events may or may not change
 * 'port_state' by calling toState(), but once they are done we loop around
 * again and perform the actions required for the new 'port_state'. 
 */

#ifndef WRPC_EXTRA_SLIM

void protocol(RunTimeOpts *rtOpts, PtpPortDS *ptpPortDS)
{
  PTPD_TRACE(TRACE_PROTO, ptpPortDS,"event POWERUP\n");
  
  ptpPortDS->ptpClockDS->globalStateDecisionEvent = FALSE;
  
  toState(PTP_INITIALIZING, rtOpts, ptpPortDS);

  for(;;)
  {
    ptpd_handle_wripc();

    if(ptpPortDS->portState != PTP_INITIALIZING)
      doState(rtOpts, ptpPortDS);
    else if(!doInit(rtOpts, ptpPortDS))
      return;

    if(ptpPortDS->ptpClockDS->globalStateDecisionEvent) 
    {
      PTPD_TRACE(TRACE_PROTO, ptpPortDS,"update secondary slaves\n");
      /* Do after State Decision Even in all the ports */
      if(globalSecondSlavesUpdate(ptpPortDS) == FALSE)
	PTPD_TRACE(TRACE_PROTO, ptpPortDS,"no secondary slaves\n");
      ptpPortDS->ptpClockDS->globalStateDecisionEvent = FALSE;
    }
    
    /* Handle Best Master Clock Algorithm globally */
    if(globalBestForeignMastersUpdate(ptpPortDS))
    {
      PTPD_TRACE(TRACE_PROTO, ptpPortDS,"Initiate global State Decision Event\n");
      ptpPortDS->ptpClockDS->globalStateDecisionEvent = TRUE;
    }
    else
      ptpPortDS->ptpClockDS->globalStateDecisionEvent = FALSE;
    

    checkClockClassValidity(ptpPortDS->ptpClockDS);
  }
}

#endif

/*
 * perform actions required when leaving 'port_state' and entering 'state'
 */
void toState(UInteger8 state, RunTimeOpts *rtOpts, PtpPortDS *ptpPortDS)
{

  /*
   * kind-of non-pre-emption of WR FSM is
   * implemented by banning change of PTP state
   * if WR state is different then WRS_IDLE.
   */
  if(ptpPortDS->wrPortState !=WRS_IDLE)
      return ;

  ptpPortDS->message_activity = TRUE;

  /**
    leaving state tasks
  **/
  switch(ptpPortDS->portState)
  {
  case PTP_MASTER:
    PTPD_TRACE(TRACE_PROTO, ptpPortDS,"PTP_FSM .... exiting PTP_MASTER\n");
    
    timerStop(&ptpPortDS->timers.sync);
    timerStop(&ptpPortDS->timers.announceInterval);
    timerStop(&ptpPortDS->timers.pdelayReq);

    break;

  case PTP_SLAVE:
    PTPD_TRACE(TRACE_PROTO, ptpPortDS,"PTP_FSM .... exiting PTP_SLAVE\n");
    timerStop(&ptpPortDS->timers.announceReceipt);

    if (rtOpts->E2E_mode)
      timerStop(&ptpPortDS->timers.delayReq);
    else
      timerStop(&ptpPortDS->timers.pdelayReq);

    //White Rabbit
    ptpPortDS->wrMode = NON_WR;
    ptpPortDS->wrModeON = FALSE;
    
//    wr_servo_init(ptpClock); // tomek's chagne
    break;

   case PTP_PASSIVE:
     PTPD_TRACE(TRACE_PROTO, ptpPortDS,"PTP_FSM .... exiting PTP_PASSIVE\n");
     timerStop(&ptpPortDS->timers.pdelayReq);
     timerStop(&ptpPortDS->timers.announceReceipt);
     break;
      
  case PTP_LISTENING:
    PTPD_TRACE(TRACE_PROTO, ptpPortDS,"PTP_FSM .... exiting PTP_LISTENING\n");
    timerStop(&ptpPortDS->timers.announceReceipt);
      
    break;
  case PTP_UNCALIBRATED:

  default:
    break;
  }

  /**
      entering state tasks
  **/

  switch(state)
  {
  case PTP_INITIALIZING:
    PTPD_TRACE(TRACE_PROTO, ptpPortDS,"PTP_FSM .... entering PTP_INITIALIZING\n");
    ptpPortDS->portState = PTP_INITIALIZING;
    break;

  case PTP_FAULTY:
    PTPD_TRACE(TRACE_PROTO, ptpPortDS,"PTP_FSM .... entering PTP_FAULTY\n");
    ptpPortDS->portState = PTP_FAULTY;
    break;

  case PTP_DISABLED:
    PTPD_TRACE(TRACE_PROTO, ptpPortDS,"PTP_FSM .... entering  PTP_DISABLED\n");
    ptpPortDS->portState = PTP_DISABLED;
    break;

  case PTP_LISTENING:
    PTPD_TRACE(TRACE_PROTO, ptpPortDS,"PTP_FSM .... entering  PTP_LISTENING\n");

    timerStart(&ptpPortDS->timers.announceReceipt, 
	       ptpPortDS->announceReceiptTimeout * 1000 * (pow_2(ptpPortDS->logAnnounceInterval)));
    ptpPortDS->portState = PTP_LISTENING;
    break;

   case PTP_MASTER:
    PTPD_TRACE(TRACE_PROTO, ptpPortDS,"PTP_FSM .... entering  PTP_MASTER\n");

    timerStart(&ptpPortDS->timers.sync, 
	       1000 * pow_2(ptpPortDS->logSyncInterval));
    timerStart(&ptpPortDS->timers.announceInterval, 
	       1000 * pow_2(ptpPortDS->logAnnounceInterval));
    timerStart(&ptpPortDS->timers.pdelayReq, 
	       1000 * pow_2(ptpPortDS->logMinPdelayReqInterval));

    ptpPortDS->portState = PTP_MASTER;
    break;


  case PTP_PASSIVE:
    PTPD_TRACE(TRACE_PROTO, ptpPortDS,"PTP_FSM .... entering  PTP_PASSIVE\n");

    timerStart(&ptpPortDS->timers.pdelayReq, 
	      1000 * pow_2(ptpPortDS->logMinPdelayReqInterval));

    timerStart(&ptpPortDS->timers.announceInterval, 
	       ptpPortDS->announceReceiptTimeout * 1000 * pow_2(ptpPortDS->logAnnounceInterval));

    ptpPortDS->portState = PTP_PASSIVE;
    break;

  case PTP_UNCALIBRATED:
    PTPD_TRACE(TRACE_PROTO, ptpPortDS,"PTP_FSM .... entering  PTP_UNCALIBRATED\n");

    /*********** White Rabbit SLAVE*************
     *
     * here we have case of slave which
     * detectes that calibration is needed
     */

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
	PTPD_TRACE(TRACE_PROTO, ptpPortDS,"PTP_FSM .... entering PTP_UNCALIBRATED ( WR_SLAVE )\n");
	break;
      }
    }
    else
    {// one of the ports on the link is not WR-enabled
      ptpPortDS->wrMode = NON_WR;
      /*
       * you are Master_ONLY? go to master state ?
       * this is not allowed transtion
       */
    }
    

    /* Standard PTP, go straight to SLAVE */
    PTPD_TRACE(TRACE_PROTO, ptpPortDS,"PTP_FSM .... entering PTP_SLAVE =>failed to enter PTP_UNCALIBRATED \n");
    
    //TODO: bug 1
    PTPD_TRACE(TRACE_SPECIAL_DBG, ptpPortDS,"Failed to enter PTP_UNCALIBRATED because\n")
    if( ptpPortDS->wrMode != WR_SLAVE)
      PTPD_TRACE(TRACE_SPECIAL_DBG, ptpPortDS,"wrMode != WR_SLAVE\n")
    if(ptpPortDS->wrConfig != WR_S_ONLY && ptpPortDS->wrConfig != WR_M_AND_S)
      PTPD_TRACE(TRACE_SPECIAL_DBG, ptpPortDS,"wrConfig != (WR_S_ONLY || WR_M_AND_S)\n")
    if(ptpPortDS->parentWrConfig != WR_M_ONLY && ptpPortDS->parentWrConfig != WR_M_AND_S)
      PTPD_TRACE(TRACE_SPECIAL_DBG, ptpPortDS,"parentWrConfig != (WR_M_ONLY || WR_M_AND_S)\n")
   
    
    ptpPortDS->portState  = PTP_SLAVE;
   
    break;


  case PTP_SLAVE:
    PTPD_TRACE(TRACE_PROTO, ptpPortDS,"PTP_FSM .... entering PTP_SLAVE\n");
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
    PTPD_TRACE(TRACE_PROTO, ptpPortDS,"PTP_FSM .... entering  unrecognized state\n");
    break;
  }

  if(rtOpts->displayStats)
    displayStats(rtOpts, ptpPortDS);
}




/*
 here WR adds initWRCalibration()
 */
Boolean doInit(RunTimeOpts *rtOpts, PtpPortDS *ptpPortDS)
{


  /* network init */

  /* all port data initialization (PTP + WRPTP)  */
  initDataPort(rtOpts, ptpPortDS);

  /* 
   * attempt autodetection, otherwise the configured setting is forced
   */
  if(ptpPortDS->wrConfig == WR_MODE_AUTO)
    autoDetectPortWrConfig(&ptpPortDS->netPath, ptpPortDS); //TODO (3): handle error
  else
    PTPD_TRACE(TRACE_PROTO, ptpPortDS,"wrConfig .............. FORCED configuration\n")  ;


	/* Slave-only port reinitialized: reset the servo. FIXME: won't work with multiple slave ports. */
	if(ptpPortDS->wrConfig == WR_S_ONLY)
		wr_servo_reset();
  
  /* Create the timers (standard PTP only, the WR ones are created in another function: initWrData) */
  timerInit(&ptpPortDS->timers.sync, "Sync");
  timerInit(&ptpPortDS->timers.delayReq, "DelayReq");
  timerInit(&ptpPortDS->timers.pdelayReq, "PDelayReq");
  timerInit(&ptpPortDS->timers.announceReceipt, "AnnReceipt");
  timerInit(&ptpPortDS->timers.announceInterval, "AnnInterval");
  

  // servo initializatin
  initClock(rtOpts, ptpPortDS);
  
  // parent data init
  m1(ptpPortDS);
  msgPackHeader(ptpPortDS->msgObuf, ptpPortDS);

  toState(PTP_LISTENING, rtOpts, ptpPortDS);

  return TRUE;
}

/*
 * handle actions and events for 'port_state'
 */
void doState(RunTimeOpts *rtOpts, PtpPortDS *ptpPortDS)
{
	UInteger8 state;
	//Boolean linkUP;



	switch(ptpPortDS->portState)
	{
	case PTP_LISTENING:
	case PTP_PASSIVE:
	case PTP_SLAVE:
	case PTP_MASTER:
		/*State decision Event*/
		/*kind-of-non-WRFSM-preemption implementation*/
		
		// it is executed globally for all ports in the same for()
		if(ptpPortDS->ptpClockDS->globalStateDecisionEvent == TRUE && ptpPortDS->wrPortState == WRS_IDLE)		  
		{
			
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
				    ptpPortDS->portState == PTP_UNCALIBRATED )
				    )
				{
				  
					/* implementation of PTP_UNCALIBRATED state
					* as transcient state between sth and SLAVE
					* as specified in PTP state machine: Figure 23, 78p
					*/

					/* Candidate for WR Slave */
					ptpPortDS->wrMode  = WR_SLAVE;
					
					/* 
					 * If a source of reference (master) is disconnected,
					 * it might happen that a WR Master is now to be WR Slave.
					 * In such case, we need to disable WR (wrModeON=FALSE) so
					 * the WR Link Setup is entered (condition when entering
					 * UNCALIBRATED state)
					 */
					ptpPortDS->wrModeON = FALSE; 
					PTPD_TRACE(TRACE_PROTO, ptpPortDS,"recommended state = PTP_SLAVE, current state = PTP_MASTER\n");
					PTPD_TRACE(TRACE_PROTO, ptpPortDS,"recommended wrMode = WR_SLAVE\n");
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
				  /*makes sense only if link is up*/
				  if(ptpPortDS->linkUP		     == TRUE &&      \
				     ptpPortDS->portState	     == PTP_SLAVE && \
				     ptpPortDS->wrMode               == WR_SLAVE  && \
		   		    (ptpPortDS->parentWrModeON       == FALSE     || \
		   		     ptpPortDS->wrModeON             == FALSE     ))
		 		  {
				      PTPD_TRACE( (TRACE_PROTO|TRACE_WRPC), ptpPortDS,"event SYNCHRONIZATION_FAULT : go to UNCALIBRATED\n");
				      if(ptpPortDS->parentWrModeON  == FALSE)
					PTPD_TRACE( (TRACE_PROTO|TRACE_WRPC), ptpPortDS,"parent node left White Rabbit Mode- WR Master-forced\n");
					
				      if(ptpPortDS->wrModeON             == FALSE)
					PTPD_TRACE( (TRACE_PROTO|TRACE_WRPC), ptpPortDS,"this node left White Rabbit Mode - WR Slave-forced\n");
					
					PTPD_TRACE( (TRACE_PROTO|TRACE_WRPC), ptpPortDS,"re-synchronization\n");
				      
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


		toState(PTP_INITIALIZING, rtOpts, ptpPortDS);
		return;

	case PTP_UNCALIBRATED:
		

		/* Execute WR protocol state machine */
		
		if(ptpPortDS->wrMode == WR_SLAVE)
			/* handling messages inside: handle()*/
			doWRState(rtOpts, ptpPortDS);
		else
			toState(PTP_SLAVE, rtOpts, ptpPortDS);
		
		ptpPortDS->msgTmpWrMessageID = NULL_WR_TLV;
		break;

	case PTP_LISTENING:
        if(ptpPortDS->wrConfig == WR_M_ONLY)
        {
            PTPD_TRACE(TRACE_PROTO, ptpPortDS,"Port is set to WR-master-only mode. Going to PTP_MASTER state immediately.\n");
            m1(ptpPortDS);
			toState(PTP_MASTER, rtOpts, ptpPortDS);
			break;
        }
	case PTP_PASSIVE:
	case PTP_SLAVE:

		handle(rtOpts, ptpPortDS);

		if(timerExpired(&ptpPortDS->timers.announceReceipt) )
		{
			
			PTPD_TRACE(TRACE_PROTO, ptpPortDS,"ANNOUNCE_RECEIPT_INTERVAL_TIMEOUT_EXPIRED- "
			"in effect exiting PTP_SLAVE state\n");
			ptpPortDS->number_foreign_records = 0;
			ptpPortDS->foreign_record_i = 0;
			
//			clearForeignMasters(ptpPortDS);	TODO(1): check why it's not working with this, investigate the standard

			ptpPortDS->wrMode = NON_WR;
			ptpPortDS->wrModeON = FALSE;

			if((! (ptpPortDS->wrConfig == WR_S_ONLY)) && ptpPortDS->ptpClockDS->clockQuality.clockClass != 255  )
			{
				m1(ptpPortDS);
				toState(PTP_MASTER, rtOpts, ptpPortDS);
			}
			else if (ptpPortDS->portState != PTP_LISTENING) //???
				toState(PTP_LISTENING, rtOpts, ptpPortDS);
		}

		if (rtOpts->E2E_mode)
		{
			if(timerExpired(&ptpPortDS->timers.delayReq))
			{
				PTPD_TRACE(TRACE_PROTO, ptpPortDS,"DELAYREQ_INTERVAL_TIMEOUT_EXPIRED\n");
				issueDelayReq(rtOpts,ptpPortDS);
			}
		}
		else
		{
			if(timerExpired(&ptpPortDS->timers.pdelayReq))
			{
				PTPD_TRACE(TRACE_PROTO, ptpPortDS,"PDELAYREQ_INTERVAL_TIMEOUT_EXPIRED\n");
				issuePDelayReq(rtOpts,ptpPortDS);
			}
		}
		break;

	case PTP_MASTER:

		if(ptpPortDS->wrMode == WR_MASTER  && ptpPortDS->wrPortState != WRS_IDLE)
		{
			/* handling messages inside: handle()*/
			doWRState(rtOpts, ptpPortDS);
			ptpPortDS->msgTmpWrMessageID = NULL_WR_TLV;
			break;
		}
		else
			handle(rtOpts, ptpPortDS);

		if(timerExpired(&ptpPortDS->timers.sync))
		{

			 
			issueSync(rtOpts, ptpPortDS);
			if(ptpPortDS->synch_tx_ts.correct)
				issueFollowup(rtOpts,ptpPortDS);
		}

		if(timerExpired(&ptpPortDS->timers.announceInterval))
		{

			
			issueAnnounce(rtOpts, ptpPortDS);
		}

		if (!rtOpts->E2E_mode)
		{
			if(timerExpired(&ptpPortDS->timers.pdelayReq))
			{
				PTPD_TRACE(TRACE_PROTO, ptpPortDS,"event PDELAYREQ_INTERVAL_TIMEOUT_EXPIRES\n");
				issuePDelayReq(rtOpts,ptpPortDS);
			}
		}

		if(ptpPortDS->wrConfig == WR_S_ONLY || ptpPortDS->ptpClockDS->clockQuality.clockClass == 255)
			toState(PTP_LISTENING, rtOpts, ptpPortDS);

		break;

	case PTP_DISABLED:

		handle(rtOpts, ptpPortDS);
		break;

	default:
		break;
	}
}


/*
 * check and handle received messages
 */
void handle(RunTimeOpts *rtOpts, PtpPortDS *ptpPortDS)
{

	ssize_t length;
	Boolean isFromSelf;
	TimeInternal time = { 0, 0 };

	/* In White Rabbit event and general message are received in the same
	* way, no difference, any of the functions (netRecvEvent and netRecvGeneral
	* can receive any event, need to clean things later */

	length = netRecvMsg(ptpPortDS->msgIbuf, &ptpPortDS->netPath, &ptpPortDS->current_rx_ts);

	if(length < 0)
	{
		PTPD_TRACE(TRACE_ERROR, NULL,"Failed to receive on the event socket, len = %d\n", (int)length);
		PTPD_TRACE(TRACE_PROTO, ptpPortDS,"Failed to receive on the event socket, len = %d\n", (int)length);
		toState(PTP_FAULTY, rtOpts, ptpPortDS);
		return;
	}
	else if(!length) /* nothing received, just exit */
	{
		return;
	}

	ptpPortDS->message_activity = TRUE;

	if(length < HEADER_LENGTH)
	{
		PTPD_TRACE(TRACE_ERROR, NULL,"Message shorter than header length\n");
		PTPD_TRACE(TRACE_PROTO, ptpPortDS,"Message shorter than header length\n");
		toState(PTP_FAULTY, rtOpts, ptpPortDS);
		return;
	}


	// !!!!!!!!!!!!!!!!!
	/*
	* WR: to comply with old PTP daemon work flow
	* this should go to netRecvMsg
	*/
	time.seconds = ptpPortDS->current_rx_ts.sec;
	time.nanoseconds = ptpPortDS->current_rx_ts.nsec;
	// !!!!!!!!!!!!!!!!!

	msgUnpackHeader(ptpPortDS->msgIbuf, &ptpPortDS->msgTmpHeader);

	if(ptpPortDS->msgTmpHeader.versionPTP != ptpPortDS->versionNumber)
	{
		PTPD_TRACE(TRACE_PROTO, ptpPortDS,"Ignored version %d message\n", ptpPortDS->msgTmpHeader.versionPTP);
		return;
	}

	if(ptpPortDS->msgTmpHeader.domainNumber != ptpPortDS->ptpClockDS->domainNumber)
	{
		PTPD_TRACE(TRACE_PROTO, ptpPortDS,"Ignored message from domainNumber %d\n", ptpPortDS->msgTmpHeader.domainNumber);
		return;
	}

	/*Spec 9.5.2.2*/

	isFromSelf = (ptpPortDS->portIdentity.portNumber == ptpPortDS->msgTmpHeader.sourcePortIdentity.portNumber
		      && !memcmp(ptpPortDS->msgTmpHeader.sourcePortIdentity.clockIdentity, ptpPortDS->portIdentity.clockIdentity, CLOCK_IDENTITY_LENGTH));

	/* subtract the inbound latency adjustment if it is not a loop back and the
	time stamp seems reasonable */
  
	if(!isFromSelf && time.seconds > 0)
		subTime(&time, &time, &rtOpts->inboundLatency);

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
		PTPD_TRACE(TRACE_PROTO, ptpPortDS,"handle ..... DELAY_REQ\n");
		handleDelayReq(&ptpPortDS->msgTmpHeader, ptpPortDS->msgIbuf, length, isFromSelf, rtOpts, ptpPortDS);
		break;

#ifndef WR_MODE_ONLY
	case PDELAY_REQ:
		handlePDelayReq(&ptpPortDS->msgTmpHeader, ptpPortDS->msgIbuf, length, &time, isFromSelf, rtOpts, ptpPortDS);
		break;

	case PDELAY_RESP:
		PTPD_TRACE(TRACE_PROTO, ptpPortDS,"handle ..... PDELAY_RESP\n");
		handlePDelayResp(&ptpPortDS->msgTmpHeader, ptpPortDS->msgIbuf,&time, length, isFromSelf, rtOpts, ptpPortDS);
		break;

	case PDELAY_RESP_FOLLOW_UP:
		PTPD_TRACE(TRACE_PROTO, ptpPortDS,"handle ..... PDELAY_RESP_FOLLOW_UP\n");
		handlePDelayRespFollowUp(&ptpPortDS->msgTmpHeader, ptpPortDS->msgIbuf, length, isFromSelf, rtOpts, ptpPortDS);
		break;
#endif

	case DELAY_RESP:
		handleDelayResp(&ptpPortDS->msgTmpHeader, ptpPortDS->msgIbuf, length, isFromSelf, rtOpts, ptpPortDS);
		break;


	case MANAGEMENT:
		PTPD_TRACE(TRACE_PROTO, ptpPortDS,"MANAGEMENT\n");
		handleManagement(&ptpPortDS->msgTmpHeader, ptpPortDS->msgIbuf, length, isFromSelf, rtOpts, ptpPortDS);
		break;

	case SIGNALING:
		
		handleSignaling(&ptpPortDS->msgTmpHeader, ptpPortDS->msgIbuf, length, isFromSelf, rtOpts, ptpPortDS);
		break;

	default:
		break;
	}
}

Boolean msgIsFromCurrentParent(MsgHeader *header, PtpPortDS *ptpPortDS)
{
/*	PTPD_TRACE(TRACE_BMC, ptpPortDS," msgIsFromCurrentParent: parent = %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n",
      ptpPortDS->ptpClockDS->parentPortIdentity.clockIdentity[0], ptpPortDS->ptpClockDS->parentPortIdentity.clockIdentity[1],
      ptpPortDS->ptpClockDS->parentPortIdentity.clockIdentity[2], ptpPortDS->ptpClockDS->parentPortIdentity.clockIdentity[3],
      ptpPortDS->ptpClockDS->parentPortIdentity.clockIdentity[4], ptpPortDS->ptpClockDS->parentPortIdentity.clockIdentity[5],
      ptpPortDS->ptpClockDS->parentPortIdentity.clockIdentity[6], ptpPortDS->ptpClockDS->parentPortIdentity.clockIdentity[7]);

	PTPD_TRACE(TRACE_BMC, ptpPortDS," msgIsFromCurrentParent: header = %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n",
      header->sourcePortIdentity.clockIdentity[0], header->sourcePortIdentity.clockIdentity[1],
      header->sourcePortIdentity.clockIdentity[2], header->sourcePortIdentity.clockIdentity[3],
      header->sourcePortIdentity.clockIdentity[4], header->sourcePortIdentity.clockIdentity[5],
      header->sourcePortIdentity.clockIdentity[6], header->sourcePortIdentity.clockIdentity[7]);
*/
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
		PTPD_TRACE(TRACE_ERROR, NULL,"Too short Announce message\n");
		toState(PTP_FAULTY, rtOpts, ptpPortDS);
	   	return;
	}

	if(length > ANNOUNCE_LENGTH)
		PTPD_TRACE(TRACE_PROTO, ptpPortDS,"handle ..... ANNOUNCE (WR ?): perhaps a message from another White Rabbit node\n")
	else
		PTPD_TRACE(TRACE_PROTO, ptpPortDS,"handle ..... ANNOUNCE: standard PTP\n")

	switch(ptpPortDS->portState )
	{
	case PTP_INITIALIZING:
	case PTP_FAULTY:
	case PTP_DISABLED:
		return;

	case PTP_UNCALIBRATED:

	case PTP_SLAVE:

		if (isFromSelf)
		{
			return;
		}

		/* Valid announce message has been received : BMC algorithm will be executed */
		ptpPortDS->record_update = TRUE; 

		if(msgIsFromCurrentParent(header, ptpPortDS))
		{
			PTPD_TRACE(TRACE_PROTO, ptpPortDS,"handle ..... WR_ANNOUNCE:  msg is from current parent\n");
			msgUnpackAnnounce(ptpPortDS->msgIbuf,&ptpPortDS->msgTmp.announce,header);

			if(ptpPortDS->msgTmp.announce.wr_flags != NON_WR)
				PTPD_TRACE(TRACE_PROTO, ptpPortDS,"handle ..... WR_ANNOUNCE:  message from another White Rabbit node [wr_flag != NON_WR]\n");
			
			/*******  bug fix ???? *****
			* the problem was that we update directly the data in portDS but later
			* we executed BMC which uses data of foreignMasters, this was not updated,
			* so, if there was change of date received from the parent, it was ignored.
			* Therefore, now we update the best foreignMaster (which is the parent) and
			* then let the BMC do the job of updating portDS
			*/
			msgUnpackHeader(ptpPortDS->msgIbuf,&ptpPortDS->foreign[ptpPortDS->foreign_record_best].header);
			msgUnpackAnnounce(ptpPortDS->msgIbuf,&ptpPortDS->foreign[ptpPortDS->foreign_record_best].announce,&ptpPortDS->foreign[ptpPortDS->foreign_record_best].header);
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
			PTPD_TRACE(TRACE_PROTO, ptpPortDS,"handle ..... ANNOUNCE:  Ignore message from self \n");
			return;
		}

//		mprintf("AddForeign!\n");
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
		PTPD_TRACE(TRACE_ERROR, NULL,"Too short Sync message\n");
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
				
				return;
			}

		case PTP_SLAVE:
			if (isFromSelf)
			{
				return;
			}

			if (msgIsFromCurrentParent(header, ptpPortDS))
			{
				/*
				* WR: HW timestamps
				*/
				ptpPortDS->sync_receive_time.seconds = ptpPortDS->current_rx_ts.sec;
				ptpPortDS->sync_receive_time.nanoseconds = ptpPortDS->current_rx_ts.nsec;
				ptpPortDS->sync_receive_time.phase = ptpPortDS->current_rx_ts.phase;
				ptpPortDS->sync_receive_time.correct = ptpPortDS->current_rx_ts.correct;

				if ((header->flagField[0] & 0x02) == TWO_STEP_FLAG)
				{
					PTPD_TRACE(TRACE_PROTO, ptpPortDS,"handle ..... SYNC    : two step clock mode\n");
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
					PTPD_TRACE(TRACE_ERROR, NULL,"BAD !!!!!!!! we don't use this; handle Sync msg, one step clock\n");
					msgUnpackSync(ptpPortDS->msgIbuf,&ptpPortDS->msgTmp.sync);
					integer64_to_internalTime(ptpPortDS->msgTmpHeader.correctionfield,&correctionField);

#ifndef WRPC_EXTRA_SLIM
					timeInternal_display(&correctionField);
#endif

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

		  break;
	}
}


void handleFollowUp(MsgHeader *header, Octet *msgIbuf, ssize_t length, Boolean isFromSelf, RunTimeOpts *rtOpts, PtpPortDS *ptpPortDS)
{
	TimeInternal preciseOriginTimestamp;
	TimeInternal correctionField;
	
	
	if(length < FOLLOW_UP_LENGTH)
	{
		PTPD_TRACE(TRACE_ERROR, NULL,"Too short FollowUp message\n");
		toState(PTP_FAULTY, rtOpts, ptpPortDS);
		return;
	}

	if (isFromSelf)
	{
		return;
	}

	switch(ptpPortDS->portState )
	{
		case PTP_INITIALIZING:
		case PTP_FAULTY:
		case PTP_DISABLED:
		case PTP_LISTENING:

			return;

		case PTP_UNCALIBRATED:

		  /*White Rabbit */
		  if(ptpPortDS->wrMode != NON_WR)
		  {
		    return;
		  }

		case PTP_SLAVE:


		if (msgIsFromCurrentParent(header, ptpPortDS))
		{
			if (ptpPortDS->waitingForFollow)
			{
				if ((ptpPortDS->recvSyncSequenceId == header->sequenceId))
				{

					msgUnpackFollowUp(ptpPortDS->msgIbuf,&ptpPortDS->msgTmp.follow);

					PTPD_TRACE(TRACE_PROTO, ptpPortDS,"handle ..... FOLLOW_UP:  , succedded [sec.msb = %ld sec.lsb = %lld nanosec = %lld]\n", \
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
				else PTPD_TRACE(TRACE_PROTO, ptpPortDS,"handle ..... FOLLOW_UP:, SequenceID doesn't match with last Sync message \n");
				

			}
			else PTPD_TRACE(TRACE_PROTO, ptpPortDS,"handle ..... FOLLOW_UP:, Slave was not waiting a follow up message \n");
			
		}
		else PTPD_TRACE(TRACE_PROTO, ptpPortDS,"handle ..... FOLLOW_UP:, Follow up message is not from current parent \n");
		

		case PTP_MASTER:
			
			break;

		default:
		PTPD_TRACE(TRACE_PROTO, ptpPortDS,"handle ..... FOLLOW_UP: do unrecognized state\n");
		break;
	}//Switch on (port_state)

}


static Integer32 phase_to_cf_units(Integer32 phase)
{
  return (Integer32) ((int64_t)phase * 65536LL / 1000LL); 
}

void handleDelayReq(MsgHeader *header,Octet *msgIbuf,ssize_t length,Boolean isFromSelf,RunTimeOpts *rtOpts,PtpPortDS *ptpPortDS)
{
	if (!rtOpts->E2E_mode)
	{
		return;
	}


	if(length < DELAY_REQ_LENGTH)
	{
		PTPD_TRACE(TRACE_ERROR, NULL,"short DelayReq message\n");
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
		return;

	case PTP_SLAVE:

		if (isFromSelf)
		{
			break;
		}

		break;

	case PTP_MASTER:

		PTPD_TRACE(TRACE_PROTO, ptpPortDS,"handle ..... DELAY_REQ:, succedded\n");
		msgUnpackHeader(ptpPortDS->msgIbuf,&ptpPortDS->delayReqHeader);
				
		/* FIXME: do this, but properly */
		ptpPortDS->delayReqHeader.correctionfield.msb = 0;
		ptpPortDS->delayReqHeader.correctionfield.lsb = phase_to_cf_units(ptpPortDS->current_rx_ts.phase);

		issueDelayResp(&ptpPortDS->delayReqHeader,rtOpts,ptpPortDS);
		break;

	default:
		PTPD_TRACE(TRACE_PROTO, ptpPortDS,"handle ..... DELAY_REQdo unrecognized state\n");
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
	

	if(length < DELAY_RESP_LENGTH)
	{
		PTPD_TRACE(TRACE_ERROR, NULL,"Too short DelayResp message\n");
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

		
		return;

	case PTP_SLAVE:


		msgUnpackDelayResp(ptpPortDS->msgIbuf,&ptpPortDS->msgTmp.resp);

		PTPD_TRACE(TRACE_PROTO, ptpPortDS,"handle ..... DELAY_RESP, succedded: [sec.msb = %ld sec.lsb = %lld nanosec = %lld]\n", \
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
			ptpPortDS->delay_req_receive_time.correct = requestReceiptTimestamp.correct;

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


	 if(length < PDELAY_REQ_LENGTH)
	 {
	PTPD_TRACE(TRACE_ERROR, NULL,"short PDelayReq message\n");
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
			PTPD_TRACE(TRACE_PROTO, ptpPortDS,"handle PDelayReq msg, succedded\n");
			msgUnpackHeader(ptpPortDS->msgIbuf,&ptpPortDS->PdelayReqHeader);
			issuePDelayResp(time,header,rtOpts,ptpPortDS);
			break;
		}

		default:
		break;
	}
   }

  else //(End to End mode..)
  {
  }
}

void handlePDelayResp(MsgHeader *header, Octet *msgIbuf, TimeInternal *time,ssize_t length, Boolean isFromSelf, RunTimeOpts *rtOpts, PtpPortDS *ptpPortDS)
{

if (!rtOpts->E2E_mode)
 {
	Boolean isFromCurrentParent = FALSE;
	TimeInternal requestReceiptTimestamp;
	TimeInternal correctionField;


	 if(length < PDELAY_RESP_LENGTH)
	 {
	PTPD_TRACE(TRACE_ERROR, NULL,"short PDelayResp message\n");
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

			return;

		case PTP_SLAVE:

		if (isFromSelf)
		{
		 addTime(time,time,&rtOpts->outboundLatency);
		 issuePDelayRespFollowUp(time,&ptpPortDS->PdelayReqHeader,rtOpts,ptpPortDS);
		 break;
		}

			msgUnpackPDelayResp(ptpPortDS->msgIbuf,&ptpPortDS->msgTmp.presp);

			PTPD_TRACE(TRACE_PROTO, ptpPortDS,"handle PDelayResp msg, succedded [SLAVE sec.msb = %ld sec.lsb = %lld nanosec = %lld]\n",\
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

					PTPD_TRACE(TRACE_PROTO, ptpPortDS,"\n\n\ntime[two steps]: ptpPortDS->pdelay_resp_receive_time.seconds     = %d\n",time->seconds);
					PTPD_TRACE(TRACE_PROTO, ptpPortDS,"\n\n\ntime[two steps]: ptpPortDS->pdelay_resp_receive_time.nanoseconds = %d\n\n\n",time->nanoseconds);

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

					PTPD_TRACE(TRACE_PROTO, ptpPortDS,"\n\n\ntime[one step]: ptpPortDS->pdelay_resp_receive_time.seconds     = %d\n",time->seconds);
					PTPD_TRACE(TRACE_PROTO, ptpPortDS,"\n\n\ntime[one step]: ptpPortDS->pdelay_resp_receive_time.nanoseconds = %d\n\n\n",time->nanoseconds);

					integer64_to_internalTime(header->correctionfield,&correctionField);



					break;
				}

			}
			else
			{
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

				PTPD_TRACE(TRACE_PROTO, ptpPortDS,"handle PDelayResp msg, succedded [MASTER: sec.msb = %ld sec.lsb = %lld nanosec = %lld]\n",\
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


						//						updatePeerDelay (&ptpPortDS->owd_filt,rtOpts,ptpPortDS,&correctionField,FALSE);


						break;
					}

				}
		default:
		break;
		}
 }

else //(End to End mode..)
 {
 }

}

void handlePDelayRespFollowUp(MsgHeader *header, Octet *msgIbuf, ssize_t length, Boolean isFromSelf, RunTimeOpts *rtOpts, PtpPortDS *ptpPortDS){

if (!rtOpts->E2E_mode)
 {
	TimeInternal responseOriginTimestamp;
	TimeInternal correctionField;


	 if(length < PDELAY_RESP_FOLLOW_UP_LENGTH)
	 {
	PTPD_TRACE(TRACE_ERROR, NULL,"short PDelayRespfollowup message\n");
	toState(PTP_FAULTY, rtOpts, ptpPortDS);
	return;
	 }

	switch(ptpPortDS->portState )
	{
		case PTP_INITIALIZING:
		case PTP_FAULTY:
		case PTP_DISABLED:
		case PTP_UNCALIBRATED:
			return;

		case PTP_SLAVE:

		if (header->sequenceId == ptpPortDS->sentPDelayReqSequenceId-1)
		{
			msgUnpackPDelayRespFollowUp(ptpPortDS->msgIbuf,&ptpPortDS->msgTmp.prespfollow);

			PTPD_TRACE(TRACE_PROTO, ptpPortDS,"handle handlePDelayRespFollowUp msg [MASTER], succedded: \n\t\t sec.msb = %ld \n\t\t sec.lsb = %lld \n\t\t nanosec = %lld\n",\
			(unsigned      long)ptpPortDS->msgTmp.prespfollow.responseOriginTimestamp.secondsField.msb,\
			(unsigned long long)ptpPortDS->msgTmp.prespfollow.responseOriginTimestamp.secondsField.lsb,\
			(unsigned long long)ptpPortDS->msgTmp.prespfollow.responseOriginTimestamp.nanosecondsField);

			toInternalTime(&responseOriginTimestamp,&ptpPortDS->msgTmp.prespfollow.responseOriginTimestamp);
			ptpPortDS->pdelay_resp_send_time.seconds = responseOriginTimestamp.seconds;
			ptpPortDS->pdelay_resp_send_time.nanoseconds = responseOriginTimestamp.nanoseconds;
		    integer64_to_internalTime(ptpPortDS->msgTmpHeader.correctionfield,&correctionField);
			addTime(&correctionField,&correctionField,&ptpPortDS->lastPdelayRespCorrectionField);


			PTPD_TRACE(TRACE_PROTO, ptpPortDS,"\n -------------------------------finish calculation ------------------------\n\n");

			break;
		}

		case PTP_MASTER:

		if (header->sequenceId == ptpPortDS->sentPDelayReqSequenceId-1)
		{
			msgUnpackPDelayRespFollowUp(ptpPortDS->msgIbuf,&ptpPortDS->msgTmp.prespfollow);

			PTPD_TRACE(TRACE_PROTO, ptpPortDS,"handle handlePDelayRespFollowUp msg [MASTER], succedded: \n\t\t sec.msb = %ld \n\t\t sec.lsb = %lld \n\t\t nanosec = %lld\n",\
			(unsigned      long)ptpPortDS->msgTmp.prespfollow.responseOriginTimestamp.secondsField.msb,\
			(unsigned long long)ptpPortDS->msgTmp.prespfollow.responseOriginTimestamp.secondsField.lsb,\
			(unsigned long long)ptpPortDS->msgTmp.prespfollow.responseOriginTimestamp.nanosecondsField);


			toInternalTime(&responseOriginTimestamp,&ptpPortDS->msgTmp.prespfollow.responseOriginTimestamp);
			ptpPortDS->pdelay_resp_send_time.seconds = responseOriginTimestamp.seconds;
			ptpPortDS->pdelay_resp_send_time.nanoseconds = responseOriginTimestamp.nanoseconds;
		    integer64_to_internalTime(ptpPortDS->msgTmpHeader.correctionfield,&correctionField);
			addTime(&correctionField,&correctionField,&ptpPortDS->lastPdelayRespCorrectionField);


			updatePeerDelay (&ptpPortDS->owd_filt,rtOpts,ptpPortDS,&correctionField,TRUE);

			break;
		}

		default:
		  break;
	}

}

else //(End to End mode..)
 {
 }

}

#endif // WR_MODE_ONLY

void handleManagement(MsgHeader *header, Octet *msgIbuf, ssize_t length, Boolean isFromSelf, RunTimeOpts *rtOpts, PtpPortDS *ptpPortDS)
{
	if(isFromSelf)
		return;

	switch(ptpPortDS->msgTmpManagementId)
	{
	default:
		PTPD_TRACE(TRACE_PROTO, ptpPortDS,"\n\nhandle Management msg : no support !!! \n\n");
		break;
	}



}


void handleSignaling(MsgHeader *header, Octet *msgIbuf, ssize_t length, Boolean isFromSelf, RunTimeOpts *rtOpts, PtpPortDS *ptpPortDS) 
{
	MsgSignaling signalingMsg;

	if(isFromSelf)
		return;


	msgUnpackWRSignalingMsg(ptpPortDS->msgIbuf,&signalingMsg,&(ptpPortDS->msgTmpWrMessageID),ptpPortDS);

	switch(ptpPortDS->msgTmpWrMessageID)
	{

	case CALIBRATE:

	PTPD_TRACE(TRACE_PROTO, ptpPortDS,"handle ..... WR_SIGNALING, [CALIBRATE]:	\
	\n\tcalibrateSendPattern  = %32x			\
	\n\tcalPeriod    	  = %32lld us\n",\
		    ptpPortDS->otherNodeCalSendPattern,	  \
		    (unsigned long long)ptpPortDS->otherNodeCalPeriod);
		break;

	case CALIBRATED:

	PTPD_TRACE(TRACE_PROTO, ptpPortDS,"handle ..... WR_SIGNALING [CALIBRATED]: \
	\n\tdeltaTx = %16lld			     \
	\n\tdeltaRx = %16lld\n", 
		    ((unsigned long long)ptpPortDS->otherNodeDeltaTx.scaledPicoseconds.msb)<<32 | (unsigned long long)ptpPortDS->otherNodeDeltaTx.scaledPicoseconds.lsb, \
		    ((unsigned long long)ptpPortDS->otherNodeDeltaRx.scaledPicoseconds.msb)<<32 | (unsigned long long)ptpPortDS->otherNodeDeltaRx.scaledPicoseconds.lsb);
		break;

	case SLAVE_PRESENT:
		PTPD_TRACE(TRACE_PROTO, ptpPortDS,"handle ..... WR_SIGNALING [SLAVE_PRESENT], succedded \n");
		break;
	      
	case LOCK:
		PTPD_TRACE(TRACE_PROTO, ptpPortDS,"handle ..... WR_SIGNALING [LOCK], succedded \n");
		break;

	case LOCKED:

		PTPD_TRACE(TRACE_PROTO, ptpPortDS,"handle ..... WR_SIGNALING [LOCKED], succedded \n");
		break;

	case WR_MODE_ON:

		PTPD_TRACE(TRACE_PROTO, ptpPortDS,"handle ..... WR_SIGNALING [WR_LINK_ON], succedded \n");
		break;

	default:
		PTPD_TRACE(TRACE_PROTO, ptpPortDS,"handle ..... WR_SIGNALING [UNKNOWN], failed \n");
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
	     PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"wrMode <= WR_MASTER\n");
	     ptpPortDS->wrMode = WR_MASTER;
	     ///////////////////////////////////////////
	     toWRState(WRS_M_LOCK, rtOpts, ptpPortDS);
	}

}


/*Pack and send on general multicast ip adress an Announce message*/
void issueAnnounce(RunTimeOpts *rtOpts,PtpPortDS *ptpPortDS)
{
	UInteger16 announce_len;

	msgPackAnnounce(ptpPortDS->msgObuf,ptpPortDS);
	if (ptpPortDS->wrConfig != NON_WR && ptpPortDS->wrConfig != WR_S_ONLY)
		announce_len = WR_ANNOUNCE_LENGTH;
	else
		announce_len = ANNOUNCE_LENGTH;

	if (!netSendGeneral(ptpPortDS->msgObuf,announce_len,&ptpPortDS->netPath))
	{
		toState(PTP_FAULTY,rtOpts,ptpPortDS);
		
		PTPD_TRACE(TRACE_PROTO, ptpPortDS,"issue  ..... ANNOUNCE : Announce Msg, failed \n");
	}
	else
	{
		if (ptpPortDS->wrMode != NON_WR)
		  PTPD_TRACE(TRACE_PROTO, ptpPortDS,"issue  ..... WR ANNOUNCE : succedded \n")
		else
		  PTPD_TRACE(TRACE_PROTO, ptpPortDS,"issue  ..... ANNOUNCE : succedded \n")
		
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
		
		PTPD_TRACE(TRACE_PROTO, ptpPortDS,"issue  ..... SYNC:   failed");
	}
	else
	{
		PTPD_TRACE(TRACE_PROTO, ptpPortDS,"issue  ..... SYNC: succedded [synch timestamp: %s]\n", \
		format_wr_timestamp(ptpPortDS->synch_tx_ts));
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
		
		PTPD_TRACE(TRACE_PROTO, ptpPortDS,"issue  ..... FOLLOW_UP: failed\n");
	}
	else
	{
		PTPD_TRACE(TRACE_PROTO, ptpPortDS,"issue  ..... FOLLOW_UP: succedded [sending time of sync tx: sec = %lld  nanosec = %lld]\n",\
		(unsigned long long)ptpPortDS->synch_tx_ts.sec,\
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
		
		PTPD_TRACE(TRACE_PROTO, ptpPortDS,"issue  ..... DELAY_REQ: failed\n");
	}
	else
	{
		PTPD_TRACE(TRACE_PROTO, ptpPortDS,"issue  ..... DELAY_REQ: succedded [timestamp: %s]\n",format_wr_timestamp(ptpPortDS->delayReq_tx_ts));
		ptpPortDS->sentDelayReqSequenceId++;
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
		
		PTPD_TRACE(TRACE_PROTO, ptpPortDS,"issue  ..... PDELAY_REQ, failed\n");
	}
	else
	{
		PTPD_TRACE(TRACE_PROTO, ptpPortDS,"issue  ..... PDELAY_REQ, succedded \n");
		
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
		
		PTPD_TRACE(TRACE_PROTO, ptpPortDS,"issue  ..... PDELAY_RESP, failed\n");
	}
	else
	{
		PTPD_TRACE(TRACE_PROTO, ptpPortDS,"issue  ..... PDELAY_RESP, succedded [sending PDelayReq receive time: sec = %lld nanosec = %lld]\n",\
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
		
		PTPD_TRACE(TRACE_PROTO, ptpPortDS,"issue  ..... DELAY_RESP, failed\n");
	}
	else
	{
		PTPD_TRACE(TRACE_PROTO, ptpPortDS,"issue  ..... DELAY_RESP, succedded [sending DelayReq receive time]: sec = %lld nanosec = %lld]\n", \
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
		
		PTPD_TRACE(TRACE_PROTO, ptpPortDS,"issue  ..... PDELAY_RESP_FOLLOW_UP, failed\n");
	}
	else
	{
		PTPD_TRACE(TRACE_PROTO, ptpPortDS,"issue  ..... PDELAY_RESP_FOLLOW_UP, succedded [sending time of pDelayResp tx]: \n\t\t sec = %lld \n\t\t  nanosec = %lld\n",\
		(unsigned long long)ptpPortDS->pDelayResp_tx_ts.sec,\
		(unsigned long long)ptpPortDS->pDelayResp_tx_ts.nsec);
	}
}
#endif


void issueWRSignalingMsg(Enumeration16 wrMessageID,RunTimeOpts *rtOpts,PtpPortDS *ptpPortDS)
{
	UInteger16 len;
	
	len = msgPackWRSignalingMsg(ptpPortDS->msgObuf,ptpPortDS, wrMessageID);

	if (!netSendGeneral(ptpPortDS->msgObuf,len,&ptpPortDS->netPath))
	{
		toState(PTP_FAULTY,rtOpts,ptpPortDS);
		
		PTPD_TRACE(TRACE_PROTO, ptpPortDS,"issue ...... WR_SIGNALING: failed \n");
		
	}
	else
	{
		switch(wrMessageID)
		{
		case CALIBRATE:
		  PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"issue ...... WR_SIGNALING [CALIBRATE], succedded, \
		  \n\t\tcalibrationSendPattern = %32x			\
		  \n\t\tcalPeriod    	       = %32lld us\n\n",	\
			    !ptpPortDS->calibrated,			\
			    (unsigned long long)ptpPortDS->calPeriod);
			break;

		case CALIBRATED:
			PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"issue ...... WR_SIGNALINGg [CALIBRATED], succedded, params: \n  \t\tdeltaTx= %16lld \n \t\tdeltaRx= %16lld\n", \
			    ((unsigned long long)ptpPortDS->deltaTx.scaledPicoseconds.msb)<<32 | (unsigned long long)ptpPortDS->deltaTx.scaledPicoseconds.lsb, \
			    ((unsigned long long)ptpPortDS->deltaRx.scaledPicoseconds.msb)<<32 | (unsigned long long)ptpPortDS->deltaRx.scaledPicoseconds.lsb);
			break;
		case SLAVE_PRESENT:
			PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"issue ...... WR_SIGNALING [SLAVE_PRESENT], succedded, len = %d \n",len);
			break;
		case LOCK:
			PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"issue ...... WR_SIGNALING [LOCK], succedded\n");
			break;
		case LOCKED:
			PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"issue ...... WR_SIGNALING [LOCKED], succedded \n");
			break;
		case WR_MODE_ON:
			PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"issue ...... WR_SIGNALING [WR_MODE_ON], succedded \n");
			break;
		default:
			PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"issue ...... WR_SIGNALING [UNKNOWN], failed \n");
			break;
		}
	}
}
void addForeign(Octet *buf,MsgHeader *header,PtpPortDS *ptpPortDS)
{
	int i,j;
	Boolean found = FALSE;

	j = ptpPortDS->foreign_record_best;
	PTPD_TRACE(TRACE_PROTO, ptpPortDS,"addForeign=>number_foreign_records = %d\n",ptpPortDS->number_foreign_records);
	/*Check if Foreign master is already known*/
	for (i=0;i<ptpPortDS->number_foreign_records;i++)
	{
		if (!memcmp(header->sourcePortIdentity.clockIdentity,ptpPortDS->foreign[j].foreignMasterPortIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH)
		    && (header->sourcePortIdentity.portNumber == ptpPortDS->foreign[j].foreignMasterPortIdentity.portNumber))
		{
			PTPD_TRACE(TRACE_PROTO, ptpPortDS,"addForeign=>foreign master already in data set\n");
			/*Foreign Master is already in Foreignmaster data set*/
			ptpPortDS->foreign[j].foreignMasterAnnounceMessages++;
			found = TRUE;

			msgUnpackHeader(buf,&ptpPortDS->foreign[j].header);
			msgUnpackAnnounce(buf,&ptpPortDS->foreign[j].announce,&ptpPortDS->foreign[j].header);
			if(ptpPortDS->foreign[j].announce.wr_flags != NON_WR)
				PTPD_TRACE(TRACE_PROTO, ptpPortDS,"addForeign=> message from another White Rabbit node [wr_flag != NON_WR]\n");
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

		PTPD_TRACE(TRACE_PROTO, ptpPortDS,"addForeign=>brand new foreign master\n");
		/*Copy new foreign master data set from Announce message*/
		memcpy(ptpPortDS->foreign[j].foreignMasterPortIdentity.clockIdentity,header->sourcePortIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH);
		ptpPortDS->foreign[j].foreignMasterPortIdentity.portNumber = header->sourcePortIdentity.portNumber;
		ptpPortDS->foreign[j].foreignMasterAnnounceMessages = 0;

		/*header and announce field of each Foreign Master are usefull to run Best Master Clock Algorithm*/
		msgUnpackHeader(buf,&ptpPortDS->foreign[j].header);
		msgUnpackAnnounce(buf,&ptpPortDS->foreign[j].announce,&ptpPortDS->foreign[j].header);
		if(ptpPortDS->foreign[j].announce.wr_flags != NON_WR)
			PTPD_TRACE(TRACE_PROTO, ptpPortDS,"addForeign.. WR_ANNOUNCE message from another White Rabbit node [wr_flag != NON_WR]\n");


		ptpPortDS->foreign_record_i = (ptpPortDS->foreign_record_i+1) % ptpPortDS->max_foreign_records;
		ptpPortDS->foreign[j].receptionPortNumber =  ptpPortDS->portIdentity.portNumber;
		PTPD_TRACE(TRACE_PROTO, ptpPortDS,"addForeign..: portIdentity.portNumber=%d\n",ptpPortDS->portIdentity.portNumber);
		
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
	      ErBest(ptpPortDS[i].foreign,ptpPortDS);
	      returnValue = TRUE;
	      PTPD_TRACE(TRACE_PROTO, ptpPortDS,"GLOBAL UPDATE: updating Erbest on port=%d\n",ptpPortDS[i].portIdentity.portNumber);
	    }
	}
	if(returnValue)
	  EBest(ptpPortDS);
  
	
	return returnValue;

}
Boolean globalSecondSlavesUpdate(PtpPortDS *ptpPortDS)
{

	Integer16 i;
	Integer16 Ebest;
	
	for (Ebest=0; Ebest < ptpPortDS->ptpClockDS->numberPorts; Ebest++)
	{
		if(ptpPortDS[Ebest].wrSlaveRole == SECONDARY_SLAVE)
			break;
	}
	if(Ebest == ptpPortDS->ptpClockDS->numberPorts)
	  return FALSE; //no secondary slaves
	
	PTPD_TRACE(TRACE_PROTO, ptpPortDS,"secondary Slave Update\n");
	
	for (i= Ebest + 1; i < ptpPortDS->ptpClockDS->numberPorts; i++)
	{
		if(ptpPortDS[i].wrSlaveRole != SECONDARY_SLAVE)
			continue;
		
		if ((bmcDataSetComparison(&ptpPortDS[i].secondaryForeignMaster.header,   	\
					  &ptpPortDS[i].secondaryForeignMaster.announce, 	\
					   ptpPortDS[i].secondaryForeignMaster.receptionPortNumber, 	\
					  &ptpPortDS[Ebest].secondaryForeignMaster.header,	\
					  &ptpPortDS[Ebest].secondaryForeignMaster.announce,	\
					   ptpPortDS[Ebest].secondaryForeignMaster.receptionPortNumber, 	\
					   ptpPortDS)) < 0)
		{
			PTPD_TRACE(TRACE_PROTO, ptpPortDS,"secondary Slave Update:  update currently best (%d) to new best = %d\n",Ebest, i);
			Ebest = i;
		}
	}
	ptpPortDS->ptpClockDS->secondarySlavePortNumber = ptpPortDS[Ebest].portIdentity.portNumber; // Ebest;
	
	ptpPortDS->ptpClockDS->secondBestForeign = &ptpPortDS[Ebest].secondaryForeignMaster;
	
	PTPD_TRACE(TRACE_WR_PROTO, ptpPortDS,"secondary Slave Update: the port with the best secondary master (secondary slave) is %d\n",\
		ptpPortDS->ptpClockDS->secondarySlavePortNumber);

	return TRUE;

}


void clearForeignMasters(PtpPortDS *ptpPortDS)
{
    Integer16 i;
    PTPD_TRACE(TRACE_PROTO, ptpPortDS,"addForeign=>clearing the number_foreign_records\n");
    for (i=0;i<ptpPortDS->number_foreign_records;i++)
      ptpPortDS->foreign[i].receptionPortNumber = 0;// we recognize that ForeignMaster record is empty
						    // by the value of receptionPortNumber - port numbers
						    // starts from 1, 0 value indicates empty record
    
    ptpPortDS->foreign_record_i 	= 0;
    ptpPortDS->number_foreign_records	= 0;
    ptpPortDS->foreign_record_best	= 0;

}

void checkClockClassValidity(PtpClockDS *ptpClockDS)
{

   if(ptpClockDS->clockQuality.clockClass != 6) 
     return;	//check only if we think we are locked to primary source
   
   if(!timerExpired(&ptpClockDS->clockClassValidityTimer)) //timer not expired yet
     return;
   
   if(extsrcLocked() != TRUE)		
      ptpClockDS->clockQuality.clockClass = 7; //table 5, ptp, p55
    

}
