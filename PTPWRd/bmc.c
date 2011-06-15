/* bmc.c */

#include "ptpd.h"


/* Init ptpPortDS with run time values (initialization constants are in constants.h)*/
void initDataPort(RunTimeOpts *rtOpts, PtpPortDS *ptpPortDS)
{
  int i,j;
  j=0;
  DBG("initDataPort\n");

	/*init clockIdentity with MAC address and 0xFF and 0xFE. see spec 7.5.2.2.2*/
	//TODO: should be in initDataClock()
	for (i=0;i<CLOCK_IDENTITY_LENGTH;i++)
	{

		if (i==3) ptpPortDS->clockIdentity[i]=0xFF;
		else if (i==4) ptpPortDS->clockIdentity[i]=0xFE;
		else if (i == 5 && rtOpts->overrideClockIdentity != 0x0)
		  /*
		   * temporary hack to run the daemon on lo (single interface)
		   */
		  ptpPortDS->clockIdentity[i] = rtOpts->overrideClockIdentity;
		else
		{
		  ptpPortDS->clockIdentity[i]=ptpPortDS->port_uuid_field[j];
		  j++;
		}
	}
	DBGM(" clockIdentity................. %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n",
	    ptpPortDS->clockIdentity[0], ptpPortDS->clockIdentity[1],
	    ptpPortDS->clockIdentity[2], ptpPortDS->clockIdentity[3],
	    ptpPortDS->clockIdentity[4], ptpPortDS->clockIdentity[5],
	    ptpPortDS->clockIdentity[6], ptpPortDS->clockIdentity[7]
	    );

	/*
	 * White Rabbit - init static data fields
	 */
	ptpPortDS->wrConfig			     = rtOpts->wrConfig;
	ptpPortDS->deltasKnown			     = rtOpts->deltasKnown;
	ptpPortDS->wrStateTimeout		     = rtOpts->wrStateTimeout;
	ptpPortDS->wrStateRetry			     = rtOpts->wrStateRetry;
	ptpPortDS->calPeriod			     = rtOpts->calPeriod;	
	
	ptpPortDS->knownDeltaTx.scaledPicoseconds.lsb = rtOpts->knownDeltaTx.scaledPicoseconds.lsb;
	ptpPortDS->knownDeltaTx.scaledPicoseconds.msb = rtOpts->knownDeltaTx.scaledPicoseconds.msb;
	
	ptpPortDS->knownDeltaRx.scaledPicoseconds.lsb = rtOpts->knownDeltaRx.scaledPicoseconds.lsb;
	ptpPortDS->knownDeltaRx.scaledPicoseconds.msb = rtOpts->knownDeltaRx.scaledPicoseconds.msb;
	
	
	/* 
	 * White Rabbit - init dynamic data fields 
	 */
	initWrData(ptpPortDS);
	

/*Port configuration data set */

	/*PortIdentity Init (portNumber = 1 for an ardinary clock spec 7.5.2.3)*/
	memcpy(ptpPortDS->portIdentity.clockIdentity,ptpPortDS->clockIdentity,CLOCK_IDENTITY_LENGTH);

	ptpPortDS->logMinDelayReqInterval = DEFAULT_DELAYREQ_INTERVAL;
	ptpPortDS->peerMeanPathDelay.seconds = 0;
	ptpPortDS->peerMeanPathDelay.nanoseconds = 0;

	ptpPortDS->logAnnounceInterval = rtOpts->announceInterval;
	ptpPortDS->announceReceiptTimeout = DEFAULT_ANNOUNCE_RECEIPT_TIMEOUT;
	ptpPortDS->logSyncInterval = rtOpts->syncInterval;
	ptpPortDS->delayMechanism = DEFAULT_DELAY_MECHANISM;
	ptpPortDS->logMinPdelayReqInterval = DEFAULT_PDELAYREQ_INTERVAL;
	ptpPortDS->versionNumber = VERSION_PTP;

	/*Initialize seed for random number used with Announce Timeout (spec 9.2.6.11)*/
	srand(time(NULL));
	//ptpPortDS->R = getRand();

	/*Init other stuff*/
	ptpPortDS->number_foreign_records = 0;
	ptpPortDS->max_foreign_records = rtOpts->max_foreign_records;

	ptpPortDS->pending_tx_ts            = FALSE;
	ptpPortDS->pending_Synch_tx_ts      = 0;
	ptpPortDS->pending_DelayReq_tx_ts   = 0;
	ptpPortDS->pending_PDelayReq_tx_ts  = 0;
	ptpPortDS->pending_PDelayResp_tx_ts = 0;

}


