#include "ptpd.h"

#ifndef WRPC_EXTRA_SLIM

/**\brief Display an Integer64 type*/
void integer64_display (Integer64 *bigint){
	printf("Integer 64 : \n");
	printf("LSB : %u\n",bigint->lsb);
	printf("MSB : %d\n",bigint->msb);
}

/**\brief Display an UInteger48 type*/
void uInteger48_display(UInteger48 *bigint){
	printf("Integer 48 : \n");
	printf("LSB : %u\n",bigint->lsb);
	printf("MSB : %u\n",bigint->msb);
}

/** \brief Display a TimeInternal Structure*/
void timeInternal_display(TimeInternal *timeInternal) {
	printf("seconds : %d \n",timeInternal->seconds);
	printf("nanoseconds %d \n",timeInternal->nanoseconds);
}

/** \brief Display a Timestamp Structure*/
void timestamp_display(Timestamp *timestamp) {
	uInteger48_display(&timestamp->secondsField);
	printf("nanoseconds %u \n",timestamp->nanosecondsField);
}

/**\brief Display a Clockidentity Structure*/
void clockIdentity_display(ClockIdentity clockIdentity){

printf(
    "ClockIdentity : %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n",
    clockIdentity[0], clockIdentity[1], clockIdentity[2],
    clockIdentity[3], clockIdentity[4], clockIdentity[5],
    clockIdentity[6],clockIdentity[7]
);

}

/**\brief Display MAC address*/
void clockUUID_display(Octet *sourceUuid){

printf(
    "sourceUuid %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n",
    sourceUuid[0], sourceUuid[1], sourceUuid[2],
    sourceUuid[3], sourceUuid[4], sourceUuid[5]
);

}


/**\brief Display Network info*/
void netPath_display(NetPath *net)
{
#if 0
	struct in_addr addr;

	printf("eventSock : %d \n",net->eventSock);
	printf("generalSock : %d \n",net->generalSock);

	addr.s_addr=net->multicastAddr;
	printf("multicastAdress : %s \n",inet_ntoa(addr));
	addr.s_addr=net->peerMulticastAddr;
	printf("peerMulticastAddress : %s \n",inet_ntoa(addr));
	addr.s_addr=net->unicastAddr;
	printf("unicastAddress : %s \n",inet_ntoa(addr));
#endif
}

/**\brief Display a IntervalTimer Structure*/
void intervalTimer_display(IntervalTimer *ptimer)
{
	uint64_t tics;

	tics = ptpd_netif_get_msec_tics();

	printf("interval : %d \n",ptimer->interval);
	printf("left : %d \n", (int) (tics + ptimer->interval - ptimer->t_start));
	printf("expire : %d \n",(int) (tics - ptimer->t_start));
}




/**\brief Display a TimeInterval Structure*/
void timeInterval_display(TimeInterval *timeInterval){
	integer64_display(&timeInterval->scaledNanoseconds);
}


/**\brief Display a Portidentity Structure*/
void portIdentity_display(PortIdentity *portIdentity){
	clockIdentity_display((char*)portIdentity->clockIdentity);
	printf("port number : %d \n",portIdentity->portNumber);

}

/**\brief Display a Clockquality Structure*/
void clockQuality_display (ClockQuality *clockQuality){
	printf("clockClass : %d \n",clockQuality->clockClass);
	printf("clockAccuracy : %d \n",clockQuality->clockAccuracy);
	printf("offsetScaledLogVariance : %d \n",clockQuality->offsetScaledLogVariance);
}


/**\brief Display the Network Interface Name*/
void iFaceName_display(Octet *iFaceName){

  //TODO: ifaceName[?]
int i ;
printf("iFaceName : ");

for (i=0;i<IFACE_NAME_LENGTH;i++){
	printf("%c",iFaceName[i]);
}
printf("\n");

}

/**\brief Display an Unicast Adress*/
void unicast_display(Octet *unicast){

int i ;
printf("Unicast adress : ");

for (i=0;i<NET_ADDRESS_LENGTH;i++){
	printf("%c",unicast[i]);
}
printf("\n");

}


/**\brief Display Sync message*/
void msgSync_display(MsgSync *sync){
	printf("Message Sync : \n");
	printf("\n");
	timestamp_display(&sync->originTimestamp);
	printf("\n");
}

