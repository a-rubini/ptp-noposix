/* bmc.c */

#include "ptpd.h"


/* Init ptpClock with run time values (initialization constants are in constants.h)*/
void initData(RunTimeOpts *rtOpts, PtpClock *ptpClock)
{
  int i,j;
  j=0;
  DBG("initData\n");

/* Default data set */
	ptpClock->twoStepFlag = TWO_STEP_FLAG;

	/*init clockIdentity with MAC address and 0xFF and 0xFE. see spec 7.5.2.2.2*/
	for (i=0;i<CLOCK_IDENTITY_LENGTH;i++)
	{

		if (i==3) ptpClock->clockIdentity[i]=0xFF;
		else if (i==4) ptpClock->clockIdentity[i]=0xFE;
		else if (i == 5 && rtOpts->overrideClockIdentity != 0x0)
		  /*
		   * temporary hack to run the daemon on lo (single interface)
		   */
		  ptpClock->clockIdentity[i] = rtOpts->overrideClockIdentity;
		else
		{
		  ptpClock->clockIdentity[i]=ptpClock->port_uuid_field[j];
		  j++;
		}
	}
	ptpClock->numberPorts = rtOpts->portNumber;

	ptpClock->clockQuality.clockAccuracy = rtOpts->clockQuality.clockAccuracy;

	ptpClock->clockQuality.offsetScaledLogVariance = rtOpts->clockQuality.offsetScaledLogVariance;


	/* If priority not defined at the runtime, set it high for the WR master*/
#ifdef WRPTPv2	
	/*
	 * White Rabbit - init static data fields
	 */
	ptpClock->wrConfig			     = rtOpts->wrConfig;
	ptpClock->deltasKnown			     = rtOpts->deltasKnown;
	ptpClock->wrStateTimeout		     = rtOpts->wrStateTimeout;
	ptpClock->wrStateRetry			     = rtOpts->wrStateRetry;
	ptpClock->calPeriod			     = rtOpts->calPeriod;	
	
	ptpClock->knownDeltaTx.scaledPicoseconds.lsb = rtOpts->knownDeltaTx.scaledPicoseconds.lsb;
	ptpClock->knownDeltaTx.scaledPicoseconds.msb = rtOpts->knownDeltaTx.scaledPicoseconds.msb;
	
	ptpClock->knownDeltaRx.scaledPicoseconds.lsb = rtOpts->knownDeltaRx.scaledPicoseconds.lsb;
	ptpClock->knownDeltaRx.scaledPicoseconds.msb = rtOpts->knownDeltaRx.scaledPicoseconds.msb;
	
	ptpClock->primarySlavePortNumber = 0;
	
	/* 
	 * White Rabbit - init dynamic data fields 
	 */
	initWrData(ptpClock);
	
	if(rtOpts->priority1 == DEFAULT_PRIORITY1 && ptpClock->wrConfig != NON_WR)
	  ptpClock->priority1 = WR_PRIORITY1;
#else
	if(rtOpts->priority1 == DEFAULT_PRIORITY1 && ptpClock->wrMode == WR_MASTER)
	  ptpClock->priority1 = WR_MASTER_PRIORITY1;
#endif	  
	  
	else
	  ptpClock->priority1 = rtOpts->priority1;

	ptpClock->priority2 = rtOpts->priority2;

	ptpClock->domainNumber = rtOpts->domainNumber;
	ptpClock->slaveOnly = rtOpts->slaveOnly;
	if(rtOpts->slaveOnly)
           rtOpts->clockQuality.clockClass = 255;


	ptpClock->clockQuality.clockClass = rtOpts->clockQuality.clockClass;

/*Port configuration data set */

	/*PortIdentity Init (portNumber = 1 for an ardinary clock spec 7.5.2.3)*/
	memcpy(ptpClock->portIdentity.clockIdentity,ptpClock->clockIdentity,CLOCK_IDENTITY_LENGTH);

	ptpClock->logMinDelayReqInterval = DEFAULT_DELAYREQ_INTERVAL;
	ptpClock->peerMeanPathDelay.seconds = 0;
	ptpClock->peerMeanPathDelay.nanoseconds = 0;

	ptpClock->logAnnounceInterval = rtOpts->announceInterval;
	ptpClock->announceReceiptTimeout = DEFAULT_ANNOUNCE_RECEIPT_TIMEOUT;
	ptpClock->logSyncInterval = rtOpts->syncInterval;
	ptpClock->delayMechanism = DEFAULT_DELAY_MECHANISM;
	ptpClock->logMinPdelayReqInterval = DEFAULT_PDELAYREQ_INTERVAL;
	ptpClock->versionNumber = VERSION_PTP;

	/*Initialize seed for random number used with Announce Timeout (spec 9.2.6.11)*/
	srand(time(NULL));
	//ptpClock->R = getRand();

	/*Init other stuff*/
	ptpClock->number_foreign_records = 0;
	ptpClock->max_foreign_records = rtOpts->max_foreign_records;
#ifdef WRPTPv2
	if(ptpClock->wrConfig != NON_WR)
#else
	if(ptpClock->wrMode != NON_WR)
#endif	  
	{
	  /* we want White Rabbit daemon, so here we are */



	  /* setting appropriate clock class for WR node
	   * if the class was not set by the user (is default)*/



	  if(rtOpts->clockQuality.clockClass == DEFAULT_CLOCK_CLASS)
	  {
#ifdef WRPTPv2
            // do nothing, we want it to be standard :)
#else
	    if( ptpClock->wrMode == WR_MASTER)
	      ptpClock->clockQuality.clockClass = WR_MASTER_CLOCK_CLASS;
	    else if(ptpClock->wrMode == WR_SLAVE)
	      ptpClock->clockQuality.clockClass = WR_SLAVE_CLOCK_CLASS;
#endif	    
	  }
	  else
	    ptpClock->clockQuality.clockClass = rtOpts->clockQuality.clockClass;



// 	  if(ptpClock->portIdentity.portNumber == 1 )
// 	  {
// 	    /* there can be only one slave (not entirely
// 	     * true for WR, but must be enough for the time being)
// 	     * so the first port is the one which can be WR Slave
// 	     */
// 	    ptpClock->wrMode     		  = rtOpts->wrMode;
// 	  }
// 	  else
// 	  {
// 	    /* All the other ports except port = 1 are
// 	     * WR Masters and no discussion about that
// 	     */
// 	    ptpClock->wrMode                 = WR_MASTER;
// 	  }
	}
	else
	{
	  /* normal PTP daemon on all ports */
	  //ptpClock->wrMode                   = NON_WR;
	}
#ifndef WRPTPv2	
	ptpClock->wrModeON      		 = FALSE;

	/*this one should be set at runtime*/
	ptpClock->calibrated  		 = FALSE;
	ptpClock->deltaTx.scaledPicoseconds.lsb  = 0;
	ptpClock->deltaTx.scaledPicoseconds.msb  = 0;
	ptpClock->deltaRx.scaledPicoseconds.lsb	 = 0;
	ptpClock->deltaRx.scaledPicoseconds.msb	 = 0;
	/**/
#endif
	//ptpClock->tx_tag 		 = 0;
	//ptpClock->new_tx_tag_read   		 = FALSE;

	ptpClock->pending_tx_ts            = FALSE;
	ptpClock->pending_Synch_tx_ts      = 0;
	ptpClock->pending_DelayReq_tx_ts   = 0;
	ptpClock->pending_PDelayReq_tx_ts  = 0;
	ptpClock->pending_PDelayResp_tx_ts = 0;

#ifndef WRPTPv2		
	
# ifdef NEW_SINGLE_WRFSM
	ptpClock->wrPortState = WRS_IDLE;
# else
	ptpClock->wrPortState = PTPWR_IDLE;
# endif

	ptpClock->calPeriod = rtOpts->calPeriod;
	
	
	ptpClock->calibrationPattern = rtOpts->calibrationPattern;
	ptpClock->calibrationPatternLen = rtOpts->calibrationPatternLen;

	//TODO:
	for(i = 0; i < WR_TIMER_ARRAY_SIZE;i++)
	  {
	    ptpClock->wrTimeouts[i] = WR_DEFAULT_STATE_TIMEOUT_MS;
	  }

	// TODO: fixme: locking timeout should be bigger

	ptpClock->wrTimeouts[WRS_S_LOCK]   = 10000;
	ptpClock->wrTimeouts[WRS_S_LOCK_1] = 10000;
	ptpClock->wrTimeouts[WRS_S_LOCK_2] = 10000;
#endif
}