void initDataClock(RunTimeOpts *rtOpts, PtpClockDS *ptpClockDS)
{
	fprintf(stderr, "DBG: initDataClock");
	/* Default data set */
	ptpClockDS->twoStepFlag = TWO_STEP_FLAG;
	ptpClockDS->numberPorts = rtOpts->portNumber;
	ptpClockDS->clockQuality.clockAccuracy = rtOpts->clockQuality.clockAccuracy;
	ptpClockDS->clockQuality.offsetScaledLogVariance = rtOpts->clockQuality.offsetScaledLogVariance;

	
	if(rtOpts->priority1 == DEFAULT_PRIORITY1 && rtOpts->wrConfig != NON_WR)
	  ptpClockDS->priority1 = WR_PRIORITY1;  
	else
	  ptpClockDS->priority1 = rtOpts->priority1;

	ptpClockDS->priority2 = rtOpts->priority2;

	ptpClockDS->domainNumber = rtOpts->domainNumber;
	ptpClockDS->slaveOnly = rtOpts->slaveOnly;
	if(rtOpts->slaveOnly)
           rtOpts->clockQuality.clockClass = 255;


	ptpClockDS->clockQuality.clockClass = rtOpts->clockQuality.clockClass;
	
	//WRPTP
	ptpClockDS->primarySlavePortNumber=0;
}
/////////////

/*Local clock is becoming Master. Table 13 (9.3.5) of the spec.*/
void m1(PtpPortDS *ptpPortDS)
{
  DBG("[%s]\n",__func__);
	/*Current data set update*/
	ptpPortDS->ptpClockDS->stepsRemoved = 0;
	ptpPortDS->ptpClockDS->offsetFromMaster.nanoseconds = 0;
	ptpPortDS->ptpClockDS->offsetFromMaster.seconds = 0;
	ptpPortDS->ptpClockDS->meanPathDelay.nanoseconds = 0;
	ptpPortDS->ptpClockDS->meanPathDelay.seconds = 0;

	/*Parent data set*/
	memcpy(ptpPortDS->ptpClockDS->parentPortIdentity.clockIdentity,ptpPortDS->clockIdentity,CLOCK_IDENTITY_LENGTH);
	ptpPortDS->ptpClockDS->parentPortIdentity.portNumber = 0;
	ptpPortDS->ptpClockDS->parentStats = DEFAULT_PARENTS_STATS;
	ptpPortDS->ptpClockDS->observedParentClockPhaseChangeRate = 0;
	ptpPortDS->ptpClockDS->observedParentOffsetScaledLogVariance = 0;
	memcpy(ptpPortDS->ptpClockDS->grandmasterIdentity,ptpPortDS->clockIdentity,CLOCK_IDENTITY_LENGTH);
	ptpPortDS->ptpClockDS->grandmasterClockQuality.clockAccuracy = ptpPortDS->ptpClockDS->clockQuality.clockAccuracy;
	ptpPortDS->ptpClockDS->grandmasterClockQuality.clockClass = ptpPortDS->ptpClockDS->clockQuality.clockClass;
	ptpPortDS->ptpClockDS->grandmasterClockQuality.offsetScaledLogVariance = ptpPortDS->ptpClockDS->clockQuality.offsetScaledLogVariance;
	ptpPortDS->ptpClockDS->grandmasterPriority1 = ptpPortDS->ptpClockDS->priority1;
	ptpPortDS->ptpClockDS->grandmasterPriority2 = ptpPortDS->ptpClockDS->priority2;

	/*White Rabbit*/
#ifdef WRPTPv2
	ptpPortDS->parentWrConfig      	  = ptpPortDS->wrConfig;
	ptpPortDS->parentIsWRnode     = (ptpPortDS->wrConfig != NON_WR) ;
	ptpPortDS->parentWrModeON     = ptpPortDS->wrModeON;
	ptpPortDS->parentCalibrated = ptpPortDS->calibrated;
#else
	ptpPortDS->parentWrNodeMode   = ptpPortDS->wrMode;
	ptpPortDS->parentIsWRnode     = (ptpPortDS->wrMode != NON_WR) ;
	ptpPortDS->parentWrModeON     = ptpPortDS->wrModeON;
	ptpPortDS->parentCalibrated = ptpPortDS->calibrated;
#endif	

	/*Time Properties data set*/
	ptpPortDS->ptpClockDS->timeSource = INTERNAL_OSCILLATOR;
		
}