/**\brief Display Header message*/
void msgHeader_display(MsgHeader *header){
	printf("Message header : \n");
	printf("\n");
 	printf("transportSpecific : %d\n",header->transportSpecific);
 	printf("messageType : %d\n",header->messageType);
 	printf("versionPTP : %d\n",header->versionPTP);
 	printf("messageLength : %d\n",header->messageLength);
 	printf("domainNumber : %d\n",header->domainNumber);
 	printf("FlagField %02hhx:%02hhx\n",header->flagField[0],header->flagField[1]);
 	integer64_display(&header->correctionfield);
	portIdentity_display(&header->sourcePortIdentity);
	printf("sequenceId : %d\n",header->sequenceId);
	printf("controlField : %d\n",header->controlField);
	printf("logMessageInterval : %d\n",header->logMessageInterval);
	printf("\n");
}

/**\brief Display Announce message*/
void msgAnnounce_display(MsgAnnounce *announce){
	printf("Announce Message : \n");
	printf("\n");
	printf("originTimestamp : \n");
	printf("secondField  : \n");
	timestamp_display(&announce->originTimestamp);
	printf("currentUtcOffset : %d \n",announce->currentUtcOffset);
	printf("grandMasterPriority1 : %d \n",announce->grandmasterPriority1);
	printf("grandMasterClockQuality : \n");
	clockQuality_display(&announce->grandmasterClockQuality);
	printf("grandMasterPriority2 : %d \n",announce->grandmasterPriority2);
	printf("grandMasterIdentity : \n");
	clockIdentity_display(announce->grandmasterIdentity);
	printf("stepsRemoved : %d \n",announce->stepsRemoved);
	printf("timeSource : %d \n",announce->timeSource);
	printf("\n");
}

/**\brief Display Follow_UP message*/
void msgFollowUp_display(MsgFollowUp *follow){
	timestamp_display(&follow->preciseOriginTimestamp);
}


/**\brief Display Pdelay_Req message*/
void msgPDelayReq_display(MsgPDelayReq *preq){

timestamp_display(&preq->originTimestamp);
}

/**\brief Display Pdelay_Resp message*/
void msgPDelayResp_display(MsgPDelayResp *presp){

timestamp_display(&presp->requestReceiptTimestamp);
portIdentity_display(&presp->requestingPortIdentity);
}

/**\brief Display Pdelay_Resp Follow Up message*/
void msgPDelayRespFollowUp_display(MsgPDelayRespFollowUp *prespfollow){

timestamp_display(&prespfollow->responseOriginTimestamp);
portIdentity_display(&prespfollow->requestingPortIdentity);
}

/**\brief Display runTimeOptions structure*/
void displayRunTimeOpts(RunTimeOpts* rtOpts){

printf("---Run time Options Display-- \n");
printf("\n");
printf("announceInterval : %d \n",rtOpts->announceInterval);
printf("syncInterval : %d \n",rtOpts->syncInterval);
clockQuality_display(&(rtOpts->clockQuality));
printf("priority1 : %d \n",rtOpts->priority1);
printf("priority2 : %d \n",rtOpts->priority2);
printf("domainNumber : %d \n",rtOpts->domainNumber);
printf("currentUtcOffset : %d \n",rtOpts->currentUtcOffset);
unicast_display(rtOpts->unicastAddress);
printf("noResetClock : %d \n",rtOpts->noResetClock);
printf("noAdjust : %d \n",rtOpts->noAdjust);
printf("displayStats : %d \n",rtOpts->displayStats);
printf("csvStats : %d \n",rtOpts->csvStats);
iFaceName_display(rtOpts->ifaceName[0]);
printf("ap : %d \n",rtOpts->ap);
printf("aI : %d \n",rtOpts->ai);
printf("s : %d \n",rtOpts->s);
printf("inbound latency : \n");
timeInternal_display(&(rtOpts->inboundLatency));
printf("outbound latency : \n");
timeInternal_display(&(rtOpts->outboundLatency));
printf("max_foreign_records : %d \n",rtOpts->max_foreign_records);
printf("ethernet mode : %d \n",rtOpts->ethernet_mode);
printf("\n");
}