/*Local clock is becoming Master. Table 13 (9.3.5) of the spec.*/
void m1(PtpClock *ptpClock)
{
  DBG("[%s]\n",__func__);
	/*Current data set update*/
	ptpClock->stepsRemoved = 0;
	ptpClock->offsetFromMaster.nanoseconds = 0;
	ptpClock->offsetFromMaster.seconds = 0;
	ptpClock->meanPathDelay.nanoseconds = 0;
	ptpClock->meanPathDelay.seconds = 0;

	/*Parent data set*/
	memcpy(ptpClock->parentPortIdentity.clockIdentity,ptpClock->clockIdentity,CLOCK_IDENTITY_LENGTH);
	ptpClock->parentPortIdentity.portNumber = 0;
	ptpClock->parentStats = DEFAULT_PARENTS_STATS;
	ptpClock->observedParentClockPhaseChangeRate = 0;
	ptpClock->observedParentOffsetScaledLogVariance = 0;
	memcpy(ptpClock->grandmasterIdentity,ptpClock->clockIdentity,CLOCK_IDENTITY_LENGTH);
	ptpClock->grandmasterClockQuality.clockAccuracy = ptpClock->clockQuality.clockAccuracy;
	ptpClock->grandmasterClockQuality.clockClass = ptpClock->clockQuality.clockClass;
	ptpClock->grandmasterClockQuality.offsetScaledLogVariance = ptpClock->clockQuality.offsetScaledLogVariance;
	ptpClock->grandmasterPriority1 = ptpClock->priority1;
	ptpClock->grandmasterPriority2 = ptpClock->priority2;

	/*White Rabbit*/
#ifdef WRPTPv2
	ptpClock->parentWrConfig      	  = ptpClock->wrConfig;
	ptpClock->parentIsWRnode     = (ptpClock->wrConfig != NON_WR) ;
	ptpClock->parentWrModeON     = ptpClock->wrModeON;
	ptpClock->parentCalibrated = ptpClock->calibrated;
#else
	ptpClock->parentWrNodeMode   = ptpClock->wrMode;
	ptpClock->parentIsWRnode     = (ptpClock->wrMode != NON_WR) ;
	ptpClock->parentWrModeON     = ptpClock->wrModeON;
	ptpClock->parentCalibrated = ptpClock->calibrated;
#endif	

	/*Time Properties data set*/
	ptpClock->timeSource = INTERNAL_OSCILLATOR;
		
}