/*Local clock is synchronized to Ebest Table 16 (9.3.5) of the spec*/
void s1(MsgHeader *header,MsgAnnounce *announce,PtpPortDS *ptpPortDS)
{
	DBG("[%s]\n",__func__);
	/*Current DS*/
	ptpPortDS->ptpClockDS->stepsRemoved = announce->stepsRemoved + 1;

	/*Parent DS*/

	memcpy(ptpPortDS->ptpClockDS->parentPortIdentity.clockIdentity,header->sourcePortIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH);
	
	ptpPortDS->ptpClockDS->parentPortIdentity.portNumber = header->sourcePortIdentity.portNumber;
	memcpy(ptpPortDS->ptpClockDS->grandmasterIdentity,announce->grandmasterIdentity,CLOCK_IDENTITY_LENGTH);
	ptpPortDS->ptpClockDS->grandmasterClockQuality.clockAccuracy = announce->grandmasterClockQuality.clockAccuracy;
	ptpPortDS->ptpClockDS->grandmasterClockQuality.clockClass = announce->grandmasterClockQuality.clockClass;
	ptpPortDS->ptpClockDS->grandmasterClockQuality.offsetScaledLogVariance = announce->grandmasterClockQuality.offsetScaledLogVariance;
	ptpPortDS->ptpClockDS->grandmasterPriority1 = announce->grandmasterPriority1;
	ptpPortDS->ptpClockDS->grandmasterPriority2 = announce->grandmasterPriority2;

	/*White Rabbit*/
	ptpPortDS->parentIsWRnode    	 = ((announce->wr_flags & WR_NODE_MODE) != NON_WR);
	ptpPortDS->parentWrModeON     	  = ((announce->wr_flags & WR_IS_WR_MODE) == WR_IS_WR_MODE);
	ptpPortDS->parentCalibrated = ((announce->wr_flags & WR_IS_CALIBRATED) == WR_IS_CALIBRATED);
	DBGBMC(" S1: copying wr_flags.......... 0x%x\n", announce->wr_flags);
	DBGBMC(" S1: parentIsWRnode............ 0x%x\n", ptpPortDS->parentIsWRnode);
	DBGBMC(" S1: parentWrModeON............ 0x%x\n", ptpPortDS->parentWrModeON);
	DBGBMC(" S1: parentCalibrated.......... 0x%x\n", ptpPortDS->parentCalibrated);	
#ifdef WRPTPv2
	ptpPortDS->parentWrConfig      =   announce->wr_flags & WR_NODE_MODE;
	DBGBMC(" S1: parentWrConfig.......  0x%x\n", ptpPortDS->parentWrConfig);
#else
	ptpPortDS->parentWrNodeMode   =   announce->wr_flags & WR_NODE_MODE;
	DBGBMC(" S1: parentWrNodeMode....  0x%x\n",ptpPortDS->parentWrNodeMode);
#endif

	
	
	/*Timeproperties DS*/
	ptpPortDS->ptpClockDS->currentUtcOffset = announce->currentUtcOffset;
	ptpPortDS->ptpClockDS->currentUtcOffsetValid = ((header->flagField[1] & 0x04) == 0x04); //"Valid" is bit 2 in second octet of flagfield
	ptpPortDS->ptpClockDS->leap59 = ((header->flagField[1] & 0x02) == 0x02);
	ptpPortDS->ptpClockDS->leap61 = ((header->flagField[1] & 0x01) == 0x01);
	ptpPortDS->ptpClockDS->timeTraceable = ((header->flagField[1] & 0x10) == 0x10);
	ptpPortDS->ptpClockDS->frequencyTraceable = ((header->flagField[1] & 0x20) == 0x20);
	ptpPortDS->ptpClockDS->ptpTimescale = ((header->flagField[1] & 0x08) == 0x08);
	ptpPortDS->ptpClockDS->timeSource = announce->timeSource;
}


