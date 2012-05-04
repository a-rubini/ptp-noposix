/* bmc.c */

#include "ptpd.h"


/* Init ptpPortDS with run time values (initialization constants are in constants.h)*/
void initDataPort(RunTimeOpts *rtOpts, PtpPortDS *ptpPortDS)
{
  int i,j;
  j=0;


/* Default data set */
//mergeProblem:	ptpClock->twoStepFlag = TWO_STEP_FLAG;

  PTPD_TRACE(TRACE_BMC, ptpPortDS, "initDataPort\n");

	ptpPortDS->doRestart = FALSE;
	/*init clockIdentity with MAC address and 0xFF and 0xFE. see spec 7.5.2.2.2*/
	//TODO (11): should be in initDataClock()
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
	PTPD_TRACE(TRACE_BMC, ptpPortDS," clockIdentity................. %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n",
	    ptpPortDS->clockIdentity[0], ptpPortDS->clockIdentity[1],
	    ptpPortDS->clockIdentity[2], ptpPortDS->clockIdentity[3],
	    ptpPortDS->clockIdentity[4], ptpPortDS->clockIdentity[5],
	    ptpPortDS->clockIdentity[6], ptpPortDS->clockIdentity[7]
	    );

	/*
	 * White Rabbit - init static data fields
	 */
	ptpPortDS->wrConfig			     = rtOpts->wrConfig;
	ptpPortDS->phyCalibrationRequired= rtOpts->phyCalibrationRequired;
	ptpPortDS->wrStateTimeout		     = rtOpts->wrStateTimeout;
	ptpPortDS->wrStateRetry			     = rtOpts->wrStateRetry;
	ptpPortDS->calPeriod			     = rtOpts->calPeriod;
	ptpPortDS->calRetry			     = ptpPortDS->ptpClockDS->numberPorts + 2;


	/*
	 * White Rabbit - init dynamic data fields
	 */
	initWrData(ptpPortDS, INIT);
	

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
	//srand(time(NULL));
	//ptpClock->R = getRand();

	/*Init other stuff*/
	ptpPortDS->number_foreign_records = 0;
	ptpPortDS->max_foreign_records = rtOpts->max_foreign_records;
}

/* initialize ptpClockDS*/
void initDataClock(RunTimeOpts *rtOpts, PtpClockDS *ptpClockDS)
{
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

	ptpClockDS->clockClassValidityTimeout = DEFAULT_CLOCKCLASS_VALIDATE_TIMEOUT;
	
	/*if(rtOpts->slaveOnly)
          rtOpts->clockQuality.clockClass = 255;*/
	if((rtOpts->masterOnly == TRUE || rtOpts->primarySource == TRUE || rtOpts->clockQuality.clockClass == 6) )
	{
	  if(extsrcLocked()== TRUE)
	  {
	    timerInit(&ptpClockDS->clockClassValidityTimer, "clockClass");
	    timerStart(&ptpClockDS->clockClassValidityTimer, 1000 * (pow_2(ptpClockDS->clockClassValidityTimeout)));
	    rtOpts->clockQuality.clockClass = 6; 
	    //printf("Clocked to GPS, clockClass=6, timer started\n");
	  }
	  else
	  {
	    PTPD_TRACE(TRACE_ERROR, NULL, "\n\n\n ERROR: you want to be a primary source of time but you are not connected"\
		   " locked to external source of time, setting clockClass =13,"\
		   " but please do investigate the issue !!! \n\n\n");
	    rtOpts->clockQuality.clockClass = 13; /*table 5, p55, PTP spec*/
	  }
	    
	}

	ptpClockDS->clockQuality.clockClass = rtOpts->clockQuality.clockClass;  
	
	//WRPTP
	ptpClockDS->primarySlavePortNumber=0;
	
	ptpClockDS->Ebest = -1;
	

}

/*Local clock is becoming Master. Table 13 (9.3.5) of the spec.*/
void m1(PtpPortDS *ptpPortDS)
{

#if 0
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
#endif

	/*White Rabbit*/
	ptpPortDS->parentWrConfig      	= ptpPortDS->wrConfig;
	ptpPortDS->parentIsWRnode     	= (ptpPortDS->wrConfig != NON_WR) ;
	ptpPortDS->parentWrModeON     	= ptpPortDS->wrModeON;
	ptpPortDS->parentCalibrated 	= ptpPortDS->calibrated;

	/*Time Properties data set*/
	ptpPortDS->ptpClockDS->timeSource = INTERNAL_OSCILLATOR;
		
	ptpPortDS->wrSlaveRole = NON_SLAVE;
	
	ptpPortDS->ptpClockDS->primarySlavePortNumber=0;
}

void m3(PtpPortDS *ptpPortDS)
{
	ptpPortDS->wrSlaveRole = NON_SLAVE;
}

void p1(PtpPortDS *ptpPortDS)
{
	ptpPortDS->wrSlaveRole = NON_SLAVE;
}