/*Local clock is synchronized to Ebest Table 16 (9.3.5) of the spec*/
void s1(MsgHeader *header,MsgAnnounce *announce,PtpClock *ptpClock)
{
	DBG("[%s]\n",__func__);
	/*Current DS*/
	ptpClock->stepsRemoved = announce->stepsRemoved + 1;

	/*Parent DS*/

	memcpy(ptpClock->parentPortIdentity.clockIdentity,header->sourcePortIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH);
	ptpClock->parentPortIdentity.portNumber = header->sourcePortIdentity.portNumber;
	memcpy(ptpClock->grandmasterIdentity,announce->grandmasterIdentity,CLOCK_IDENTITY_LENGTH);
	ptpClock->grandmasterClockQuality.clockAccuracy = announce->grandmasterClockQuality.clockAccuracy;
	ptpClock->grandmasterClockQuality.clockClass = announce->grandmasterClockQuality.clockClass;
	ptpClock->grandmasterClockQuality.offsetScaledLogVariance = announce->grandmasterClockQuality.offsetScaledLogVariance;
	ptpClock->grandmasterPriority1 = announce->grandmasterPriority1;
	ptpClock->grandmasterPriority2 = announce->grandmasterPriority2;

	/*White Rabbit*/
	ptpClock->parentIsWRnode    	 = ((announce->wr_flags & WR_NODE_MODE) != NON_WR);
	ptpClock->parentWrModeON     	  = ((announce->wr_flags & WR_IS_WR_MODE) == WR_IS_WR_MODE);
	ptpClock->parentCalibrated = ((announce->wr_flags & WR_IS_CALIBRATED) == WR_IS_CALIBRATED);
	DBGBMC(" S1: copying wr_flags.......... 0x%x\n", announce->wr_flags);
	DBGBMC(" S1: parentIsWRnode............ 0x%x\n", ptpClock->parentIsWRnode);
	DBGBMC(" S1: parentWrModeON............ 0x%x\n", ptpClock->parentWrModeON);
	DBGBMC(" S1: parentCalibrated.......... 0x%x\n", ptpClock->parentCalibrated);	
#ifdef WRPTPv2
	ptpClock->parentWrConfig      =   announce->wr_flags & WR_NODE_MODE;
	DBGBMC(" S1: parentWrConfig.......  0x%x\n", ptpClock->parentWrConfig);
#else
	ptpClock->parentWrNodeMode   =   announce->wr_flags & WR_NODE_MODE;
	DBGBMC(" S1: parentWrNodeMode....  0x%x\n",ptpClock->parentWrNodeMode);
#endif

	
	
	/*Timeproperties DS*/
	ptpClock->currentUtcOffset = announce->currentUtcOffset;
	ptpClock->currentUtcOffsetValid = ((header->flagField[1] & 0x04) == 0x04); //"Valid" is bit 2 in second octet of flagfield
	ptpClock->leap59 = ((header->flagField[1] & 0x02) == 0x02);
	ptpClock->leap61 = ((header->flagField[1] & 0x01) == 0x01);
	ptpClock->timeTraceable = ((header->flagField[1] & 0x10) == 0x10);
	ptpClock->frequencyTraceable = ((header->flagField[1] & 0x20) == 0x20);
	ptpClock->ptpTimescale = ((header->flagField[1] & 0x08) == 0x08);
	ptpClock->timeSource = announce->timeSource;
}