/*Copy local data set into header and announce message. 9.3.4 table 12*/
void copyD0(MsgHeader *header, MsgAnnounce *announce, PtpPortDS *ptpPortDS)
{
	DBG("[%s]\n",__func__);
  	announce->grandmasterPriority1 = ptpPortDS->ptpClockDS->priority1;
	memcpy(announce->grandmasterIdentity,ptpPortDS->clockIdentity,CLOCK_IDENTITY_LENGTH);
	announce->grandmasterClockQuality.clockClass = ptpPortDS->ptpClockDS->clockQuality.clockClass;
	announce->grandmasterClockQuality.clockAccuracy = ptpPortDS->ptpClockDS->clockQuality.clockAccuracy;
	announce->grandmasterClockQuality.offsetScaledLogVariance = ptpPortDS->ptpClockDS->clockQuality.offsetScaledLogVariance;
	announce->grandmasterPriority2 = ptpPortDS->ptpClockDS->priority2;
	announce->stepsRemoved = 0;
	memcpy(header->sourcePortIdentity.clockIdentity,ptpPortDS->clockIdentity,CLOCK_IDENTITY_LENGTH);

	/*White Rabbit*/
#ifdef WRPTPv2
	announce->wr_flags = (announce->wr_flags | ptpPortDS->wrConfig) & WR_NODE_MODE  ;
	announce->wr_flags =  announce->wr_flags | ptpPortDS->calibrated << 2;
	announce->wr_flags =  announce->wr_flags | ptpPortDS->wrModeON     << 3;

#else
	announce->wr_flags = (announce->wr_flags | ptpPortDS->wrMode) & WR_NODE_MODE  ;
	announce->wr_flags =  announce->wr_flags | ptpPortDS->calibrated << 2;
	announce->wr_flags =  announce->wr_flags | ptpPortDS->wrModeON     << 3;
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

Integer8 bmcDataSetComparison(MsgHeader *headerA, MsgAnnounce *announceA, UInteger16 receptionPortNumberA,
			      MsgHeader *headerB, MsgAnnounce *announceB, UInteger16 receptionPortNumberB,
			      PtpPortDS *ptpPortDS)
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
	if (!memcmp(announceA->grandmasterIdentity,announceB->grandmasterIdentity,CLOCK_IDENTITY_LENGTH)) // (1)
	{
		DBGBMC("DCA: Grandmasters are identical\n");
		//Algorithm part2 Fig 28
		// compare steps removed of A and B
		if (announceA->stepsRemoved > announceB->stepsRemoved+1) //(2)
		{
		    DBGBMC("DCA: .. B better than A \n");
		    //return 1;// B better than A
		    return B_better_then_A;
		}
		else if (announceB->stepsRemoved > announceA->stepsRemoved+1) //(2)
		{
		    DBGBMC("DCA: .. A better than B \n");
		    //return -1;//A better than B
		    return A_better_then_B;
		}
		else //A within 1 of B
		{
			DBGBMC("DCA: .. A within 1 of B \n");
			// Compare Steps Removed of A and B
			if (announceA->stepsRemoved > announceB->stepsRemoved) // (3)
			{
				DBGBMC("DCA: .. .. A > B\n");
				
				/* Compare Identities of Receiver of A and Sender of A */
				
				if (!memcmp(headerA->sourcePortIdentity.clockIdentity,ptpPortDS->portIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH)) //(4)
				{
				    DBGBMC("DCA: .. .. Comparing Identities of A: Sender=Receiver : Error -1");
				    //return 0;
				    return A_equals_B;
				}
				else if(memcmp(headerA->sourcePortIdentity.clockIdentity,ptpPortDS->portIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH) > 0)//(4)
				{
				  
				    DBGBMC("DCA: .. .. .. B better than A (Comparing Identities of A: Receiver < Sender)\n");
				    return B_better_then_A;
				}
				else if(memcmp(headerA->sourcePortIdentity.clockIdentity,ptpPortDS->portIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH) < 0)//(4)
				{
				  
				    DBGBMC("DCA: .. .. .. B better by topology than A (Comparing Identities of A: Receiver > Sender)\n");
				    return B_better_by_topology_then_A;
				}
				else 
				   DBGBMC("Impossible to get here \n");

			}
			else if (announceB->stepsRemoved > announceA->stepsRemoved) //(3)
			{
				if (!memcmp(headerB->sourcePortIdentity.clockIdentity,ptpPortDS->portIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH))  //(5)
				{
				  	DBGBMC("DCA: .. .. Comparing Identities of A: Sender=Receiver : Error -1");
					return A_equals_B;
				}
				else if(memcmp(headerB->sourcePortIdentity.clockIdentity,ptpPortDS->portIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH) > 0)//(5)
				{
				  
				    DBGBMC("DCA: .. .. .. B better than A (Comparing Identities of A: Receiver < Sender)\n");
				    return A_better_then_B;
				}
				else if(memcmp(headerB->sourcePortIdentity.clockIdentity,ptpPortDS->portIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH) < 0)//(5)
				{
				  
				    DBGBMC("DCA: .. .. .. A better by topology than B (Comparing Identities of A: Receiver > Sender)\n");
				    return A_better_by_topology_then_B;
				}
				else 
				   DBGBMC("Impossible to get here \n");
			}
			else // steps removed A = steps removed B
			{
				DBGBMC("DCA: .. steps removed A = steps removed B \n");
				// compare Identities of Senders of A and B
				if (!memcmp(headerA->sourcePortIdentity.clockIdentity,headerB->sourcePortIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH)) //(6)
				{
					
					//Compare Port Numbers of receivers of A and B
					//TODO:
					if(receptionPortNumberA == receptionPortNumberB) //(7)
					{
					    DBG("DCA: .. Sender=Receiver : Error -2");
					    //return 0;
					    return A_equals_B;
					
					}
					else if(receptionPortNumberA < receptionPortNumberB) //(7)
					{
					    DBGBMC("DCA: .. .. .. .. A better by topology than B (Compare Port Numbers of Receivers of A and B : A < B)\n");
					    return A_better_by_topology_then_B;
					}
					else if(receptionPortNumberA > receptionPortNumberB) //(7)
					{
					    DBGBMC("DCA: .. .. .. .. B better by topology than A (Compare Port Numbers of Receivers of A and B : A > B)\n");
					    return B_better_by_topology_then_A;
					}

					
					
				}
				else if ((memcmp(headerA->sourcePortIdentity.clockIdentity,headerB->sourcePortIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH))<0)
				{
					DBGBMC("DCA: .. .. A better by topology than B \n");
					//return -1;
					return A_better_by_topology_then_B;
				}
				else
				{	DBGBMC("DCA: .. .. B better by topologythan A \n");
					//return 1;
					return B_better_by_topology_then_A;
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
UInteger8 bmcStateDecision (MsgHeader *header,MsgAnnounce *announce, UInteger16 receptionPortNumber, RunTimeOpts *rtOpts,PtpPortDS *ptpPortDS)
{
	DBGBMC("SDA: State Decision Algorith,\n");
	if (rtOpts->slaveOnly)
	{
		DBGBMC("SDA: .. Slave Only Mode: PTP_SLAVE\n");
		s1(header,announce,ptpPortDS);

		  return PTP_SLAVE;
	}

	if ((!ptpPortDS->number_foreign_records) && (ptpPortDS->portState == PTP_LISTENING))
	{
		DBGBMC("SDA: .. No foreing nasters : PTP_LISTENING\n");
		return PTP_LISTENING;
	}
	
	copyD0(&ptpPortDS->msgTmpHeader,&ptpPortDS->msgTmp.announce,ptpPortDS);

	if (ptpPortDS->ptpClockDS->clockQuality.clockClass < 128)
	{
		DBGBMC("SDA: .. clockClass < 128\n");
		if ((bmcDataSetComparison(&ptpPortDS->msgTmpHeader,
					  &ptpPortDS->msgTmp.announce,
					   ptpPortDS->portIdentity.portNumber,
					   header,announce,receptionPortNumber,ptpPortDS)<0))
		{
			DBGBMC("SDA: .. .. D0 Better or better by topology then Ebest: YES => m1: PTP_MASTER\n");
			m1(ptpPortDS);
			return PTP_MASTER;
		}
		else if ((bmcDataSetComparison(&ptpPortDS->msgTmpHeader,
					       &ptpPortDS->msgTmp.announce,
					        ptpPortDS->portIdentity.portNumber,
					        header,announce,receptionPortNumber,ptpPortDS)>0))
		{
			DBGBMC("SDA: .. .. D0 Better or better by topology then Ebest: NO => s1: PTP_PASSIVE =modify=>> PTP_SLAVE\n");
			s1(header,announce,ptpPortDS);
#ifdef WRPTPv2			
			return PTP_SLAVE;
#else
			return PTP_PASSIVE;
#endif			
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
		//if ((bmcDataSetComparison(&ptpPortDS->msgTmpHeader,&ptpPortDS->msgTmp.announce,header,announce,ptpPortDS))<0)
		/* compare  D0 with Ebest */
		if ((bmcDataSetComparison(&ptpPortDS->msgTmpHeader,
					  &ptpPortDS->msgTmp.announce,
					   ptpPortDS->portIdentity.portNumber,
					  &ptpPortDS->ptpClockDS->bestForeign->header,
					  &ptpPortDS->ptpClockDS->bestForeign->announce,
					   ptpPortDS->ptpClockDS->bestForeign->receptionPortNumber,
					  ptpPortDS)<0))		
		{		
			DBGBMC("SDA: .. .. D0 Better or better by topology then Ebest: YES => m1: PTP_MASTER\n");
			m1(ptpPortDS);
			return PTP_MASTER;
		}
		//else if ((bmcDataSetComparison(&ptpPortDS->msgTmpHeader,&ptpPortDS->msgTmp.announce,header,announce,ptpPortDS)>0))
		else if ((bmcDataSetComparison( &ptpPortDS->msgTmpHeader,
						&ptpPortDS->msgTmp.announce,
						 ptpPortDS->portIdentity.portNumber,
						&ptpPortDS->ptpClockDS->bestForeign->header,
						&ptpPortDS->ptpClockDS->bestForeign->announce,
						 ptpPortDS->ptpClockDS->bestForeign->receptionPortNumber,
						 ptpPortDS)>0))
		{
#ifdef WRPTPv2
			DBGBMC("SDA: .. .. D0 Better or better by topology then Ebest: NO\n");
			if(ptpPortDS->ptpClockDS->bestForeign->receptionPortNumber == ptpPortDS->portIdentity.portNumber)
			{
				
				DBGBMC("SDA: .. .. .. Ebest received on port r (=%d): YES => s1: PTP_SLAVE\n",ptpPortDS->ptpClockDS->bestForeign->receptionPortNumber);
				s1(header,announce,ptpPortDS);
				return PTP_SLAVE;
			}
			else
			{
				DBGBMC("SDA: .. .. .. Ebest received on port r (foreign_receivd_on=%d,current_port=%d ): NO ->> no implemented -> PTP_SLAVE\n", \
				ptpPortDS->ptpClockDS->bestForeign->receptionPortNumber, ptpPortDS->portIdentity.portNumber);
				
				if ((bmcDataSetComparison(&ptpPortDS->ptpClockDS->bestForeign->header,
							  &ptpPortDS->ptpClockDS->bestForeign->announce,
							   ptpPortDS->ptpClockDS->bestForeign->receptionPortNumber,
							   header,announce,receptionPortNumber,ptpPortDS))<0)	
				{		
					DBGBMC("SDA: .. .. .. .. Ebest better or better by topology then Erbest: YES =>  PTP_SLAVE [modifiedBMC->> to be implemented]\n");
					s1(header,announce,ptpPortDS); //TODO: change according to the spec
					return PTP_SLAVE;
				}
				else if ((bmcDataSetComparison( &ptpPortDS->ptpClockDS->bestForeign->header,
								&ptpPortDS->ptpClockDS->bestForeign->announce,
								 ptpPortDS->ptpClockDS->bestForeign->receptionPortNumber,
								 header,announce,receptionPortNumber,ptpPortDS))>0)
				{
					DBGBMC("SDA: .. .. .. .. Ebest better or better by topology then Erbest: NO =>  PTP_MASTER [m3() to be implemented]\n");
					m1(ptpPortDS); //TODO: change according to the spec
					return PTP_MASTER;			
				}
				else
				{
					DBGBMC("SDA: .. .. Error in bmcDataSetComparison..\n");
				}				  
			}

#else
			/*
			 * For a boundary clock we should have more staff here !!!!!!!!
			 * see: page 87, Figure 26
			 */			
			DBGBMC("SDA: .. .. D0 Better or better by topology then Ebest: NO => m1: PTP_SLAVE\n");
			s1(header,announce,ptpPortDS);
			return PTP_SLAVE;
#endif			
		}
		else
		{
			DBGBMC("SDA: .. .. Error in bmcDataSetComparison..\n");
		}

	}
	return PTP_PASSIVE; /* only reached in error condition */

}

UInteger8 bmc(ForeignMasterRecord *foreignMaster,RunTimeOpts *rtOpts ,PtpPortDS *ptpPortDS )
{
	DBG("BMC: Best Master Clock Algorithm @ working\n");
	Integer16 i,best;

////////// move this  -- all below
//         do it separately for multiple ports
	if (!ptpPortDS->number_foreign_records)
	{
		DBGBMC("BMC: .. no foreign masters\n");
		if (ptpPortDS->portState == PTP_MASTER)
		{
			DBGBMC("BMC: .. .. m1: PTP_MASTER\n");
			m1(ptpPortDS);
			return ptpPortDS->portState;
		}
	}

// 	for (i=1,best = 0; i<ptpPortDS->number_foreign_records;i++)
// 	{
// 		DBGBMC("BMC: .. looking at %d foreign master\n",i);
// 		if ((bmcDataSetComparison(&foreignMaster[i].header,&foreignMaster[i].announce,
// 								 &foreignMaster[best].header,&foreignMaster[best].announce,ptpPortDS)) < 0)
// 		{
// 			DBGBMC("BMC: .. .. update currently best (%d) to new best = %d\n",best, i);
// 			best = i;
// 		}
// 	}
// 
// 	DBGBMC("BMC: the best foreign master index: %d\n",best);
// 	ptpPortDS->foreign_record_best = best;
////////// move this - all above

	best = ptpPortDS->foreign_record_best;

	return bmcStateDecision(&foreignMaster[best].header,&foreignMaster[best].announce, foreignMaster[best].receptionPortNumber, rtOpts,ptpPortDS);
}

UInteger8 ErBest(ForeignMasterRecord *foreignMaster,PtpPortDS *ptpPortDS )
{
	Integer16 i,best;
	
	if (!ptpPortDS->number_foreign_records)
	{
	    //nothing to look for
	    return -1;
	}

	for (i=1,best = 0; i<ptpPortDS->number_foreign_records;i++)
	{
		DBGBMC("BMC: .. looking at %d foreign master\n",i);
		if ((bmcDataSetComparison(&foreignMaster[i].header,
					  &foreignMaster[i].announce,
					   foreignMaster[i].receptionPortNumber,
					  &foreignMaster[best].header,
					  &foreignMaster[best].announce,
					   foreignMaster[best].receptionPortNumber,
					  ptpPortDS)) < 0)
		{
			DBGBMC("BMC: .. .. update currently best (%d) to new best = %d\n",best, i);
			best = i;
		}
	}

 	DBGBMC("ErBest: the best foreign master for port = %d is indexed = %d received on port = %d\n",\
 		 ptpPortDS->portIdentity.portNumber, best, foreignMaster[best].receptionPortNumber);
		
	ptpPortDS->foreign_record_best = best;

	return best;
}

UInteger8 EBest(PtpPortDS *ptpPortDS )
{
	Integer16 i;
	Integer16 Ebest;
	Integer16 ERbest_i;
	Integer16 ERbest_b;	
  
	
	for (Ebest=0; Ebest < ptpPortDS->ptpClockDS->numberPorts; Ebest++)
	{
		if(ptpPortDS[Ebest].number_foreign_records > 0)
			break;
	}
	
	
	for (i= Ebest + 1; i < ptpPortDS->ptpClockDS->numberPorts; i++)
	{
	  
		if(ptpPortDS[i].number_foreign_records > 0)
			continue;
		
		ERbest_i 	= ptpPortDS[i].foreign_record_best;
		ERbest_b	= ptpPortDS[Ebest].foreign_record_best;
		
		DBGBMC("BMC: .. looking at %d foreign master\n",i);
		if ((bmcDataSetComparison(&ptpPortDS[i].foreign[ERbest_i].header,   	\
					  &ptpPortDS[i].foreign[ERbest_i].announce, 	\
					   ptpPortDS[i].foreign[ERbest_i].receptionPortNumber, 	\
					  &ptpPortDS[Ebest].foreign[ERbest_b].header,	\
					  &ptpPortDS[Ebest].foreign[ERbest_b].announce,	\
					   ptpPortDS[i].foreign[ERbest_i].receptionPortNumber, 	\
					   ptpPortDS)) < 0)
		{
			DBGBMC("BMC: .. .. update currently best (%d) to new best = %d\n",Ebest, i);
			Ebest = i;
		}
	}
	ptpPortDS->ptpClockDS->Ebest = Ebest;
	
	ERbest_b = ptpPortDS[Ebest].foreign_record_best;
	
	ptpPortDS->ptpClockDS->bestForeign = &ptpPortDS[Ebest].foreign[ERbest_b];
	
	DBGBMC("Ebest: the port with the best foreign master number=%d, the foreign master record number=%d\n",\
		ptpPortDS[Ebest].foreign[ERbest_b].receptionPortNumber ,Ebest);

	return Ebest;
}