/*Local clock is synchronized to Ebest Table 16 (9.3.5) of the spec*/
void s1(MsgHeader *header,MsgAnnounce *announce,PtpPortDS *ptpPortDS)
{
	PTPD_TRACE(TRACE_BMC, ptpPortDS,"[%s]\n",__func__);
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
	PTPD_TRACE(TRACE_BMC, ptpPortDS," S1: copying wr_flags.......... 0x%x\n", announce->wr_flags);
	PTPD_TRACE(TRACE_BMC, ptpPortDS," S1: parentIsWRnode............ 0x%x\n", ptpPortDS->parentIsWRnode);
	PTPD_TRACE(TRACE_BMC, ptpPortDS," S1: parentWrModeON............ 0x%x\n", ptpPortDS->parentWrModeON);
	PTPD_TRACE(TRACE_BMC, ptpPortDS," S1: parentCalibrated.......... 0x%x\n", ptpPortDS->parentCalibrated);	

	ptpPortDS->parentWrConfig      =   announce->wr_flags & WR_NODE_MODE;
	PTPD_TRACE(TRACE_BMC, ptpPortDS," S1: parentWrConfig.......  0x%x\n", ptpPortDS->parentWrConfig);
	
	ptpPortDS->ptpClockDS->primarySlavePortNumber	= ptpPortDS->portIdentity.portNumber;

 PTPD_TRACE(TRACE_BMC, ptpPortDS," S1 : announceID = %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n",
      header->sourcePortIdentity.clockIdentity[0], header->sourcePortIdentity.clockIdentity[1],
      header->sourcePortIdentity.clockIdentity[2], header->sourcePortIdentity.clockIdentity[3],
      header->sourcePortIdentity.clockIdentity[4], header->sourcePortIdentity.clockIdentity[5],
      header->sourcePortIdentity.clockIdentity[6], header->sourcePortIdentity.clockIdentity[7]);
	

 PTPD_TRACE(TRACE_BMC, ptpPortDS," S1 : parent = %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n",
      ptpPortDS->ptpClockDS->parentPortIdentity.clockIdentity[0], ptpPortDS->ptpClockDS->parentPortIdentity.clockIdentity[1],
      ptpPortDS->ptpClockDS->parentPortIdentity.clockIdentity[2], ptpPortDS->ptpClockDS->parentPortIdentity.clockIdentity[3],
      ptpPortDS->ptpClockDS->parentPortIdentity.clockIdentity[4], ptpPortDS->ptpClockDS->parentPortIdentity.clockIdentity[5],
      ptpPortDS->ptpClockDS->parentPortIdentity.clockIdentity[6], ptpPortDS->ptpClockDS->parentPortIdentity.clockIdentity[7]);
	
	
	PTPD_TRACE(TRACE_BMC, ptpPortDS," S1: g-masterIdentity[announce]. %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n",
	    announce->grandmasterIdentity[0], announce->grandmasterIdentity[1],
	    announce->grandmasterIdentity[2], announce->grandmasterIdentity[3],
	    announce->grandmasterIdentity[4], announce->grandmasterIdentity[5],
	    announce->grandmasterIdentity[6], announce->grandmasterIdentity[7]);	

	PTPD_TRACE(TRACE_BMC, ptpPortDS," S1: g-masterIdentity[clockDS].. %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n",
	    ptpPortDS->ptpClockDS->grandmasterIdentity[0], ptpPortDS->ptpClockDS->grandmasterIdentity[1],
	    ptpPortDS->ptpClockDS->grandmasterIdentity[2], ptpPortDS->ptpClockDS->grandmasterIdentity[3],
	    ptpPortDS->ptpClockDS->grandmasterIdentity[4], ptpPortDS->ptpClockDS->grandmasterIdentity[5],
	    ptpPortDS->ptpClockDS->grandmasterIdentity[6], ptpPortDS->ptpClockDS->grandmasterIdentity[7]);	    
	    
	
	/*Timeproperties DS*/
	ptpPortDS->ptpClockDS->currentUtcOffset = announce->currentUtcOffset;
	ptpPortDS->ptpClockDS->currentUtcOffsetValid = ((header->flagField[1] & 0x04) == 0x04); //"Valid" is bit 2 in second octet of flagfield
	ptpPortDS->ptpClockDS->leap59 = ((header->flagField[1] & 0x02) == 0x02);
	ptpPortDS->ptpClockDS->leap61 = ((header->flagField[1] & 0x01) == 0x01);
	ptpPortDS->ptpClockDS->timeTraceable = ((header->flagField[1] & 0x10) == 0x10);
	ptpPortDS->ptpClockDS->frequencyTraceable = ((header->flagField[1] & 0x20) == 0x20);
	ptpPortDS->ptpClockDS->ptpTimescale = ((header->flagField[1] & 0x08) == 0x08);
	ptpPortDS->ptpClockDS->timeSource = announce->timeSource;
	
	ptpPortDS->wrSlaveRole = PRIMARY_SLAVE;
}