/*Copy local data set into header and announce message. 9.3.4 table 12*/
void copyD0(MsgHeader *header, MsgAnnounce *announce, PtpClock *ptpClock)
{
	DBG("[%s]\n",__func__);
  	announce->grandmasterPriority1 = ptpClock->priority1;
	memcpy(announce->grandmasterIdentity,ptpClock->clockIdentity,CLOCK_IDENTITY_LENGTH);
	announce->grandmasterClockQuality.clockClass = ptpClock->clockQuality.clockClass;
	announce->grandmasterClockQuality.clockAccuracy = ptpClock->clockQuality.clockAccuracy;
	announce->grandmasterClockQuality.offsetScaledLogVariance = ptpClock->clockQuality.offsetScaledLogVariance;
	announce->grandmasterPriority2 = ptpClock->priority2;
	announce->stepsRemoved = 0;
	memcpy(header->sourcePortIdentity.clockIdentity,ptpClock->clockIdentity,CLOCK_IDENTITY_LENGTH);

	/*White Rabbit*/
#ifdef WRPTPv2
	announce->wr_flags = (announce->wr_flags | ptpClock->wrConfig) & WR_NODE_MODE  ;
	announce->wr_flags =  announce->wr_flags | ptpClock->calibrated << 2;
	announce->wr_flags =  announce->wr_flags | ptpClock->wrModeON     << 3;

#else
	announce->wr_flags = (announce->wr_flags | ptpClock->wrMode) & WR_NODE_MODE  ;
	announce->wr_flags =  announce->wr_flags | ptpClock->calibrated << 2;
	announce->wr_flags =  announce->wr_flags | ptpClock->wrModeON     << 3;
#endif	
}