/**\brief Display Default data set of a PtpPortDS*/
void displayDefault (PtpPortDS *ptpPortDS){

printf("---Ptp Clock Default Data Set-- \n");
printf("\n");
printf("twoStepFlag : %d \n",ptpPortDS->ptpClockDS->twoStepFlag);
clockIdentity_display(ptpPortDS->clockIdentity);
printf("numberPorts : %d \n",ptpPortDS->ptpClockDS->numberPorts);
clockQuality_display(&(ptpPortDS->ptpClockDS->clockQuality));
printf("priority1 : %d \n",ptpPortDS->ptpClockDS->priority1);
printf("priority2 : %d \n",ptpPortDS->ptpClockDS->priority2);
printf("domainNumber : %d \n",ptpPortDS->ptpClockDS->domainNumber);
printf("\n");
}


/**\brief Display Current data set of a PtpPortDS*/
void displayCurrent (PtpPortDS *ptpPortDS){

printf("---Ptp Clock Current Data Set-- \n");
printf("\n");

printf("stepsremoved : %d \n",ptpPortDS->ptpClockDS->stepsRemoved);
printf("Offset from master : \n");
timeInternal_display(&ptpPortDS->ptpClockDS->offsetFromMaster);
printf("Mean path delay : \n");
timeInternal_display(&ptpPortDS->ptpClockDS->meanPathDelay);
printf("\n");
}



/**\brief Display Parent data set of a PtpPortDS*/
void displayParent (PtpPortDS *ptpPortDS){

printf("---Ptp Clock Parent Data Set-- \n");
printf("\n");
portIdentity_display(&(ptpPortDS->ptpClockDS->parentPortIdentity));
printf("parentStats : %d \n",ptpPortDS->ptpClockDS->parentStats);
printf("observedParentOffsetScaledLogVariance : %d \n",ptpPortDS->ptpClockDS->observedParentOffsetScaledLogVariance);
printf("observedParentClockPhaseChangeRate : %d \n",ptpPortDS->ptpClockDS->observedParentClockPhaseChangeRate);
printf("--GrandMaster--\n");
clockIdentity_display(ptpPortDS->ptpClockDS->grandmasterIdentity);
clockQuality_display(&ptpPortDS->ptpClockDS->grandmasterClockQuality);
printf("grandmasterpriority1 : %d \n",ptpPortDS->ptpClockDS->grandmasterPriority1);
printf("grandmasterpriority2 : %d \n",ptpPortDS->ptpClockDS->grandmasterPriority2);
printf("\n");
}

/**\brief Display Global data set of a PtpPortDS*/
void displayGlobal (PtpPortDS *ptpPortDS){

printf("---Ptp Clock Global Time Data Set-- \n");
printf("\n");

printf("currentUtcOffset : %d \n",ptpPortDS->ptpClockDS->currentUtcOffset);
printf("currentUtcOffsetValid : %d \n",ptpPortDS->ptpClockDS->currentUtcOffsetValid);
printf("leap59 : %d \n",ptpPortDS->ptpClockDS->leap59);
printf("leap61 : %d \n",ptpPortDS->ptpClockDS->leap61);
printf("timeTraceable : %d \n",ptpPortDS->ptpClockDS->timeTraceable);
printf("frequencyTraceable : %d \n",ptpPortDS->ptpClockDS->frequencyTraceable);
printf("ptpTimescale : %d \n",ptpPortDS->ptpClockDS->ptpTimescale);
printf("timeSource : %d \n",ptpPortDS->ptpClockDS->timeSource);
printf("\n");
}

/**\brief Display Port data set of a PtpPortDS*/
void displayPort (PtpPortDS *ptpPortDS){

printf("---Ptp Clock Port Data Set-- \n");
printf("\n");

portIdentity_display(&ptpPortDS->portIdentity);
printf("port state : %d \n",ptpPortDS->portState);
printf("logMinDelayReqInterval : %d \n",ptpPortDS->logMinDelayReqInterval);
printf("peerMeanPathDelay : \n");
timeInternal_display(&ptpPortDS->peerMeanPathDelay);
printf("logAnnounceInterval : %d \n",ptpPortDS->logAnnounceInterval);
printf("announceReceiptTimeout : %d \n",ptpPortDS->announceReceiptTimeout);
printf("logSyncInterval : %d \n",ptpPortDS->logSyncInterval);
printf("delayMechanism : %d \n",ptpPortDS->delayMechanism);
printf("logMinPdelayReqInterval : %d \n",ptpPortDS->logMinPdelayReqInterval);
printf("versionNumber : %d \n",ptpPortDS->versionNumber);
printf("\n");
}