void s2(MsgHeader *header,MsgAnnounce *announce,PtpPortDS *ptpPortDS)
{
	
	
	/*Copy new foreign master data set from Announce message*/
	ptpPortDS->wrSlaveRole = SECONDARY_SLAVE;
	
	memcpy(ptpPortDS->secondaryForeignMaster.foreignMasterPortIdentity.clockIdentity,header->sourcePortIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH);
	ptpPortDS->secondaryForeignMaster.foreignMasterPortIdentity.portNumber = header->sourcePortIdentity.portNumber;
	ptpPortDS->secondaryForeignMaster.foreignMasterAnnounceMessages = 0;

	/*header and announce field of each Foreign Master are usefull to run Best Master Clock Algorithm*/
	msgUnpackHeader(ptpPortDS->msgIbuf,&ptpPortDS->secondaryForeignMaster.header);
	msgUnpackAnnounce(ptpPortDS->msgIbuf,&ptpPortDS->secondaryForeignMaster.announce,&ptpPortDS->secondaryForeignMaster.header);

	PTPD_TRACE(TRACE_BMC, ptpPortDS,"Secondary foreign Master added/updated \n");

	ptpPortDS->secondaryForeignMaster.receptionPortNumber =  ptpPortDS->portIdentity.portNumber;
	
}

/*Copy local data set into header and announce message. 9.3.4 table 12*/
void copyD0(MsgHeader *header, MsgAnnounce *announce, PtpPortDS *ptpPortDS)
{
	PTPD_TRACE(TRACE_BMC, ptpPortDS,"[%s]\n",__func__);
  	announce->grandmasterPriority1 = ptpPortDS->ptpClockDS->priority1;
	memcpy(announce->grandmasterIdentity,ptpPortDS->clockIdentity,CLOCK_IDENTITY_LENGTH);
	announce->grandmasterClockQuality.clockClass = ptpPortDS->ptpClockDS->clockQuality.clockClass;
	announce->grandmasterClockQuality.clockAccuracy = ptpPortDS->ptpClockDS->clockQuality.clockAccuracy;
	announce->grandmasterClockQuality.offsetScaledLogVariance = ptpPortDS->ptpClockDS->clockQuality.offsetScaledLogVariance;
	announce->grandmasterPriority2 = ptpPortDS->ptpClockDS->priority2;
	announce->stepsRemoved = 0;
	memcpy(header->sourcePortIdentity.clockIdentity,ptpPortDS->clockIdentity,CLOCK_IDENTITY_LENGTH);

	/*White Rabbit*/
	announce->wr_flags = (announce->wr_flags | ptpPortDS->wrConfig) & WR_NODE_MODE  ;
	announce->wr_flags =  announce->wr_flags | ptpPortDS->calibrated << 2;
	announce->wr_flags =  announce->wr_flags | ptpPortDS->wrModeON     << 3;
}

/*
Data set comparison bewteen two foreign masters (9.3.4 fig 27)

return:
      A_better_by_topology_then_B 	(= -2)
      A_better_then_B 			(= -1)
      B_better_then_A 			(= 1)
      B_better_by_topology_then_A	(= 2)
      A_equals_B			(= 0)
      DSC_error				(= 0)

*/