char* clockDescription(MsgHeader *header, MsgAnnounce *announce)
{
	char *tmp;
	char wrConfig[10];
	
	if(announce->wr_flags & WR_M_ONLY)
	  strcpy(wrConfig,"WR_M_ONLY\0");
	else if(announce->wr_flags & WR_S_ONLY)
	 strcpy(wrConfig,"WR_S_ONLY\0");
	else if(announce->wr_flags & WR_M_AND_S)
	  strcpy(wrConfig,"WR_M_AND_S\0");
	else
	  strcpy(wrConfig,"NON_WR\0");

 	sprintf(tmp, " [clkId=%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx port=%d wrConfig=%s] ",
 	    header->sourcePortIdentity.clockIdentity[0], header->sourcePortIdentity.clockIdentity[1],
 	    header->sourcePortIdentity.clockIdentity[2], header->sourcePortIdentity.clockIdentity[3],
 	    header->sourcePortIdentity.clockIdentity[4], header->sourcePortIdentity.clockIdentity[5],
	    header->sourcePortIdentity.clockIdentity[6], header->sourcePortIdentity.clockIdentity[7],
 	    header->sourcePortIdentity.portNumber,   
 	    wrConfig
 	    );

	return tmp;

}

/*Data set comparison bewteen two foreign masters (9.3.4 fig 27)
 * return similar to memcmp() */