/**\brief Display ForeignMaster data set of a PtpPortDS*/
void displayForeignMaster (PtpPortDS *ptpPortDS){

	ForeignMasterRecord *foreign;
	int i;

	if (ptpPortDS->number_foreign_records > 0){

		printf("---Ptp Clock Foreign Data Set-- \n");
		printf("\n");
		printf("There is %d Foreign master Recorded \n",ptpPortDS->number_foreign_records);
		foreign = ptpPortDS->foreign;

			for (i=0;i<ptpPortDS->number_foreign_records;i++){

				portIdentity_display(&foreign->foreignMasterPortIdentity);
				printf("number of Announce message received : %d \n",foreign->foreignMasterAnnounceMessages);
				msgHeader_display(&foreign->header);
				msgAnnounce_display(&foreign->announce);

				foreign++;
			}

	}

	else
	{printf("No Foreign masters recorded \n");}

	printf("\n");


}

/**\brief Display other data set of a PtpPortDS*/

void displayOthers (PtpPortDS *ptpPortDS){

printf("---Ptp Others Data Set-- \n");
printf("\n");
printf("master_to_slave_delay : \n");
timeInternal_display(&ptpPortDS->master_to_slave_delay);
printf("\n");
printf("slave_to_master_delay : \n");
timeInternal_display(&ptpPortDS->slave_to_master_delay);
printf("\n");
printf("delay_req_receive_time : \n");
timeInternal_display(&ptpPortDS->pdelay_req_receive_time);
printf("\n");
printf("delay_req_send_time : \n");
timeInternal_display(&ptpPortDS->pdelay_req_send_time);
printf("\n");
printf("delay_resp_receive_time : \n");
timeInternal_display(&ptpPortDS->pdelay_resp_receive_time);
printf("\n");
printf("delay_resp_send_time : \n");
timeInternal_display(&ptpPortDS->pdelay_resp_send_time);
printf("\n");
printf("sync_receive_time : \n");
timeInternal_display(&ptpPortDS->sync_receive_time);
printf("\n");
//printf("R : %f \n",ptpPortDS->R);
printf("sentPdelayReq : %d \n",ptpPortDS->sentPDelayReq);
printf("sentPDelayReqSequenceId : %d \n",ptpPortDS->sentPDelayReqSequenceId);
printf("waitingForFollow : %d \n",ptpPortDS->waitingForFollow);
printf("\n");
printf("Offset from master filter : \n");
printf("nsec_prev : %d \n",ptpPortDS->ofm_filt.nsec_prev);
printf("y : %d \n",ptpPortDS->ofm_filt.y);
printf("\n");
printf("One way delay filter : \n");
printf("nsec_prev : %d \n",ptpPortDS->owd_filt.nsec_prev);
printf("y : %d \n",ptpPortDS->owd_filt.y);
printf("s_exp : %d \n",ptpPortDS->owd_filt.s_exp);
printf("\n");
printf("observed_drift : %d \n",ptpPortDS->observed_drift);
printf("message activity %d \n",ptpPortDS->message_activity);
printf("\n");

#if 0
for (i=0;i<TIMER_ARRAY_SIZE;i++){
	printf("%s : \n",timer[i]);
	intervalTimer_display(&ptpPortDS->itimer[i]);
	printf("\n");
}
#endif

netPath_display(&ptpPortDS->netPath);
printf("mCommunication technology %d \n",ptpPortDS->port_communication_technology);
clockUUID_display(ptpPortDS->port_uuid_field);
printf("\n");
}


/**\brief Display Buffer in & out of a PtpPortDS*/
void displayBuffer (PtpPortDS *ptpPortDS){

	int i;
	int j;
	j=0;

	printf("PtpPortDS Buffer Out  \n");
	printf("\n");

	for (i=0;i<PACKET_SIZE;i++){
		printf(":%02hhx",ptpPortDS->msgObuf[i]);
		j++;

		if (j==8){
			printf(" ");

		}

		if (j==16){
			printf("\n");
			j=0;
		}
	}
 printf("\n");
 j=0;
 printf("\n");

	printf("PtpPortDS Buffer In  \n");
	printf("\n");
	for (i=0;i<PACKET_SIZE;i++){
		printf(":%02hhx",ptpPortDS->msgIbuf[i]);
		j++;

		if (j==8){
			printf(" ");

		}

		if (j==16){
			printf("\n");
			j=0;
		}
	}
	printf("\n");
	printf("\n");
}