Integer8 bmcDataSetComparison(MsgHeader *headerA, MsgAnnounce *announceA, UInteger16 receptionPortNumberA,
			      MsgHeader *headerB, MsgAnnounce *announceB, UInteger16 receptionPortNumberB,
			      PtpPortDS *ptpPortDS)
{
	/*
	 * The original implementation of BMC was modified:
	 * - entire BMC from IEEE1588 was implemented (before a simplified version was only implemented)
	 * - the WR modification (cosmetics) is included
	 *
	 * So, now, there is a full-blown modifiedBM, beware
	 *
	 */
	
	PTPD_TRACE(TRACE_BMC, ptpPortDS,"DCA: Data set comparison \n");
	
	short comp = 0;

	// the indexing of ports starts from 1, 0 indicates no port
	if(receptionPortNumberA == 0 && receptionPortNumberB == 0)
	{
	  PTPD_TRACE(TRACE_ERROR, ptpPortDS,"Data Set Comparison Alg ERROR: both data sets to be compared are empty !!!\n");
	  return DSC_error;
	}
	else if(receptionPortNumberB == 0)
	{
	  PTPD_TRACE(TRACE_BMC, ptpPortDS,"A better than B because B is empty !!!\n");
	  return A_better_then_B;
	}	
	else if(receptionPortNumberA == 0) 
	{
	  PTPD_TRACE(TRACE_BMC, ptpPortDS,"B better than A because A is empty !!! [in theory, this should not happen !!!\n");
	  return B_better_then_A;
	}


	/*Identity comparison*/
	if (!memcmp(announceA->grandmasterIdentity,announceB->grandmasterIdentity,CLOCK_IDENTITY_LENGTH)) // (1)
	{
		PTPD_TRACE(TRACE_BMC, ptpPortDS,"DCA: Grandmasters are identical\n");
		//Algorithm part2 Fig 28
		// compare steps removed of A and B
		if (announceA->stepsRemoved > announceB->stepsRemoved+1) //(2)
		{
		    PTPD_TRACE(TRACE_BMC, ptpPortDS,"DCA: .. B better than A \n");
		    return B_better_then_A;
		}
		else if (announceB->stepsRemoved > announceA->stepsRemoved+1) //(2)
		{
		    PTPD_TRACE(TRACE_BMC, ptpPortDS,"DCA: .. A better than B \n");
		    return A_better_then_B;
		}
		else //A within 1 of B
		{
			PTPD_TRACE(TRACE_BMC, ptpPortDS,"DCA: .. A within 1 of B \n");
			// Compare Steps Removed of A and B
			if (announceA->stepsRemoved > announceB->stepsRemoved) // (3)
			{
				PTPD_TRACE(TRACE_BMC, ptpPortDS,"DCA: .. .. A > B\n");
				
				/* Compare Identities of Receiver of A and Sender of A */
				
				if (!memcmp(headerA->sourcePortIdentity.clockIdentity,ptpPortDS->portIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH)) //(4)
				{
				    PTPD_TRACE(TRACE_BMC, ptpPortDS,"DCA: .. .. Comparing Identities of A: Sender=Receiver : Error -1");
				    return A_equals_B;
				}
				else if(memcmp(headerA->sourcePortIdentity.clockIdentity,ptpPortDS->portIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH) > 0)//(4)
				{
				  
				    PTPD_TRACE(TRACE_BMC, ptpPortDS,"DCA: .. .. .. B better than A (Comparing Identities of A: Receiver < Sender)\n");
				    return B_better_then_A;
				}
				else if(memcmp(headerA->sourcePortIdentity.clockIdentity,ptpPortDS->portIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH) < 0)//(4)
				{
				  
				    PTPD_TRACE(TRACE_BMC, ptpPortDS,"DCA: .. .. .. B better by topology than A (Comparing Identities of A: Receiver > Sender)\n");
				    return B_better_by_topology_then_A;
				}
				else 
				{
				   PTPD_TRACE(TRACE_BMC, ptpPortDS,"Impossible to get here \n");
				   return DSC_error;
				}

			}
			else if (announceB->stepsRemoved > announceA->stepsRemoved) //(3)
			{
				if (!memcmp(headerB->sourcePortIdentity.clockIdentity,ptpPortDS->portIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH))  //(5)
				{
				  	PTPD_TRACE(TRACE_BMC, ptpPortDS,"DCA: .. .. Comparing Identities of A: Sender=Receiver : Error -1");
					return A_equals_B;
				}
				else if(memcmp(headerB->sourcePortIdentity.clockIdentity,ptpPortDS->portIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH) > 0)//(5)
				{
				  
				    PTPD_TRACE(TRACE_BMC, ptpPortDS,"DCA: .. .. .. B better than A (Comparing Identities of A: Receiver < Sender)\n");
				    return A_better_then_B;
				}
				else if(memcmp(headerB->sourcePortIdentity.clockIdentity,ptpPortDS->portIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH) < 0)//(5)
				{
				  
				    PTPD_TRACE(TRACE_BMC, ptpPortDS,"DCA: .. .. .. A better by topology than B (Comparing Identities of A: Receiver > Sender)\n");
				    return A_better_by_topology_then_B;
				}
				else 
				{
				   PTPD_TRACE(TRACE_BMC, ptpPortDS,"Impossible to get here \n");
				   return DSC_error;
				}
			}
			else // steps removed A = steps removed B
			{
				PTPD_TRACE(TRACE_BMC, ptpPortDS,"DCA: .. steps removed A = steps removed B \n");
				// compare Identities of Senders of A and B
				if (!memcmp(headerA->sourcePortIdentity.clockIdentity,headerB->sourcePortIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH)) //(6)
				{				
					if(receptionPortNumberA < receptionPortNumberB) //(7)
					{
					    PTPD_TRACE(TRACE_BMC, ptpPortDS,"DCA: .. .. .. .. A better by topology than B (Compare Port Numbers of Receivers of A and B : A < B)\n");
					    return A_better_by_topology_then_B;
					}
					else if(receptionPortNumberA > receptionPortNumberB) //(7)
					{
					    PTPD_TRACE(TRACE_BMC, ptpPortDS,"DCA: .. .. .. .. B better by topology than A (Compare Port Numbers of Receivers of A and B : A > B)\n");
					    return B_better_by_topology_then_A;
					}
					else // receptionPortNumberA == receptionPortNumberB //(7)
					{
					    PTPD_TRACE(TRACE_BMC, ptpPortDS,"DCA: .. Sender=Receiver : Error -2");
					    return A_equals_B;
					}
					
				}
				else if ((memcmp(headerA->sourcePortIdentity.clockIdentity,headerB->sourcePortIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH))<0)
				{
					PTPD_TRACE(TRACE_BMC, ptpPortDS,"DCA: .. .. A better by topology than B \n");
					return A_better_by_topology_then_B;
				}
				else
				{	PTPD_TRACE(TRACE_BMC, ptpPortDS,"DCA: .. .. B better by topologythan A \n");
					return B_better_by_topology_then_A;
				}
			}

		}
	}
	else //GrandMaster are not identical
	{
		PTPD_TRACE(TRACE_BMC, ptpPortDS,"DCA: GrandMaster are not identical\n");
		if(announceA->grandmasterPriority1 == announceB->grandmasterPriority1)
		{
			PTPD_TRACE(TRACE_BMC, ptpPortDS,"DCA: .. A->Priority1 == B->Priority1 == %d\n",announceB->grandmasterPriority1);
			if (announceA->grandmasterClockQuality.clockClass == announceB->grandmasterClockQuality.clockClass)
			{
				PTPD_TRACE(TRACE_BMC, ptpPortDS,"DCA: .. .. A->clockClass == B->clockClass == %d\n",announceB->grandmasterClockQuality.clockClass);
				if (announceA->grandmasterClockQuality.clockAccuracy == announceB->grandmasterClockQuality.clockAccuracy)
				{
					PTPD_TRACE(TRACE_BMC, ptpPortDS,"DCA: .. .. .. A->clockAccuracy == B->clockAccuracy == %d\n",announceB->grandmasterClockQuality.offsetScaledLogVariance);
					if (announceA->grandmasterClockQuality.offsetScaledLogVariance == announceB->grandmasterClockQuality.offsetScaledLogVariance)
					{
						PTPD_TRACE(TRACE_BMC, ptpPortDS,"DCA: .. .. .. .. A->offsetScaledLogVariance == B->offsetScaledLogVariance == %d\n",announceB->grandmasterClockQuality.offsetScaledLogVariance);
						if (announceA->grandmasterPriority2 == announceB->grandmasterPriority2)
						{
							PTPD_TRACE(TRACE_BMC, ptpPortDS,"DCA: .. .. .. .. .. A->Priority2 == B->Priority2 == %d\n",announceB->grandmasterPriority2);
							comp = memcmp(announceA->grandmasterIdentity,announceB->grandmasterIdentity,CLOCK_IDENTITY_LENGTH);
							if (comp < 0)
							{
								PTPD_TRACE(TRACE_BMC, ptpPortDS,"DCA: .. .. .. .. .. .. A better than B by GrandMaster ID\n");
								return A_better_then_B;
							}
							else if (comp > 0)
							{
								PTPD_TRACE(TRACE_BMC, ptpPortDS,"DCA: .. .. .. .. .. .. B better than A GrandMaster ID\n");
								return B_better_then_A;
							}
							else
							{
								PTPD_TRACE(TRACE_BMC, ptpPortDS,"DCA: .. .. .. .. .. .. not good: GrandMaster ID [should not get here]\n");
								return DSC_error;
							}
						}
						else //Priority2 are not identical
						{
							comp =memcmp(&announceA->grandmasterPriority2,&announceB->grandmasterPriority2,1);
							if (comp < 0)
							{
								PTPD_TRACE(TRACE_BMC, ptpPortDS,"DCA: .. .. .. .. .. A better than B by Priority2\n");
								return A_better_then_B;
							}
							else if (comp > 0)
							{
								PTPD_TRACE(TRACE_BMC, ptpPortDS,"DCA: .. .. .. .. .. B better than A by Priority2\n");
								return B_better_then_A;
							}
							else
							{
								PTPD_TRACE(TRACE_BMC, ptpPortDS,"DCA: .. .. .. .. .. not good: Priority2\n");
								return DSC_error;
							}
						}
					}

					else //offsetScaledLogVariance are not identical
					{
						comp= memcmp(&announceA->grandmasterClockQuality.offsetScaledLogVariance,&announceB->grandmasterClockQuality.offsetScaledLogVariance,1);
						if (comp < 0)
						{
							PTPD_TRACE(TRACE_BMC, ptpPortDS,"DCA: .. .. .. .. A better than B by offsetScaledLogVariance\n");
							return A_better_then_B;
						}
						else if (comp > 0)
						{
							PTPD_TRACE(TRACE_BMC, ptpPortDS,"DCA: .. .. .. .. B better than A by offsetScaledLogVariance\n");
							return B_better_then_A;
						}
						else
						{	PTPD_TRACE(TRACE_BMC, ptpPortDS,"DCA: .. .. .. .. not good: offsetScaledLogVariance\n");
							return DSC_error;
						}
					}

				}

				else // Accuracy are not identitcal
				{
					comp = memcmp(&announceA->grandmasterClockQuality.clockAccuracy,&announceB->grandmasterClockQuality.clockAccuracy,1);
					if (comp < 0)
					{
						PTPD_TRACE(TRACE_BMC, ptpPortDS,"DCA: .. .. .. A better than B by clockAccuracy\n");
						return A_better_then_B;
					}
					else if (comp > 0)
					{
						PTPD_TRACE(TRACE_BMC, ptpPortDS,"DCA: .. .. .. B better than A by clockAccuracy\n");
						return B_better_then_A;
					}
					else
					{	
						PTPD_TRACE(TRACE_BMC, ptpPortDS,"DCA: .. .. .. not good: clockAccuracy\n");
						return DSC_error;
					}
				}

			}

			else //ClockClass are not identical
			{
				comp =  memcmp(&announceA->grandmasterClockQuality.clockClass,&announceB->grandmasterClockQuality.clockClass,1);
				if (comp < 0)
				{
					PTPD_TRACE(TRACE_BMC, ptpPortDS,"DCA: .. .. A better than B by clockClass\n");
					return A_better_then_B;
				}
				else if (comp > 0)
				{
					PTPD_TRACE(TRACE_BMC, ptpPortDS,"DCA: .. .. B better than A by clockClass\n");
					return B_better_then_A;
				}
				else
				{
					PTPD_TRACE(TRACE_BMC, ptpPortDS,"DCA: .. .. not good:  clockClass\n");
					return DSC_error;
				}
			}
		}

		else // Priority1 are not identical
		{
			comp =  memcmp(&announceA->grandmasterPriority1,&announceB->grandmasterPriority1,1);
			if (comp < 0)
			{
				PTPD_TRACE(TRACE_BMC, ptpPortDS,"DCA: .. A better than B by Priority1\n");
				return A_better_then_B;
			}
			else if (comp > 0)
			{
				PTPD_TRACE(TRACE_BMC, ptpPortDS,"DCA: .. B better than A by Priority1\n");
				return B_better_then_A;
			}
			else
			{
				PTPD_TRACE(TRACE_BMC, ptpPortDS,"DCA: .. not good: Priority1\n");
				return DSC_error;
			}
		}

	}

}