Integer8 bmcDataSetComparison(MsgHeader *headerA, MsgAnnounce *announceA,
								MsgHeader *headerB,MsgAnnounce *announceB,PtpClock *ptpClock)
{
	/*
	 * This implementation is not precisely as in the standard !@!!!!!
	 * see: page 90, Figure 28
	 * The return (non-error) values should be 4
	 * 1) A bettter then B
	 * 2) B better then A
	 * 3) A better by topology than B
	 * 4) B better by topology then A
	 * 
	 * then, error values
	 * 5) error-1: Receiver=Sender
	 * 6) error-2: A=B
	 *
	 * in this implementation:
	 * (1) = (3)
	 * (2) = (4)
	 * (5) = (6)
	 *
	 * If I'm not mistaken, it will cause problems in the low-most decision of
	 * State Decision Algorithm (SDA): "Ebest better by topology than Erbest"
	 * If Ebest is absolutely better (A+1>B) than Erbest, it is not
	 * better by topology, so the decision should be NO, and the state
	 * should be MASTER, but in this implementation it will be PASSIVE.....
	 * see IEEE book, page 83, case 5
	 * this is true, provided that the State Decision Alg (SDA) is implemented as in 
	 * the standard, but it is not ! It seems that it is only for Ordinary Clock, or..
	 * I do not understand something :)
	 *
	 */  
	DBGBMC("DCA: Data set comparison \n");
	//DBGBMC("DCA: Comparing A%s with B%s\n",clockDescription(headerA,announceA),clockDescription(headerB,announceB));
	
	short comp = 0;
#ifndef WRPTPv2	
	if(announceA->wr_flags & WR_MASTER)
	  DBG("A is WR_MASTER\n");
	else if(announceA->wr_flags & WR_SLAVE)
	  DBG("A is WR_SLAVE\n");
	else
	  DBG("A is not WR\n");

	if(announceB->wr_flags & WR_MASTER)
	  DBG("B is WR_MASTER\n");
	else if(announceB->wr_flags & WR_SLAVE)
	  DBG("B is WR_SLAVE\n");
	else
	  DBG("B is not WR\n");


	/*white rabbit staff*/
	if(announceA->wr_flags & WR_MASTER && !(announceB->wr_flags & WR_MASTER))
	{
	  DBG("A better B [White Rabbit]\n");
	  return -1;
	}

	if(announceB->wr_flags & WR_MASTER && !(announceA->wr_flags & WR_MASTER))
	{
	  DBG("B better A [White Rabbit]\n");
	  return 1;
	}
#endif
	/*Identity comparison*/
	if (!memcmp(announceA->grandmasterIdentity,announceB->grandmasterIdentity,CLOCK_IDENTITY_LENGTH))
	{
		DBGBMC("DCA: Grandmasters are identical\n");
		//Algorithm part2 Fig 28
		if (announceA->stepsRemoved > announceB->stepsRemoved+1)
		{
		    DBGBMC("DCA: .. B better than A \n");
		    return 1;// B better than A
		}
		else if (announceB->stepsRemoved > announceA->stepsRemoved+1)
		{
		    DBGBMC("DCA: .. A better than B \n");
		    return -1;//A better than B
		}
		else //A within 1 of B
		{
			DBGBMC("DCA: .. A within 1 of B \n");
			if (announceA->stepsRemoved > announceB->stepsRemoved)
			{
				if (!memcmp(headerA->sourcePortIdentity.clockIdentity,ptpClock->parentPortIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH))
				{
				    DBGBMC("DCA: .. .. Sender=Receiver : Error -1");
				    return 0;
				}
				else
				{
				    DBGBMC("DCA: .. .. B better than A \n");
				    return 1;
				}

			}
			else if (announceB->stepsRemoved > announceA->stepsRemoved)
			{
				if (!memcmp(headerB->sourcePortIdentity.clockIdentity,ptpClock->parentPortIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH))
				{
				  	DBGBMC("DCA: .. .. Sender=Receiver : Error -1");
					return 0;
				}
				else
				{	DBGBMC("DCA: .. .. A better than B \n");
				  	return -1;
				}
			}
			else // steps removed A = steps removed B
			{
				DBGBMC("DCA: .. steps removed A = steps removed B \n");
				if (!memcmp(headerA->sourcePortIdentity.clockIdentity,headerB->sourcePortIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH))
				{
					DBG("DCA: .. Sender=Receiver : Error -2");
					return 0;
				}
				else if ((memcmp(headerA->sourcePortIdentity.clockIdentity,headerB->sourcePortIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH))<0)
				{
					DBGBMC("DCA: .. .. A better than B \n");
					return -1;
				}
				else
				{	DBGBMC("DCA: .. .. B better than A \n");
					return 1;
				}
			}

		}
	}
	else //GrandMaster are not identical
	{
		DBGBMC("DCA: GrandMaster are not identical\n");
		if(announceA->grandmasterPriority1 == announceB->grandmasterPriority1)
		{
			DBGBMC("DCA: .. A->Priority1 == B->Priority1 == %d\n",announceB->grandmasterPriority1);
			if (announceA->grandmasterClockQuality.clockClass == announceB->grandmasterClockQuality.clockClass)
			{
				DBGBMC("DCA: .. .. A->clockClass == B->clockClass == %d\n",announceB->grandmasterClockQuality.clockClass);
				if (announceA->grandmasterClockQuality.clockAccuracy == announceB->grandmasterClockQuality.clockAccuracy)
				{
					DBGBMC("DCA: .. .. .. A->clockAccuracy == B->clockAccuracy == %d\n",announceB->grandmasterClockQuality.offsetScaledLogVariance);
					if (announceA->grandmasterClockQuality.offsetScaledLogVariance == announceB->grandmasterClockQuality.offsetScaledLogVariance)
					{
						DBGBMC("DCA: .. .. .. .. A->offsetScaledLogVariance == B->offsetScaledLogVariance == %d\n",announceB->grandmasterClockQuality.offsetScaledLogVariance);
						if (announceA->grandmasterPriority2 == announceB->grandmasterPriority2)
						{
							DBGBMC("DCA: .. .. .. .. .. A->Priority2 == B->Priority2 == %d\n",announceB->grandmasterPriority2);
							comp = memcmp(announceA->grandmasterIdentity,announceB->grandmasterIdentity,CLOCK_IDENTITY_LENGTH);
							if (comp < 0)
							{
								DBGBMC("DCA: .. .. .. .. .. .. A better than B by clock ID\n");
								return -1;
							}
							else if (comp > 0)
							{
								DBGBMC("DCA: .. .. .. .. .. .. B better than A clock ID\n");
								return 1;
							}
							else
							{
								DBGBMC("DCA: .. .. .. .. .. .. not good: clock ID\n");
								return 0;
							}
						}
						else //Priority2 are not identical
						{
							comp =memcmp(&announceA->grandmasterPriority2,&announceB->grandmasterPriority2,1);
							if (comp < 0)
							{
								DBGBMC("DCA: .. .. .. .. .. A better than B by Priority2\n");
								return -1;
							}
							else if (comp > 0)
							{
								DBGBMC("DCA: .. .. .. .. .. B better than A by Priority2\n");
								return 1;
							}
							else
							{
								DBGBMC("DCA: .. .. .. .. .. not good: Priority2\n");
								return 0;
							}
						}
					}

					else //offsetScaledLogVariance are not identical
					{
						comp= memcmp(&announceA->grandmasterClockQuality.offsetScaledLogVariance,&announceB->grandmasterClockQuality.offsetScaledLogVariance,1);
						if (comp < 0)
						{
							DBGBMC("DCA: .. .. .. .. A better than B by offsetScaledLogVariance\n");
							return -1;
						}
						else if (comp > 0)
						{
							DBGBMC("DCA: .. .. .. .. B better than A by offsetScaledLogVariance\n");
							return 1;
						}
						else
						{	DBGBMC("DCA: .. .. .. .. not good: offsetScaledLogVariance\n");
							return 0;
						}
					}

				}

				else // Accuracy are not identitcal
				{
					comp = memcmp(&announceA->grandmasterClockQuality.clockAccuracy,&announceB->grandmasterClockQuality.clockAccuracy,1);
					if (comp < 0)
					{
						DBGBMC("DCA: .. .. .. A better than B by clockAccuracy\n");
						return -1;
					}
					else if (comp > 0)
					{
						DBGBMC("DCA: .. .. .. B better than A by clockAccuracy\n");
						return 1;
					}
					else
					{	
						DBGBMC("DCA: .. .. .. not good: clockAccuracy\n");
						return 0;
					}
				}

			}

			else //ClockClass are not identical
			{
				comp =  memcmp(&announceA->grandmasterClockQuality.clockClass,&announceB->grandmasterClockQuality.clockClass,1);
				if (comp < 0)
				{
					DBGBMC("DCA: .. .. A better than B by clockClass\n");
					return -1;
				}
				else if (comp > 0)
				{
					DBGBMC("DCA: .. .. B better than A by clockClass\n");
					return 1;
				}
				else
				{
					DBGBMC("DCA: .. .. not good:  clockClass\n");
					return 0;
				}
			}
		}

		else // Priority1 are not identical
		{
			comp =  memcmp(&announceA->grandmasterPriority1,&announceB->grandmasterPriority1,1);
			if (comp < 0)
			{
				DBGBMC("DCA: .. A better than B by Priority1\n");
				return -1;
			}
			else if (comp > 0)
			{
				DBGBMC("DCA: .. B better than A by Priority1\n");
				return 1;
			}
			else
			{
				DBGBMC("DCA: .. not good: Priority1\n");
				return 0;
			}
		}

	}

}