/**\brief Display All data set of a PtpPortDS*/
void displayPtpPortDS (PtpPortDS *ptpPortDS){

displayDefault (ptpPortDS);
displayCurrent (ptpPortDS);
displayParent (ptpPortDS);
displayGlobal(ptpPortDS);
displayPort(ptpPortDS);
displayForeignMaster(ptpPortDS);
displayBuffer(ptpPortDS);
displayOthers (ptpPortDS);

}

void displayConfigINFO(RunTimeOpts* rtOpts)
{

    int i;
   /* White rabbit debugging info*/
    PTPD_TRACE(TRACE_PTPD_MAIN, NULL,"------------- INFO ----------------------\n\n");
    if(rtOpts->E2E_mode)
      PTPD_TRACE(TRACE_PTPD_MAIN, NULL,"E2E_mode ........................ TRUE\n")
    else
      PTPD_TRACE(TRACE_PTPD_MAIN, NULL,"P2P_mode ........................ TRUE\n")

    PTPD_TRACE(TRACE_PTPD_MAIN, NULL,"portNumber  ..................... %d\n",rtOpts->portNumber);

    if(rtOpts->portNumber == 1 && rtOpts->autoPortDiscovery == FALSE)
     PTPD_TRACE(TRACE_PTPD_MAIN, NULL,"running as ...................... single port node (forced, no auto port number discovery)\n")
    else if(rtOpts->portNumber == 1 && rtOpts->autoPortDiscovery == TRUE)
     PTPD_TRACE(TRACE_PTPD_MAIN, NULL,"running as ...................... single port node (auto port number discovery)\n")
    else if(rtOpts->portNumber > 1 && rtOpts->autoPortDiscovery == TRUE)
     PTPD_TRACE(TRACE_PTPD_MAIN, NULL,"running as ....................... multi port node [%d] (auto port number discovery)\n",rtOpts->portNumber )
    else if(rtOpts->portNumber > 1 && rtOpts->autoPortDiscovery == FALSE)
     PTPD_TRACE(TRACE_PTPD_MAIN, NULL,"running as ....................... multi port node [%d] (forced, no auto port number discovery)\n",rtOpts->portNumber )
    else
     PTPD_TRACE(TRACE_PTPD_MAIN, NULL,"running as ....................... ERROR,should not get here\n")

    for(i = 0; i < rtOpts->portNumber; i++)
    {
      if(rtOpts->autoPortDiscovery == FALSE) //so the interface is forced, thus in rtOpts
      PTPD_TRACE(TRACE_PTPD_MAIN, NULL,"net ifaceName [port = %d] ........ %s\n",i+1,rtOpts->ifaceName[i]);

      if(rtOpts->wrConfig == WR_MODE_AUTO)
	PTPD_TRACE(TRACE_PTPD_MAIN, NULL,"wrConfig  [port = %d] ............ Autodetection (ptpx-implementation-specific) \n",i+1)
      else if(rtOpts->wrConfig == WR_M_AND_S)
	PTPD_TRACE(TRACE_PTPD_MAIN, NULL,"wrConfig  [port = %d] ............ Master and Slave \n",i+1)
      else if(rtOpts->wrConfig == WR_SLAVE)
	PTPD_TRACE(TRACE_PTPD_MAIN, NULL,"wrConfig  [port = %d] ............ Slave \n",i+1)
      else if(rtOpts->wrConfig == WR_MASTER)
	PTPD_TRACE(TRACE_PTPD_MAIN, NULL,"wrConfig  [port = %d] ............ Master \n",i+1)
      else if(rtOpts->wrConfig == NON_WR)
	PTPD_TRACE(TRACE_PTPD_MAIN, NULL,"wrConfig  [port = %d] ............ NON_WR\n",i+1)
      else
	PTPD_TRACE(TRACE_PTPD_MAIN, NULL,"wrConfig  [port = %d] ............ ERROR\n",i+1)
    }

    PTPD_TRACE(TRACE_PTPD_MAIN, NULL,"----------- now the fun ------------\n\n")

    PTPD_TRACE(TRACE_PTPD_MAIN, NULL,"clockClass ....................... %d\n",rtOpts->clockQuality.clockClass);

}

#endif