/*
State decision algorithm 9.3.3 Fig 26

return: recommended state
*/
UInteger8 bmcStateDecision (MsgHeader *header,MsgAnnounce *announce, UInteger16 receptionPortNumber, RunTimeOpts *rtOpts,PtpPortDS *ptpPortDS)
{
	Integer8 comp;
	
	PTPD_TRACE(TRACE_BMC, ptpPortDS,"SDA: State Decision Algorith,\n");
	if (ptpPortDS->wrConfig == WR_S_ONLY)
	{
		PTPD_TRACE(TRACE_BMC, ptpPortDS,"SDA: .. Slave Only Mode: PTP_SLAVE\n");
		s1(header,announce,ptpPortDS);

		  return PTP_SLAVE;
	}

	if (ptpPortDS->wrConfig == WR_M_ONLY)
	{
		PTPD_TRACE(TRACE_BMC, ptpPortDS,"SDA: .. Master Only Mode: PTP_MASTER\n");
		m1(ptpPortDS); 
		return PTP_MASTER;
	}

	if ((!ptpPortDS->number_foreign_records) && (ptpPortDS->portState == PTP_LISTENING)) //(2)
	{
		PTPD_TRACE(TRACE_BMC, ptpPortDS,"SDA: .. No foreing nasters : PTP_LISTENING [3]\n");
		return PTP_LISTENING;
	}
	
	copyD0(&ptpPortDS->msgTmpHeader,&ptpPortDS->msgTmp.announce,ptpPortDS);

	if (ptpPortDS->ptpClockDS->clockQuality.clockClass < 128) // (4)
	{
		PTPD_TRACE(TRACE_BMC, ptpPortDS,"SDA: .. clockClass < 128\n");
		
		comp =  bmcDataSetComparison(&ptpPortDS->msgTmpHeader,
					     &ptpPortDS->msgTmp.announce,
					      ptpPortDS->portIdentity.portNumber,
					      header,announce,receptionPortNumber,ptpPortDS);
		
		if (comp < 0) // (5): better or better by to topology
		{
			PTPD_TRACE(TRACE_BMC, ptpPortDS,"SDA: .. .. D0 Better or better by topology then Ebest: YES => m1(): PTP_MASTER [7]\n");
			m1(ptpPortDS); //(7)
			return PTP_MASTER;
		}
		else if (comp>0) //better or better by to topology
		{
			PTPD_TRACE(TRACE_BMC, ptpPortDS,"SDA: .. .. D0 Better or better by topology then Ebest: NO => p1(): =>> PTP_PASSIVE[8]\n");
			
			//s2(header,announce,ptpPortDS); //(8)
			p1(ptpPortDS);
			
			return PTP_PASSIVE;
		}
		else
		{
			PTPD_TRACE(TRACE_BMC, ptpPortDS,"SDA: .. .. Error in bmcDataSetComparison..\n");
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
		PTPD_TRACE(TRACE_BMC, ptpPortDS,"SDA: .. clockClass > 128\n");
		/* compare  D0 with Ebest */
		
		comp =  bmcDataSetComparison(&ptpPortDS->msgTmpHeader,
					     &ptpPortDS->msgTmp.announce,
					      ptpPortDS->portIdentity.portNumber,
					     &ptpPortDS->ptpClockDS->bestForeign->header,
					     &ptpPortDS->ptpClockDS->bestForeign->announce,
					      ptpPortDS->ptpClockDS->bestForeign->receptionPortNumber,
					      ptpPortDS);
		
		if ((comp < 0))	// (6) better or better by to topology	
		{		
			PTPD_TRACE(TRACE_BMC, ptpPortDS,"SDA: .. .. D0 Better or better by topology then Ebest: YES => m1(): PTP_MASTER [9]\n");
			m1(ptpPortDS);//(9) actually, m2(), but it's the same as m1()
			return PTP_MASTER;
		}
		else if (comp>0) //better or better by to topology
		{
			PTPD_TRACE(TRACE_BMC, ptpPortDS,"SDA: .. .. D0 Better or better by topology then Ebest: NO\n");
			if(ptpPortDS->ptpClockDS->bestForeign->receptionPortNumber == ptpPortDS->portIdentity.portNumber) //(10)
			{
				
				PTPD_TRACE(TRACE_BMC, ptpPortDS,"SDA: .. .. .. Ebest received on port r (=%d): YES => s1(): PTP_SLAVE [11]\n",ptpPortDS->ptpClockDS->bestForeign->receptionPortNumber);
				s1(header,announce,ptpPortDS); //(11)
				return PTP_SLAVE;
			}
			else
			{
				PTPD_TRACE(TRACE_BMC, ptpPortDS,"SDA: .. .. .. Ebest received on port r (foreign_receivd_on=%d,current_port=%d ): NO \n", \
				ptpPortDS->ptpClockDS->bestForeign->receptionPortNumber, ptpPortDS->portIdentity.portNumber);
				
				comp = bmcDataSetComparison(&ptpPortDS->ptpClockDS->bestForeign->header,
							    &ptpPortDS->ptpClockDS->bestForeign->announce,
							     ptpPortDS->ptpClockDS->bestForeign->receptionPortNumber,
							     header,announce,receptionPortNumber,ptpPortDS);
				
				if (comp == A_better_by_topology_then_B)	//(12): better by topology
				{		
					PTPD_TRACE(TRACE_BMC, ptpPortDS,"SDA: .. .. .. .. Ebest better by topology (ONLY !!!) then Erbest: YES => s2()  PTP_SLAVE [modifiedBMC][13]\n");
					s2(header,announce,ptpPortDS); // (13)
					return PTP_SLAVE;
				}
				else if (comp != A_better_by_topology_then_B)	//better by topology
				{
					PTPD_TRACE(TRACE_BMC, ptpPortDS,"SDA: .. .. .. .. Ebest better by topology (ONLY !!) then Erbest: NO (it means it can be better) => m3() PTP_MASTER [14] \n");
					m3(ptpPortDS); // (14)
					return PTP_MASTER;			
				}
				else
				{
					PTPD_TRACE(TRACE_BMC, ptpPortDS,"SDA: .. .. Error in bmcDataSetComparison..\n");
				}				  
			}

		}
		else
		{
			PTPD_TRACE(TRACE_BMC, ptpPortDS,"SDA: .. .. Error in bmcDataSetComparison..\n");
		}

	}
	return PTP_PASSIVE; /* only reached in error condition */

}
/* 
modified Best Master Clock (modifiedBMC) algorithm as specified in ptp 9.3.2 and wrspec 6.4

return: recommended state
*/
UInteger8 bmc(ForeignMasterRecord *foreignMaster,RunTimeOpts *rtOpts ,PtpPortDS *ptpPortDS )
{
	PTPD_TRACE(TRACE_BMC, ptpPortDS,"BMC: Best Master Clock Algorithm @ working\n");
	Integer16 best;

	//TODO (3): check what happens when all/Ebest is removed, maybe needs some extra cleaning
	if (ptpPortDS->ptpClockDS->Ebest < 0) // no foreignMasters in entire ptpClock (all ports)
	{
		PTPD_TRACE(TRACE_BMC, ptpPortDS,"BMC: .. no foreign masters\n");
		if (ptpPortDS->portState == PTP_MASTER)
		{
			PTPD_TRACE(TRACE_BMC, ptpPortDS,"BMC: .. .. m1(): PTP_MASTER\n");
			m1(ptpPortDS);
			return ptpPortDS->portState;
		}
	}

	best = ptpPortDS->foreign_record_best;

	return bmcStateDecision(&foreignMaster[best].header,&foreignMaster[best].announce, \
		foreignMaster[best].receptionPortNumber, rtOpts,ptpPortDS);
}

/*
calculate ErBest (best foreign master) for a particular port

return: index of the best Foreign Master in the foreignMaster table for the port
*/
UInteger8 ErBest(ForeignMasterRecord *foreignMaster,PtpPortDS *ptpPortDS )
{
	Integer16 i,best;
	
	PTPD_TRACE(TRACE_BMC, ptpPortDS,"ErBest - looking for the best ForeignMaster for a port=%d\n",ptpPortDS->portIdentity.portNumber);
	
	if (!ptpPortDS->number_foreign_records)
	{
	    //nothing to look for
	    ptpPortDS->foreign_record_best = 0;
	    return -1;
	}

	//go through all foreign masters and compare, find the best one
	for (i=1,best = 0; i<ptpPortDS->number_foreign_records;i++)
	{
		PTPD_TRACE(TRACE_BMC, ptpPortDS,"BMC: .. looking at %d foreign master\n",i);
		if ((bmcDataSetComparison(&foreignMaster[i].header,
					  &foreignMaster[i].announce,
					   foreignMaster[i].receptionPortNumber,
					  &foreignMaster[best].header,
					  &foreignMaster[best].announce,
					   foreignMaster[best].receptionPortNumber,
					  ptpPortDS)) < 0)
		{
			PTPD_TRACE(TRACE_BMC, ptpPortDS,"BMC: .. .. update currently best (%d) to new best = %d\n",best, i);
			best = i;
		}
	}

 	PTPD_TRACE(TRACE_BMC, ptpPortDS,"ErBest: the best foreign master for port = %d is indexed = %d received on port = %d\n",\
 		 ptpPortDS->portIdentity.portNumber, best, foreignMaster[best].receptionPortNumber);
		
	ptpPortDS->foreign_record_best = best;

	return best;
}
/* 

Finds the globally (for entire Boundary Clock) best foreign master. This data is 
remembered in ptpClockDS: Ebest (port index) and bestForeign (pointer to the record)

return: index of the port on which the best foreignMaster record is located, 
	so the globally best foreign master can be found by reading the best record 
	(index: foreign_record_best) of the port with the index of the returned value.

*/
UInteger8 EBest(PtpPortDS *ptpPortDS )
{
	Integer16 i;
	Integer16 Ebest;
	Integer16 ERbest_i;
	Integer16 ERbest_b;	
  
	PTPD_TRACE(TRACE_BMC, ptpPortDS,"EBest - looking for the best ForeignMaster for all ports\n");
	
	//look for the first port with non-empty foreign master records
	for (Ebest=0; Ebest < ptpPortDS->ptpClockDS->numberPorts; Ebest++)
	{
		if(ptpPortDS[Ebest].number_foreign_records > 0)
			break;
	}
	
	// compare the Erbest (best for the port) foreign masters
	for (i= Ebest + 1; i < ptpPortDS->ptpClockDS->numberPorts; i++)
	{
	  
		if(ptpPortDS[i].number_foreign_records == 0)
			continue;
		
		ERbest_i 	= ptpPortDS[i].foreign_record_best;
		ERbest_b	= ptpPortDS[Ebest].foreign_record_best;
		
		PTPD_TRACE(TRACE_BMC, ptpPortDS,"BMC: .. looking at %d foreign master\n",i);
		if ((bmcDataSetComparison(&ptpPortDS[i].foreign[ERbest_i].header,   	\
					  &ptpPortDS[i].foreign[ERbest_i].announce, 	\
					   ptpPortDS[i].foreign[ERbest_i].receptionPortNumber, 	\
					  &ptpPortDS[Ebest].foreign[ERbest_b].header,	\
					  &ptpPortDS[Ebest].foreign[ERbest_b].announce,	\
					   ptpPortDS[Ebest].foreign[ERbest_b].receptionPortNumber, 	\
					   ptpPortDS)) < 0)
		{
			PTPD_TRACE(TRACE_BMC, ptpPortDS,"BMC: .. .. update currently best (%d) to new best = %d\n",Ebest, i);
			Ebest = i;
		}
	}
	//remember the index of the port with the best foreign master record
	ptpPortDS->ptpClockDS->Ebest = Ebest;
	
	ERbest_b = ptpPortDS[Ebest].foreign_record_best;
	
	//remember the pointer to Ebest globally
	ptpPortDS->ptpClockDS->bestForeign = &ptpPortDS[Ebest].foreign[ERbest_b];
	
	PTPD_TRACE(TRACE_BMC, ptpPortDS,"Ebest: the port with the best foreign master number=%d, the foreign master record number=%d\n",\
		ptpPortDS[Ebest].foreign[ERbest_b].receptionPortNumber ,Ebest);

	return Ebest;
}