/*State decision algorithm 9.3.3 Fig 26*/
UInteger8 bmcStateDecision (MsgHeader *header,MsgAnnounce *announce,RunTimeOpts *rtOpts,PtpClock *ptpClock)
{
	DBGBMC("SDA: State Decision Algorith,\n");
	if (rtOpts->slaveOnly)
	{
		DBGBMC("SDA: .. Slave Only Mode: PTP_SLAVE\n");
		s1(header,announce,ptpClock);

		  return PTP_SLAVE;
	}

	if ((!ptpClock->number_foreign_records) && (ptpClock->portState == PTP_LISTENING))
	{
		DBGBMC("SDA: .. No foreing nasters : PTP_LISTENING\n");
		return PTP_LISTENING;
	}
	
	copyD0(&ptpClock->msgTmpHeader,&ptpClock->msgTmp.announce,ptpClock);

	if (ptpClock->clockQuality.clockClass < 128)
	{
		DBGBMC("SDA: .. clockClass < 128\n");
		if ((bmcDataSetComparison(&ptpClock->msgTmpHeader,&ptpClock->msgTmp.announce,header,announce,ptpClock)<0))
		{
			DBGBMC("SDA: .. .. D0 Better or better by topology then Ebest: YES => m1: PTP_MASTER\n");
			m1(ptpClock);
			return PTP_MASTER;
		}
		else if ((bmcDataSetComparison(&ptpClock->msgTmpHeader,&ptpClock->msgTmp.announce,header,announce,ptpClock)>0))
		{
			DBGBMC("SDA: .. .. D0 Better or better by topology then Ebest: NO => s1: PTP_PASSIVE =modify=>> PTP_MASTER\n");
			s1(header,announce,ptpClock);
			return PTP_PASSIVE;
		}
		else
		{
			DBGBMC("SDA: .. .. Error in bmcDataSetComparison..\n");
		}
	}

	else
	{
		/*
		 * This implementation is not precisely as in the standard !@!!!!!
		 * see: page 87, Figure 26
		 * it seems that it is foreseen only for ordinary clocks, since the
		 * condition: "Erbest same as Ebest" defaults to YES
		 *
		 */
		DBGBMC("SDA: .. clockClass > 128\n");
		if ((bmcDataSetComparison(&ptpClock->msgTmpHeader,&ptpClock->msgTmp.announce,header,announce,ptpClock))<0)
		{		
			DBGBMC("SDA: .. .. D0 Better or better by topology then Ebest: YES => m1: PTP_MASTER\n");
			m1(ptpClock);
			return PTP_MASTER;
		}
		else if ((bmcDataSetComparison(&ptpClock->msgTmpHeader,&ptpClock->msgTmp.announce,header,announce,ptpClock)>0))
		{
			/*
			 * For a boundary clock we should have more staff here !!!!!!!!
			 * see: page 87, Figure 26
			 */
			DBGBMC("SDA: .. .. D0 Better or better by topology then Ebest: NO => m1: PTP_SLAVE\n");
			s1(header,announce,ptpClock);
			return PTP_SLAVE;
		}
		else
		{
			DBGBMC("SDA: .. .. Error in bmcDataSetComparison..\n");
		}

	}
	return PTP_PASSIVE; /* only reached in error condition */

}

UInteger8 bmc(ForeignMasterRecord *foreignMaster,RunTimeOpts *rtOpts ,PtpClock *ptpClock )
{
	DBG("BMC: Best Master Clock Algorithm @ working\n");
	Integer16 i,best;




	if (!ptpClock->number_foreign_records)
	{
		DBGBMC("BMC: .. no foreign masters\n");
		if (ptpClock->portState == PTP_MASTER)
		{
			DBGBMC("BMC: .. .. m1: PTP_MASTER\n");
			m1(ptpClock);
			return ptpClock->portState;
		}
	}

	for (i=1,best = 0; i<ptpClock->number_foreign_records;i++)
	{
		DBGBMC("BMC: .. looking at %d foreign master\n",i);
		if ((bmcDataSetComparison(&foreignMaster[i].header,&foreignMaster[i].announce,
								 &foreignMaster[best].header,&foreignMaster[best].announce,ptpClock)) < 0)
		{
			DBGBMC("BMC: .. .. update currently best (%d) to new best = %d\n",best, i);
			best = i;
		}
	}

	DBGBMC("BMC: the best foreign master index: %d\n",best);
	ptpClock->foreign_record_best = best;


	return bmcStateDecision(&foreignMaster[best].header,&foreignMaster[best].announce,rtOpts,ptpClock);
}


