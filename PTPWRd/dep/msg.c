/* msg.c */


#include "../ptpd.h"

/*
 * WR way to handle little/big endianess
 */
//put 16 bit word in big endianess (from little endianess)
static inline void put_be16(void *ptr, UInteger16 x)
{
  *(unsigned char *)(ptr++) = (x >> 8) & 0xff;
  *(unsigned char *)(ptr++) = (x) & 0xff;
}
//put 32 bit word in big endianess (from little endianess)
static inline void put_be32(void *ptr, UInteger32 x)
{
  *(unsigned char *)(ptr++) = (x >> 24) & 0xff;
  *(unsigned char *)(ptr++) = (x >> 16) & 0xff;
  *(unsigned char *)(ptr++) = (x >> 8) & 0xff;
  *(unsigned char *)(ptr++) = (x) & 0xff;
}

//gets 32 bit word from big endianess (to little endianess)
static inline UInteger32 get_be32(void *ptr)
{
  UInteger32 res = 0x0;

  res = res | ((*(unsigned char *)(ptr++) << 24) & 0xFF000000);
  res = res | ((*(unsigned char *)(ptr++) << 16) & 0x00FF0000);
  res = res | ((*(unsigned char *)(ptr++) << 8 ) & 0x0000FF00);
  res = res | ((*(unsigned char *)(ptr++) << 0 ) & 0x000000FF);
  return res;
}

//gets 16 bit word from big endianess (to little endianess)
static inline UInteger16 get_be16(void *ptr)
{
  UInteger16 res = 0x0;

  res = res | ((*(unsigned char *)(ptr++) << 8 ) & 0xFF00);
  res = res | ((*(unsigned char *)(ptr++) << 0 ) & 0x000FF);
  return res;
}


/*Unpack Header from IN buffer to msgTmpHeader field */
void msgUnpackHeader(void *buf, MsgHeader *header)
{
	header->transportSpecific               = (*(Nibble*)(buf+0))>>4;
	header->messageType                     = (*(Enumeration4*)(buf+0)) & 0x0F;
	header->versionPTP                      = (*(UInteger4*)(buf+1)) & 0x0F; //force reserved bit to zero if not
	header->messageLength                   = (UInteger16)get_be16(buf+2);
	header->domainNumber                    = (*(UInteger8*)(buf+4));
	memcpy(header->flagField,(buf+6),FLAG_FIELD_LENGTH);

	header->correctionfield.msb             = (Integer32)get_be32(buf+8);
	header->correctionfield.lsb             = (Integer32)get_be32(buf+12);

	memcpy(header->sourcePortIdentity.clockIdentity,(buf+20),CLOCK_IDENTITY_LENGTH);
	header->sourcePortIdentity.portNumber   = (UInteger16)get_be16(buf+28);
	header->sequenceId                      = flip16(*(UInteger16*)(buf+30));
	header->controlField                    = (*(UInteger8*)(buf+32));
	header->logMessageInterval              = (*(Integer8*)(buf+33));

	DBGM("------------ msgUnpackHeader ------\n");
	DBGM(" transportSpecific............. %u\n", header->transportSpecific);
	DBGM(" messageType................... %u\n", header->messageType);
	DBGM(" versionPTP.................... %u\n", header->versionPTP);
	DBGM(" messageLength................. %u\n", header->messageLength);
	DBGM(" domainNumber.................. %u\n", header->domainNumber);
	DBGM(" flagField..................... %02hhx %02hhx\n",
	    header->flagField[0],
	    header->flagField[1]
	    );
	DBGM(" correctionfield.msb........... %d\n", header->correctionfield.msb);
	DBGM(" correctionfield.lsb........... %d\n", (unsigned int)header->correctionfield.lsb);
	DBGM(" clockIdentity................. %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n",
	    header->sourcePortIdentity.clockIdentity[0], header->sourcePortIdentity.clockIdentity[1],
	    header->sourcePortIdentity.clockIdentity[2], header->sourcePortIdentity.clockIdentity[3],
	    header->sourcePortIdentity.clockIdentity[4], header->sourcePortIdentity.clockIdentity[5],
	    header->sourcePortIdentity.clockIdentity[6], header->sourcePortIdentity.clockIdentity[7]
	    );
	DBGM(" portNumber.................... %d\n", header->sourcePortIdentity.portNumber);
	DBGM(" sequenceId.................... %d\n", header->sequenceId);
	DBGM(" control....................... %d\n", header->controlField);
	DBGM(" logMessageInterval............ %d\n", header->logMessageInterval);
	DBGM("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");

}
/*Pack header message into OUT buffer of ptpClock*/
void msgPackHeader(void *buf, PtpClock *ptpClock)
{
	Nibble transport                        = 0x0; //0x80; //(spec annex D)
	*(UInteger8*)(buf+0)                    = transport;
	*(UInteger4*)(buf+1)                    = ptpClock->versionNumber;
	*(UInteger8*)(buf+4)                    = ptpClock->domainNumber;

	if (ptpClock->twoStepFlag)
		*(UInteger8*)(buf+6)            = TWO_STEP_FLAG;

	memset((buf+8),0,8);
	memcpy((buf+20),ptpClock->portIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH);

	put_be16(buf+28,ptpClock->portIdentity.portNumber);

	*(UInteger8*)(buf+33)                   = 0x7F; //Default value (spec Table 24)

	DBGM("------------ msgPackHeader --------\n");
	DBGM(" transportSpecific............. %u\n", transport);
	DBGM(" versionPTP.................... %u\n", ptpClock->versionNumber);
	DBGM(" domainNumber.................. %u\n", ptpClock->domainNumber);
	if (ptpClock->twoStepFlag)
	  DBGM(" flagField..................... %x\n", TWO_STEP_FLAG);
	else
	  DBGM(" flagField..................... %x\n", 0);
	DBGM(" clockIdentity................. %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n",
	    ptpClock->portIdentity.clockIdentity[0], ptpClock->portIdentity.clockIdentity[1],
	    ptpClock->portIdentity.clockIdentity[2], ptpClock->portIdentity.clockIdentity[3],
	    ptpClock->portIdentity.clockIdentity[4], ptpClock->portIdentity.clockIdentity[5],
	    ptpClock->portIdentity.clockIdentity[6], ptpClock->portIdentity.clockIdentity[7]);
	DBGM(" portNumber.................... %d\n", ptpClock->portIdentity.portNumber);
	DBGM(" logMessageInterval............ %d\n", 0x7F);
	DBGM("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
}



/*Pack SYNC message into OUT buffer of ptpClock*/
void msgPackSync(void *buf,Timestamp *originTimestamp,PtpClock *ptpClock)
{
	/*changes in header*/
	*(char*)(buf+0)= *(char*)(buf+0) & 0xF0; //RAZ messageType
	*(char*)(buf+0)= *(char*)(buf+0) | 0x00; //Table 19
	put_be16(buf + 2, SYNC_LENGTH);
	*(UInteger16*)(buf+30)=flip16(ptpClock->sentSyncSequenceId);
	*(UInteger8*)(buf+32)=0x00; //Table 23
	*(Integer8*)(buf+33) = ptpClock->logSyncInterval;
	memset((buf+8),0,8);

	/*Sync message*/
	*(UInteger16*)(buf+34) = flip16(originTimestamp->secondsField.msb);
	*(UInteger32*)(buf+36) = flip32(originTimestamp->secondsField.lsb);
	*(UInteger32*)(buf+40) = flip32(originTimestamp->nanosecondsField);

	DBGM("------------ msgPackSync ----------\n");
	DBGM(" messageLength................. %u\n", SYNC_LENGTH);
	DBGM(" sentSyncSequenceId............ %u\n", ptpClock->sentSyncSequenceId);
	DBGM(" logSyncInterval............... %u\n", ptpClock->logSyncInterval);
	DBGM(" clockIdentity................. %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n",
	    ptpClock->portIdentity.clockIdentity[0], ptpClock->portIdentity.clockIdentity[1],
	    ptpClock->portIdentity.clockIdentity[2], ptpClock->portIdentity.clockIdentity[3],
	    ptpClock->portIdentity.clockIdentity[4], ptpClock->portIdentity.clockIdentity[5],
	    ptpClock->portIdentity.clockIdentity[6], ptpClock->portIdentity.clockIdentity[7]);
	DBGM(" portNumber.................... %d\n", ptpClock->portIdentity.portNumber);
	DBGM(" originTimestamp.secs.msb...... %d\n", originTimestamp->secondsField.msb);
	DBGM(" originTimestamp.secs.lsb...... %d\n", originTimestamp->secondsField.lsb);
	DBGM(" originTimestamp.nsecs......... %d\n",  originTimestamp->nanosecondsField);
	DBGM("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");

}

/*Unpack Sync message from IN buffer */
void msgUnpackSync(void *buf,MsgSync *sync)
{
	sync->originTimestamp.secondsField.msb = flip16(*(UInteger16*)(buf+34));
	sync->originTimestamp.secondsField.lsb = flip32(*(UInteger32*)(buf+36));
	sync->originTimestamp.nanosecondsField = flip32(*(UInteger32*)(buf+40));

	DBGM("------------ msgUnpackSync ----------\n");
	DBGM(" originTimestamp.secs.msb...... %d\n", sync->originTimestamp.secondsField.msb);
	DBGM(" originTimestamp.secs.lsb...... %d\n", sync->originTimestamp.secondsField.lsb);
	DBGM(" originTimestamp.nsecs......... %d\n", sync->originTimestamp.nanosecondsField);
	DBGM("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");

}



/*Pack Announce message into OUT buffer of ptpClock*/
void msgPackAnnounce(void *buf,PtpClock *ptpClock)
{
	/*changes in header*/
	*(char*)(buf+0)= *(char*)(buf+0) & 0xF0; //RAZ messageType
	*(char*)(buf+0)= *(char*)(buf+0) | 0x0B; //Table 19
#ifdef WRPTPv2
	if (ptpClock->portWrConfig != NON_WR && ptpClock->portWrConfig != WR_S_ONLY)
#else
	if (ptpClock->wrMode != NON_WR)
#endif	  
	   put_be16(buf + 2,WR_ANNOUNCE_LENGTH);
	else
	   put_be16(buf + 2,ANNOUNCE_LENGTH);

	*(UInteger16*)(buf+30)=flip16(ptpClock->sentAnnounceSequenceId);
	*(UInteger8*)(buf+32)=0x05; //Table 23
	*(Integer8*)(buf+33) = ptpClock->logAnnounceInterval;

	/*Announce message*/
	memset((buf+34),0,10);
	*(Integer16*)(buf+44)=flip16(ptpClock->currentUtcOffset);

	*(UInteger8*)(buf+47)=ptpClock->grandmasterPriority1;
	*(UInteger8*)(buf+48)=ptpClock->clockQuality.clockClass;
	*(Enumeration8*)(buf+49)=ptpClock->clockQuality.clockAccuracy;
	*(UInteger16*)(buf+50)=flip16(ptpClock->clockQuality.offsetScaledLogVariance);
	*(UInteger8*)(buf+52)=ptpClock->grandmasterPriority2;
	memcpy((buf+53),ptpClock->grandmasterIdentity,CLOCK_IDENTITY_LENGTH);
	*(UInteger16*)(buf+61)=flip16(ptpClock->stepsRemoved);
	*(Enumeration8*)(buf+63)=ptpClock->timeSource;

	/*
	 * White rabbit message in the suffix of PTP announce message
	 */
	UInteger16 wr_flags = 0;
#ifdef WRPTPv2
	if (ptpClock->portWrConfig != NON_WR && ptpClock->portWrConfig != WR_S_ONLY)	
	{

  	  *(UInteger16*)(buf+64) = flip16(TLV_TYPE_ORG_EXTENSION);
	  *(UInteger16*)(buf+66) = flip16(WR_ANNOUNCE_TLV_LENGTH);
	  // CERN's OUI: WR_TLV_ORGANIZATION_ID, how to flip bits?
	  *(UInteger16*)(buf+68) = flip16((WR_TLV_ORGANIZATION_ID >> 8));
	  *(UInteger16*)(buf+70) = flip16((0xFFFF & (WR_TLV_ORGANIZATION_ID << 8 | WR_TLV_MAGIC_NUMBER >> 8)));
	  *(UInteger16*)(buf+72) = flip16((0xFFFF & (WR_TLV_MAGIC_NUMBER    << 8 | WR_TLV_WR_VERSION_NUMBER)));
	  //wrMessageId
	  *(UInteger16*)(buf+74) = flip16(ANN_SUFIX);
	  wr_flags = wr_flags | ptpClock->portWrConfig;
#else
	if (ptpClock->wrMode != NON_WR)
	{
	  *(UInteger16*)(buf+64) = flip16(WR_TLV_TYPE);
	  *(UInteger16*)(buf+66) = flip16(WR_ANNOUNCE_TLV_LENGTH);
	  
	  wr_flags = wr_flags | ptpClock->wrMode;
#endif

	  

	  if (ptpClock->isCalibrated)
	    wr_flags = WR_IS_CALIBRATED | wr_flags;

	  if (ptpClock->isWRmode)
	    wr_flags = WR_IS_WR_MODE | wr_flags;
#ifdef WRPTPv2
	  *(UInteger16*)(buf+76) = flip16(wr_flags);
#else
	  *(UInteger16*)(buf+68) = flip16(wr_flags);
#endif
	}

	DBGM("------------ msgPackAnnounce ----------\n");
#ifdef WRPTPv2
	if (ptpClock->portWrConfig != NON_WR && ptpClock->portWrConfig != WR_S_ONLY)
#else
	if (ptpClock->wrMode != NON_WR)
#endif	  
	  DBGM(" messageLength................. %u\n", WR_ANNOUNCE_LENGTH);
	else
	  DBGM(" messageLength................. %u\n", ANNOUNCE_LENGTH);
	DBGM(" sentSyncSequenceId............ %u\n", ptpClock->sentAnnounceSequenceId);
	DBGM(" logSyncInterval............... %u\n", ptpClock->logAnnounceInterval);
	DBGM(" clockIdentity................. %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n",
	    ptpClock->portIdentity.clockIdentity[0], ptpClock->portIdentity.clockIdentity[1],
	    ptpClock->portIdentity.clockIdentity[2], ptpClock->portIdentity.clockIdentity[3],
	    ptpClock->portIdentity.clockIdentity[4], ptpClock->portIdentity.clockIdentity[5],
	    ptpClock->portIdentity.clockIdentity[6], ptpClock->portIdentity.clockIdentity[7]);
	DBGM(" portNumber.................... %d\n", ptpClock->portIdentity.portNumber);
	DBGM(" currentUtcOffset.............. %d\n", ptpClock->currentUtcOffset);
	DBGM(" grandmasterPriority1.......... %d\n", ptpClock->grandmasterPriority1);
	DBGM(" clockClass.................... %d\n", ptpClock->clockQuality.clockClass);
	DBGM(" clockAccuracy................. %d\n", ptpClock->clockQuality.clockAccuracy);
	DBGM(" offsetScaledLogVariance....... %d\n", ptpClock->clockQuality.offsetScaledLogVariance);
	DBGM(" grandmasterPriority2.......... %d\n", ptpClock->grandmasterPriority2);
	DBGM(" grandmasterIdentity........... %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n",
	    ptpClock->grandmasterIdentity[0], ptpClock->grandmasterIdentity[1],
	    ptpClock->grandmasterIdentity[2], ptpClock->grandmasterIdentity[3],
	    ptpClock->grandmasterIdentity[4], ptpClock->grandmasterIdentity[5],
	    ptpClock->grandmasterIdentity[6], ptpClock->grandmasterIdentity[7]);
	DBGM(" stepsRemoved.................. %d\n", ptpClock->stepsRemoved);
	DBGM(" timeSource.................... %d\n", ptpClock->timeSource);
#ifdef WRPTPv2	
	if (ptpClock->portWrConfig != NON_WR && ptpClock->portWrConfig != WR_S_ONLY)
	{

	  DBGM(" [WR suffix] tlv_type.......... 0x%x\n", TLV_TYPE_ORG_EXTENSION);
	  DBGM(" [WR suffix] tlv_length........ %d\n",   WR_ANNOUNCE_TLV_LENGTH);
	  DBGM(" [WR suffix] tlv_organizID .....0x%x\n", WR_TLV_ORGANIZATION_ID);
	  DBGM(" [WR suffix] tlv_magicNumber... 0x%x\n", WR_TLV_MAGIC_NUMBER);
	  DBGM(" [WR suffix] tlv_versionNumber. 0x%x\n", WR_TLV_WR_VERSION_NUMBER);
	  DBGM(" [WR suffix] tlv_wrMessageID... 0x%x\n", ANN_SUFIX);
#else
	if (ptpClock->wrMode != NON_WR)
	{
	  DBGM(" [WR suffix] tlv_type.......... 0x%x\n", WR_TLV_TYPE);
	  DBGM(" [WR suffix] tlv_length........ %d\n",   WR_ANNOUNCE_TLV_LENGTH);
#endif
	  if((wr_flags & WR_NODE_MODE) == NON_WR)
	    DBGM(" [WR suffix] wr_flags.......... NON_WR\n");
	  else if((wr_flags & WR_NODE_MODE) == WR_S_ONLY)
	    DBGM(" [WR suffix] wr_flags.......... WR_S_ONLY\n");
	  else if((wr_flags & WR_NODE_MODE) == WR_M_ONLY)
	    DBGM(" [WR suffix] wr_flags.......... WR_M_ONLY\n");
	  else if((wr_flags & WR_NODE_MODE) == WR_M_AND_S)
	    DBGM(" [WR suffix] wr_flags.......... WR_M_AND_S\n");
	  else
	    DBGM(" [WR suffix] wr_flags.......... UNKNOWN !!!(this is error)\n");
	  
	  if((wr_flags & WR_IS_WR_MODE) == WR_IS_WR_MODE)
	    DBGM(" [WR suffix] wr_flags.......... WR MODE ON\n");
	  else
	    DBGM(" [WR suffix] wr_flags.......... WR MODE OFF\n");
	  
          if((wr_flags & WR_IS_CALIBRATED) == WR_IS_CALIBRATED)
	    DBGM(" [WR suffix] wr_flags.......... CALIBRATED\n");
	  else 
	    DBGM(" [WR suffix] wr_flags.......... unCALIBRATED\n");
	}
	DBGM("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");


}

/*Unpack Announce message from IN buffer of ptpClock to msgtmp.Announce*/
void msgUnpackAnnounce(void *buf,MsgAnnounce *announce,  MsgHeader *header)
{
	UInteger16 tlv_type;
#ifdef WRPTPv2	
	UInteger32 tlv_organizationID;
	UInteger16 tlv_magicNumber;
	UInteger16 tlv_versionNumber;
	UInteger16 tlv_wrMessageID;
#endif	
	
	announce->originTimestamp.secondsField.msb = flip16(*(UInteger16*)(buf+34));
	announce->originTimestamp.secondsField.lsb = flip32(*(UInteger32*)(buf+36));
	announce->originTimestamp.nanosecondsField = flip32(*(UInteger32*)(buf+40));
	announce->currentUtcOffset = flip16(*(UInteger16*)(buf+44));


	announce->grandmasterPriority1 = *(UInteger8*)(buf+47);
	announce->grandmasterClockQuality.clockClass = *(UInteger8*)(buf+48);
	announce->grandmasterClockQuality.clockAccuracy = *(Enumeration8*)(buf+49);
	announce->grandmasterClockQuality.offsetScaledLogVariance = flip16(*(UInteger16*)(buf+50));
	announce->grandmasterPriority2 = *(UInteger8*)(buf+52);
	memcpy(announce->grandmasterIdentity,(buf+53),CLOCK_IDENTITY_LENGTH);
	announce->stepsRemoved = flip16(*(UInteger16*)(buf+61));
	announce->timeSource = *(Enumeration8*)(buf+63);

	/*White Rabbit- only flags in a reserved space of announce message*/
	UInteger16  messageLen = (UInteger16)get_be16(buf + 2);
	//check if there is msg suffix
	if(messageLen > ANNOUNCE_LENGTH)
	{
	  tlv_type   = (UInteger16)get_be16(buf+64);
#ifdef WRPTPv2
	  tlv_organizationID = flip16(*(UInteger16*)(buf+68)) << 8;
	  tlv_organizationID = flip16(*(UInteger16*)(buf+70)) >> 8  | tlv_organizationID;
	  tlv_magicNumber    = 0xFF00 & (flip16(*(UInteger16*)(buf+70)) << 8);
	  tlv_magicNumber    = flip16(*(UInteger16*)(buf+72)) >>  8 | tlv_magicNumber;
	  tlv_versionNumber  = 0xFF & flip16(*(UInteger16*)(buf+72));
	  tlv_wrMessageID    = flip16(*(UInteger16*)(buf+74));
#endif

#ifdef WRPTPv2
	  if(tlv_type           == TLV_TYPE_ORG_EXTENSION   && \
	     tlv_organizationID == WR_TLV_ORGANIZATION_ID   && \
	     tlv_magicNumber    == WR_TLV_MAGIC_NUMBER      && \
	     tlv_versionNumber  == WR_TLV_WR_VERSION_NUMBER && \
	     tlv_wrMessageID    == ANN_SUFIX)
	  {
	    announce->wr_flags   = (UInteger16)get_be16(buf+76);
	  }

#else
	  if(tlv_type == WR_TLV_TYPE)
	  {
	    announce->wr_flags   = (UInteger16)get_be16(buf+68);
	  }
#endif
	}

	DBGM("------------ msgUnpackAnnounce ----------\n");
	DBGM(" messageLength................. %u\n", messageLen);
	DBGM(" currentUtcOffset.............. %d\n", announce->currentUtcOffset);
	DBGM(" grandmasterPriority1.......... %d\n", announce->grandmasterPriority1);
	DBGM(" clockClass.................... %d\n", announce->grandmasterClockQuality.clockClass);
	DBGM(" clockAccuracy................. %d\n", announce->grandmasterClockQuality.clockAccuracy);
	DBGM(" offsetScaledLogVariance....... %d\n", announce->grandmasterClockQuality.offsetScaledLogVariance);
	DBGM(" grandmasterPriority2.......... %d\n", announce->grandmasterPriority2);
	DBGM(" grandmasterIdentity........... %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n",
	    announce->grandmasterIdentity[0], announce->grandmasterIdentity[1],
	    announce->grandmasterIdentity[2], announce->grandmasterIdentity[3],
	    announce->grandmasterIdentity[4], announce->grandmasterIdentity[5],
	    announce->grandmasterIdentity[6], announce->grandmasterIdentity[7]);
	DBGM(" stepsRemoved.................. %d\n", announce->stepsRemoved);
	DBGM(" timeSource.................... %d\n", announce->timeSource);
	if (messageLen > ANNOUNCE_LENGTH)
	{
	  DBGM(" [WR suffix] tlv_type.......... 0x%x\n", tlv_type);
	  DBGM(" [WR suffix] tlv_length........ %d\n", (UInteger16)get_be16(buf+66));
#ifdef WRPTPv2
	  DBGM(" [WR suffix] tlv_organizID .....0x%x\n", tlv_organizationID);
	  DBGM(" [WR suffix] tlv_magicNumber... 0x%x\n", tlv_magicNumber);
	  DBGM(" [WR suffix] tlv_versionNumber. 0x%x\n", tlv_versionNumber);
	  DBGM(" [WR suffix] tlv_wrMessageID... 0x%x\n", tlv_wrMessageID);
#endif	  
	  DBGM(" [WR suffix] wr_flags.......... 0x%x\n", announce->wr_flags);
	  if((announce->wr_flags & WR_NODE_MODE) == NON_WR)
	    DBGM(" [WR suffix] wr_flags.......... NON_WR\n");
	  else if((announce->wr_flags & WR_NODE_MODE) == WR_S_ONLY)
	    DBGM(" [WR suffix] wr_flags.......... WR_S_ONLY\n");
	  else if((announce->wr_flags & WR_NODE_MODE) == WR_M_ONLY)
	    DBGM(" [WR suffix] wr_flags.......... WR_M_ONLY\n");
	  else if((announce->wr_flags & WR_NODE_MODE) == WR_M_AND_S)
	    DBGM(" [WR suffix] wr_flags.......... WR_M_AND_S\n");
	  else
	    DBGM(" [WR suffix] wr_flags.......... UNKNOWN !!!(this is error)\n");
	  
	  if((announce->wr_flags & WR_IS_WR_MODE) == WR_IS_WR_MODE)
	    DBGM(" [WR suffix] wr_flags.......... WR MODE ON\n");
	  else
	    DBGM(" [WR suffix] wr_flags.......... WR MODE OFF\n");
	  
          if((announce->wr_flags & WR_IS_CALIBRATED) == WR_IS_CALIBRATED)
	    DBGM(" [WR suffix] wr_flags.......... CALIBRATED\n");
	  else 
	    DBGM(" [WR suffix] wr_flags.......... unCALIBRATED\n");
	}
	DBGM("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");

}


/*pack Follow_up message into OUT buffer of ptpClock*/
void msgPackFollowUp(void *buf,PtpClock *ptpClock)
//void msgPackFollowUp(void *buf,Timestamp *preciseOriginTimestamp,PtpClock *ptpClock)
{
	/*changes in header*/
	*(char*)(buf+0)= *(char*)(buf+0) & 0xF0; //RAZ messageType
	*(char*)(buf+0)= *(char*)(buf+0) | 0x08; //Table 19
	put_be16(buf + 2, FOLLOW_UP_LENGTH);
	*(UInteger16*)(buf+30)=flip16(ptpClock->sentSyncSequenceId-1);//sentSyncSequenceId has already been incremented in "issueSync"
	*(UInteger8*)(buf+32)=0x02; //Table 23
	*(Integer8*)(buf+33) = ptpClock->logSyncInterval;

	/*Follow_up message*/
	*(UInteger16*)(buf+34) = flip16(0xFFFF & (ptpClock->synch_tx_ts.utc >> 32));
	put_be32(buf+36,0xFFFFFFFF & ptpClock->synch_tx_ts.utc);
	put_be32(buf+40, ptpClock->synch_tx_ts.nsec);

	/*
	 * by ML: follow up msg can also have correction field,
	 * it's just not implemented here
	 */

	DBGM("------------ msgPackFollowUp-------\n");
	DBGM(" syncSequenceId ............... %u\n", ptpClock->sentSyncSequenceId-1);
	DBGM(" logMinDelayReqInterval ....... %u\n", ptpClock->logSyncInterval);
	DBGM(" syncTransTimestamp.secs.hi.... %d\n", 0xFFFF & (ptpClock->synch_tx_ts.utc >> 32));
	DBGM(" syncTransTimestamp.secs.lo.... %d\n", 0xFFFFFFFF & ptpClock->synch_tx_ts.utc);
	DBGM(" syncTransTimestamp.nsecs...... %d\n", ptpClock->synch_tx_ts.nsec);
	DBGM("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
}

/*Unpack Follow_up message from IN buffer of ptpClock to msgtmp.follow*/
void msgUnpackFollowUp(void *buf,MsgFollowUp *follow)
{
	follow->preciseOriginTimestamp.secondsField.msb = flip16(*(UInteger16*)(buf+34));
	follow->preciseOriginTimestamp.secondsField.lsb = get_be32(buf+36);
	follow->preciseOriginTimestamp.nanosecondsField = get_be32(buf+40);

	DBGM("------------ msgUnpackFollowUp-------\n");
	DBGM(" preciseOriginTimestamp.secs.hi.%d\n", follow->preciseOriginTimestamp.secondsField.msb);
	DBGM(" preciseOriginTimestamp.secs.lo %d\n", follow->preciseOriginTimestamp.secondsField.lsb);
	DBGM(" preciseOriginTimestamp.nsecs.. %d\n", follow->preciseOriginTimestamp.nanosecondsField);
	DBGM("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");

}


/*pack PdelayReq message into OUT buffer of ptpClock*/
void msgPackPDelayReq(void *buf,Timestamp *originTimestamp,PtpClock *ptpClock)
{
	/*changes in header*/
	*(char*)(buf+0)= *(char*)(buf+0) & 0xF0; //RAZ messageType
	*(char*)(buf+0)= *(char*)(buf+0) | 0x02; //Table 19
	put_be16(buf + 2, PDELAY_REQ_LENGTH);
	*(UInteger16*)(buf+30)= flip16(ptpClock->sentPDelayReqSequenceId);
	*(UInteger8*)(buf+32) = 0x05; //Table 23
	*(Integer8*)(buf+33) = 0x7F; //Table 24
	memset((buf+8),0,8);

	/*Pdelay_req message*/
	*(UInteger16*)(buf+34) = flip16(originTimestamp->secondsField.msb);
	*(UInteger32*)(buf+36) = flip32(originTimestamp->secondsField.lsb);
	*(UInteger32*)(buf+40) = flip32(originTimestamp->nanosecondsField);

	memset((buf+44),0,10); // RAZ reserved octets
}

/*pack delayReq message into OUT buffer of ptpClock*/
void msgPackDelayReq(void *buf,Timestamp *originTimestamp,PtpClock *ptpClock)
{
	/*changes in header*/
	*(char*)(buf+0)= *(char*)(buf+0) & 0xF0; //RAZ messageType
	*(char*)(buf+0)= *(char*)(buf+0) | 0x01; //Table 19
	put_be16(buf + 2, DELAY_REQ_LENGTH);
	*(UInteger16*)(buf+30)= flip16(ptpClock->sentDelayReqSequenceId);
	*(UInteger8*)(buf+32) = 0x01; //Table 23
	*(Integer8*)(buf+33) = 0x7F; //Table 24
	memset((buf+8),0,8);

	/*delay_req message*/
	*(UInteger16*)(buf+34) = flip16(originTimestamp->secondsField.msb);
	*(UInteger32*)(buf+36) = flip32(originTimestamp->secondsField.lsb);
	*(UInteger32*)(buf+40) = flip32(originTimestamp->nanosecondsField);

	DBGM("------------ msgPackDelayReq-------\n");
	DBGM(" delayReqSequenceId ........... %u\n", ptpClock->sentDelayReqSequenceId);
	DBGM(" originTimestamp.secs.msb...... %d\n", originTimestamp->secondsField.msb);
	DBGM(" originTimestamp.secs.lsb...... %d\n", originTimestamp->secondsField.lsb);
	DBGM(" originTimestamp.nsecs......... %d\n", originTimestamp->nanosecondsField);
	DBGM("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");


}

/*pack delayResp message into OUT buffer of ptpClock*/
void msgPackDelayResp(void *buf,MsgHeader *header,PtpClock *ptpClock)
{
	/*changes in header*/
	*(char*)(buf+0)= *(char*)(buf+0) & 0xF0; //RAZ messageType
	*(char*)(buf+0)= *(char*)(buf+0) | 0x09; //Table 19
	put_be16(buf + 2, DELAY_RESP_LENGTH);
	*(UInteger8*)(buf+4) = header->domainNumber;
	memset((buf+8),0,8);

	/*Copy correctionField of PdelayReqMessage*/
	put_be32(buf+8, header->correctionfield.msb);
	put_be32(buf+12, header->correctionfield.lsb);

	*(UInteger16*)(buf+30)= flip16(header->sequenceId);

	*(UInteger8*)(buf+32) = 0x03; //Table 23
	*(Integer8*)(buf+33) = ptpClock->logMinDelayReqInterval; //Table 24

	/*Pdelay_resp message*/
	*(UInteger16*)(buf+34) = flip16(0xFFFF & (ptpClock->current_rx_ts.utc >> 32));
	put_be32(buf+36, 0xFFFFFFFF & ptpClock->current_rx_ts.utc);
	put_be32(buf+40, ptpClock->current_rx_ts.nsec);

	memcpy((buf+44),header->sourcePortIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH);

	put_be16(buf + 52, header->sourcePortIdentity.portNumber);


	DBGM("------------ msgPackDelayResp-------\n");
	DBGM(" correctionfield.msb .......... %d\n", header->correctionfield.msb);
	DBGM(" correctionfield.lsb........... %d\n", header->correctionfield.lsb);
	DBGM(" sequenceId ................... %u\n", header->sequenceId);
	DBGM(" logMinDelayReqInterval ....... %u\n", ptpClock->logMinDelayReqInterval);
	DBGM(" delayReceiptTimestamp.secs.hi. %d\n", 0xFFFF & (ptpClock->current_rx_ts.utc >> 32));
	DBGM(" delayReceiptTimestamp.secs.lo. %d\n", 0xFFFFFFFF & ptpClock->current_rx_ts.utc);
	DBGM(" delayReceiptTimestamp.nsecs... %d\n", ptpClock->current_rx_ts.nsec);
	DBGM(" requestingSourceUuid.......... %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n",
	    header->sourcePortIdentity.clockIdentity[0],
	    header->sourcePortIdentity.clockIdentity[1],
	    header->sourcePortIdentity.clockIdentity[2],
	    header->sourcePortIdentity.clockIdentity[3],
	    header->sourcePortIdentity.clockIdentity[4],
	    header->sourcePortIdentity.clockIdentity[5],
	    header->sourcePortIdentity.clockIdentity[6],
	    header->sourcePortIdentity.clockIdentity[7]
	    );
	DBGM(" requestingSourcePortId........ %u\n", header->sourcePortIdentity.portNumber);
	DBGM("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");

}





/*pack PdelayResp message into OUT buffer of ptpClock*/
void msgPackPDelayResp(void *buf,MsgHeader *header,Timestamp *requestReceiptTimestamp,PtpClock *ptpClock)
{
	/*changes in header*/
	*(char*)(buf+0)= *(char*)(buf+0) & 0xF0; //RAZ messageType
	*(char*)(buf+0)= *(char*)(buf+0) | 0x03; //Table 19
	put_be16(buf + 2, PDELAY_RESP_LENGTH);
	*(UInteger8*)(buf+4) = header->domainNumber;
	memset((buf+8),0,8);


	*(UInteger16*)(buf+30)= flip16(header->sequenceId);

	*(UInteger8*)(buf+32) = 0x05; //Table 23
	*(Integer8*)(buf+33) = 0x7F; //Table 24

	/*Pdelay_resp message*/
// 	*(UInteger16*)(buf+34) = flip16(requestReceiptTimestamp->secondsField.msb);
// 	*(UInteger32*)(buf+36) = flip32(requestReceiptTimestamp->secondsField.lsb);
// 	*(UInteger32*)(buf+40) = flip32(requestReceiptTimestamp->nanosecondsField);
	*(UInteger16*)(buf+34) = flip16(0xFFFF & (ptpClock->current_rx_ts.utc >> 32 ));
	put_be32(buf+36,0xFFFFFFFF & ptpClock->current_rx_ts.utc);
	put_be32(buf+40, ptpClock->current_rx_ts.nsec);

	memcpy((buf+44),header->sourcePortIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH);
	put_be16(buf + 52, header->sourcePortIdentity.portNumber);

}


/*Unpack delayReq message from IN buffer of ptpClock to msgtmp.req*/
void msgUnpackDelayReq(void *buf,MsgDelayReq *delayreq)
{
	delayreq->originTimestamp.secondsField.msb = flip16(*(UInteger16*)(buf+34));
	delayreq->originTimestamp.secondsField.lsb = get_be32(buf+36);
	delayreq->originTimestamp.nanosecondsField = get_be32(buf+40);

	DBGM("------------ msgUnpackDelayReq-------\n");
	DBGM(" preciseOriginTimestamp.secs.hi.%d\n", delayreq->originTimestamp.secondsField.msb);
	DBGM(" preciseOriginTimestamp.secs.lo %d\n", delayreq->originTimestamp.secondsField.lsb);
	DBGM(" preciseOriginTimestamp.nsecs.. %d\n", delayreq->originTimestamp.nanosecondsField);
	DBGM("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");

}


/*Unpack PdelayReq message from IN buffer of ptpClock to msgtmp.req*/
void msgUnpackPDelayReq(void *buf,MsgPDelayReq *pdelayreq)
{
	pdelayreq->originTimestamp.secondsField.msb = flip16(*(UInteger16*)(buf+34));
	pdelayreq->originTimestamp.secondsField.lsb = get_be32(buf+36);
	pdelayreq->originTimestamp.nanosecondsField = get_be32(buf+40);
}


/*Unpack delayResp message from IN buffer of ptpClock to msgtmp.presp*/
void msgUnpackDelayResp(void *buf,MsgDelayResp *resp)
{
	resp->receiveTimestamp.secondsField.msb = flip16(*(UInteger16*)(buf+34));
	resp->receiveTimestamp.secondsField.lsb = get_be32(buf+36);
	resp->receiveTimestamp.nanosecondsField = get_be32(buf+40);
	memcpy(resp->requestingPortIdentity.clockIdentity,(buf+44),CLOCK_IDENTITY_LENGTH);
	resp->requestingPortIdentity.portNumber = (UInteger16)get_be16(buf+52);

	DBGM("------------ msgUnpackDelayResp-------\n");
	DBGM(" receiveTimestamp.secs.msb......%d\n", resp->receiveTimestamp.secondsField.msb);
	DBGM(" receiveTimestamp.secs.lsb..... %d\n", resp->receiveTimestamp.secondsField.lsb);
	DBGM(" receiveTimestamp.nsecs........ %d\n", resp->receiveTimestamp.nanosecondsField);
	DBGM(" requestingPortUuid.......... %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n",
	    resp->requestingPortIdentity.clockIdentity[0],
	    resp->requestingPortIdentity.clockIdentity[1],
	    resp->requestingPortIdentity.clockIdentity[2],
	    resp->requestingPortIdentity.clockIdentity[3],
	    resp->requestingPortIdentity.clockIdentity[4],
	    resp->requestingPortIdentity.clockIdentity[5],
	    resp->requestingPortIdentity.clockIdentity[6],
	    resp->requestingPortIdentity.clockIdentity[7]
	    );
	DBGM(" requestingSourcePortId........ %u\n", resp->requestingPortIdentity.portNumber);
	DBGM("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");

}


/*Unpack PdelayResp message from IN buffer of ptpClock to msgtmp.presp*/
void msgUnpackPDelayResp(void *buf,MsgPDelayResp *presp)
{
// 	presp->requestReceiptTimestamp.secondsField.msb = flip16(*(UInteger16*)(buf+34));
// 	presp->requestReceiptTimestamp.secondsField.lsb = flip32(*(UInteger32*)(buf+36));
// 	presp->requestReceiptTimestamp.nanosecondsField = flip32(*(UInteger32*)(buf+40));

	presp->requestReceiptTimestamp.secondsField.msb = flip16(*(UInteger16*)(buf+34));
	presp->requestReceiptTimestamp.secondsField.lsb = get_be32(buf+36);
	presp->requestReceiptTimestamp.nanosecondsField = get_be32(buf+40);

	memcpy(presp->requestingPortIdentity.clockIdentity,(buf+44),CLOCK_IDENTITY_LENGTH);
	presp->requestingPortIdentity.portNumber = (UInteger16)get_be16(buf+52);



}

/*pack PdelayRespfollowup message into OUT buffer of ptpClock*/
void msgPackPDelayRespFollowUp(void *buf,MsgHeader *header,Timestamp *responseOriginTimestamp,PtpClock *ptpClock)
{
	/*changes in header*/
	*(char*)(buf+0)= *(char*)(buf+0) & 0xF0; //RAZ messageType
	*(char*)(buf+0)= *(char*)(buf+0) | 0x0A; //Table 19
	put_be16(buf + 2, PDELAY_RESP_FOLLOW_UP_LENGTH);
	*(UInteger16*)(buf+30)= flip16(ptpClock->PdelayReqHeader.sequenceId);
	*(UInteger8*)(buf+32) = 0x05; //Table 23
	*(Integer8*)(buf+33) = 0x7F; //Table 24

	/*Copy correctionField of PdelayReqMessage*/
	*(Integer32*)(buf+8) = flip32(header->correctionfield.msb);
	*(Integer32*)(buf+12) = flip32(header->correctionfield.lsb);

	/*Pdelay_resp_follow_up message*/
// 	*(UInteger16*)(buf+34) = flip16(responseOriginTimestamp->secondsField.msb);
// 	*(UInteger32*)(buf+36) = flip32(responseOriginTimestamp->secondsField.lsb);
// 	*(UInteger32*)(buf+40) = flip32(responseOriginTimestamp->nanosecondsField);

	*(UInteger16*)(buf+34) = flip16(0xFFFF & (ptpClock->pDelayResp_tx_ts.utc >> 32));
	put_be32(buf+36, 0xFFFFFFFF &  ptpClock->pDelayResp_tx_ts.utc);
	put_be32(buf+40, ptpClock->pDelayResp_tx_ts.nsec);

	memcpy((buf+44),header->sourcePortIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH);
	put_be16(buf + 52, header->sourcePortIdentity.portNumber);
}

/*Unpack PdelayResp message from IN buffer of ptpClock to msgtmp.presp*/
void msgUnpackPDelayRespFollowUp(void *buf,MsgPDelayRespFollowUp *prespfollow)
{
// 	prespfollow->responseOriginTimestamp.secondsField.msb = flip16(*(UInteger16*)(buf+34));
// 	prespfollow->responseOriginTimestamp.secondsField.lsb = flip32(*(UInteger32*)(buf+36));
// 	prespfollow->responseOriginTimestamp.nanosecondsField = flip32(*(UInteger32*)(buf+40));

	prespfollow->responseOriginTimestamp.secondsField.msb = flip16(*(UInteger16*)(buf+34));
	prespfollow->responseOriginTimestamp.secondsField.lsb = get_be32(buf+36);
	prespfollow->responseOriginTimestamp.nanosecondsField = get_be32(buf+40);

	memcpy(prespfollow->requestingPortIdentity.clockIdentity,(buf+44),CLOCK_IDENTITY_LENGTH);
	prespfollow->requestingPortIdentity.portNumber = (UInteger16)get_be16(buf+52);
}



UInteger16 msgPackWRManagement(void *buf,PtpClock *ptpClock, Enumeration16 wr_managementId)
{

	if (ptpClock->wrMode == NON_WR)
	  return 0;

	/*changes in header*/
	*(char*)(buf+0)= *(char*)(buf+0) & 0xF0; //RAZ messageType
	*(char*)(buf+0)= *(char*)(buf+0) | 0x0D; //Table 19

	//*(UInteger16*)(buf+2) = flip16(WR_MANAGEMENT_LENGTH);

	*(UInteger8*)(buf+32)=0x04; //Table 23

	/*Management message*/
	//target portIdentity
	memcpy((buf+34),ptpClock->parentPortIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH);
	put_be16(buf + 42,ptpClock->parentPortIdentity.portNumber);

	//Hops staff, we dont care at the moment
	*(Integer8*)(buf+44) = 0;
	*(Integer8*)(buf+45) = 0;

	/*White Rabbit command*/
	*(Integer8*)(buf+46) = 0x00 | WR_CMD;


	/*Management TLV*/

	*(Integer16*)(buf+48) = flip16(WR_TLV_TYPE);

	DBGM("------------ msgPackWRManagement-------\n");
	DBGM(" recipient's PortUuid.......... %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n",
	    ptpClock->parentPortIdentity.clockIdentity[0],
	    ptpClock->parentPortIdentity.clockIdentity[1],
	    ptpClock->parentPortIdentity.clockIdentity[2],
	    ptpClock->parentPortIdentity.clockIdentity[3],
	    ptpClock->parentPortIdentity.clockIdentity[4],
	    ptpClock->parentPortIdentity.clockIdentity[5],
	    ptpClock->parentPortIdentity.clockIdentity[6],
	    ptpClock->parentPortIdentity.clockIdentity[7]
	    );
	DBGM(" recipient's PortId............ %u\n", ptpClock->parentPortIdentity.portNumber);
	DBGM(" management CMD................ %u\n", WR_CMD);
	DBGM(" management ID................. 0x%x\n", wr_managementId);

 	UInteger16 len = 0;
 	switch(wr_managementId)
 	{
#ifdef NEW_SINGLE_WRFSM
	  case CALIBRATE: //new fsm



	    if(ptpClock->isCalibrated)
	    {
	      put_be16(buf+54, 0x0000);
	      DBGM(" calibrationSendPattern........ FALSE \n");
	    }
	    else
	    {
	      put_be16(buf+54, 0x0001);
	      DBGM(" calibrationSendPattern........ TRUE \n");
	    }
	    put_be32(buf+56, ptpClock->calibrationPeriod);
	    put_be32(buf+60, ptpClock->calibrationPattern);
	    put_be16(buf+64, ptpClock->calibrationPatternLen);
	    len = 12;

#else
 	  case SLAVE_CALIBRATE:
 	  case MASTER_CALIBRATE:

	    put_be32(buf+54, ptpClock->calibrationPeriod);
	    put_be32(buf+58, ptpClock->calibrationPattern);
	    put_be16(buf+62, ptpClock->calibrationPatternLen);
	    len = 10;

#endif

	    DBGM(" calibrationPeriod............. %u [us]\n", ptpClock->calibrationPeriod);
	    DBGM(" calibrationPattern............ %s \n", printf_bits(ptpClock->calibrationPattern));
	    DBGM(" calibrationPatternLen......... %u [bits]\n", ptpClock->calibrationPatternLen);



	    break;

#ifdef NEW_SINGLE_WRFSM
	  case CALIBRATED: //new fsm
#else
	  case SLAVE_CALIBRATED:
	  case MASTER_CALIBRATED:
#endif


	    /*delta TX*/
	    put_be32(buf+54, ptpClock->deltaTx.scaledPicoseconds.msb);
	    put_be32(buf+58, ptpClock->deltaTx.scaledPicoseconds.lsb);

	    /*delta RX*/
	    put_be32(buf+62, ptpClock->deltaRx.scaledPicoseconds.msb);
	    put_be32(buf+66, ptpClock->deltaRx.scaledPicoseconds.lsb);

	    DBGM(" deltaTx.scaledPicoseconds.msb. %d\n", (unsigned int)ptpClock->deltaTx.scaledPicoseconds.msb);
	    DBGM(" deltaTx.scaledPicoseconds.lsb. %d\n", (unsigned int)ptpClock->deltaTx.scaledPicoseconds.lsb);

	    DBGM(" deltaRx.scaledPicoseconds.msb. %d\n", (unsigned int)ptpClock->deltaRx.scaledPicoseconds.msb);
	    DBGM(" deltaRx.scaledPicoseconds.lsb. %d\n", (unsigned int)ptpClock->deltaRx.scaledPicoseconds.lsb);


	    len = 16;

	    break;

	  default:
	    //no data

	    len = 0;

	    break;

	}
	//header len
	put_be16(buf + 2, WR_MANAGEMENT_LENGTH + len);
	//TLV len
	*(Integer16*)(buf+50) = flip16(WR_MANAGEMENT_TLV_LENGTH + len);
	*(Enumeration16*)(buf+52) = flip16(wr_managementId);

	DBGM(" messageLength................. %u\n", WR_MANAGEMENT_LENGTH + len);
	DBGM(" wr management len............. %u\n", WR_MANAGEMENT_TLV_LENGTH + len);


	DBGM("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");


	return (WR_MANAGEMENT_LENGTH + len);
}
#ifdef WRPTPv2
UInteger16 msgPackWRSignalingMsg(void *buf,PtpClock *ptpClock, Enumeration16 wrMessageID)
{

	if (ptpClock->wrMode == NON_WR || \
	    wrMessageID          == ANN_SUFIX)
	  return 0;

	/*changes in header*/
	*(char*)(buf+0)= *(char*)(buf+0) & 0xF0; //RAZ messageType
	*(char*)(buf+0)= *(char*)(buf+0) | 0x0C; //Table 19 -> signaling

	//*(UInteger16*)(buf+2) = flip16(WR_MANAGEMENT_LENGTH);

	*(UInteger8*)(buf+32)=0x05; //Table 23 -> all other

	//target portIdentity
	memcpy((buf+34),ptpClock->parentPortIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH);
	put_be16(buf + 42,ptpClock->parentPortIdentity.portNumber);


	/*WR TLV*/

	*(UInteger16*)(buf+44) = flip16(TLV_TYPE_ORG_EXTENSION);
	//leave lenght free
	*(UInteger16*)(buf+48) = flip16((WR_TLV_ORGANIZATION_ID >> 8));
	*(UInteger16*)(buf+50) = flip16((0xFFFF & (WR_TLV_ORGANIZATION_ID << 8 | WR_TLV_MAGIC_NUMBER >> 8)));
	*(UInteger16*)(buf+52) = flip16((0xFFFF & (WR_TLV_MAGIC_NUMBER    << 8 | WR_TLV_WR_VERSION_NUMBER)));
	//wrMessageId
	*(UInteger16*)(buf+54) = flip16(wrMessageID);
	

	DBGM("------------ msgPackWRSignalingMSG-------\n");
	DBGM(" recipient's PortUuid.......... %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n",
	    ptpClock->parentPortIdentity.clockIdentity[0],
	    ptpClock->parentPortIdentity.clockIdentity[1],
	    ptpClock->parentPortIdentity.clockIdentity[2],
	    ptpClock->parentPortIdentity.clockIdentity[3],
	    ptpClock->parentPortIdentity.clockIdentity[4],
	    ptpClock->parentPortIdentity.clockIdentity[5],
	    ptpClock->parentPortIdentity.clockIdentity[6],
	    ptpClock->parentPortIdentity.clockIdentity[7]
	    );
	DBGM(" recipient's PortId............ %u\n", ptpClock->parentPortIdentity.portNumber);
	DBGM(" tlv_type...................... 0x%x\n", TLV_TYPE_ORG_EXTENSION);
	DBGM(" tlv_length.................... %d\n",   WR_ANNOUNCE_TLV_LENGTH);
	DBGM(" tlv_organizID ................ 0x%x\n", WR_TLV_ORGANIZATION_ID);
	DBGM(" tlv_magicNumber............... 0x%x\n", WR_TLV_MAGIC_NUMBER);
	DBGM(" tlv_versionNumber............. 0x%x\n", WR_TLV_WR_VERSION_NUMBER);
	DBGM(" tlv_wrMessageID............... 0x%x\n", wrMessageID);	

 	UInteger16 len = 0;
 	switch(wrMessageID)
 	{
	  case CALIBRATE: //new fsm



	    if(ptpClock->isCalibrated)
	    {
	      put_be16(buf+56, 0x0000);
	      DBGM(" calibrationSendPattern........ FALSE \n");
	    }
	    else
	    {
	      put_be16(buf+56, 0x0001);
	      DBGM(" calibrationSendPattern........ TRUE \n");
	    }
	    put_be32(buf+58, ptpClock->calibrationPeriod);
	    put_be32(buf+62, ptpClock->calibrationPattern);
	    put_be16(buf+66, ptpClock->calibrationPatternLen);
	    len = 20;


	    DBGM(" calibrationPeriod............. %u [us]\n", ptpClock->calibrationPeriod);
	    DBGM(" calibrationPattern............ %s \n", printf_bits(ptpClock->calibrationPattern));
	    DBGM(" calibrationPatternLen......... %u [bits]\n", ptpClock->calibrationPatternLen);



	    break;

	  case CALIBRATED: //new fsm


	    /*delta TX*/
	    put_be32(buf+56, ptpClock->deltaTx.scaledPicoseconds.msb);
	    put_be32(buf+60, ptpClock->deltaTx.scaledPicoseconds.lsb);

	    /*delta RX*/
	    put_be32(buf+64, ptpClock->deltaRx.scaledPicoseconds.msb);
	    put_be32(buf+68, ptpClock->deltaRx.scaledPicoseconds.lsb);

	    DBGM(" deltaTx.scaledPicoseconds.msb. %d\n", (unsigned int)ptpClock->deltaTx.scaledPicoseconds.msb);
	    DBGM(" deltaTx.scaledPicoseconds.lsb. %d\n", (unsigned int)ptpClock->deltaTx.scaledPicoseconds.lsb);

	    DBGM(" deltaRx.scaledPicoseconds.msb. %d\n", (unsigned int)ptpClock->deltaRx.scaledPicoseconds.msb);
	    DBGM(" deltaRx.scaledPicoseconds.lsb. %d\n", (unsigned int)ptpClock->deltaRx.scaledPicoseconds.lsb);


	    len = 22;

	    break;

	  default:
	    //only WR TLV "header" and wrMessageID

	    len = 8;

	    break;

	}
	//header len
	put_be16(buf + 2, WR_SIGNALING_MSG_BASE_LENGTH + len);
	//TLV len
	*(Integer16*)(buf+46) = flip16(len);
	

	DBGM(" messageLength................. %u\n",  WR_SIGNALING_MSG_BASE_LENGTH + len);
	DBGM(" WR TLV len.................... %u\n", len);


	DBGM("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");


	return (WR_SIGNALING_MSG_BASE_LENGTH + len);
}

void msgUnpackWRSignalingMsg(void *buf,MsgSignaling *signalingMsg, Enumeration16 *wrMessageID, PtpClock *ptpClock )
{
	UInteger16 tlv_type;
	UInteger32 tlv_organizationID;
	UInteger16 tlv_magicNumber;
	UInteger16 tlv_versionNumber;
	

	UInteger16 len = (UInteger16)get_be16(buf+2);

	
	memcpy(signalingMsg->targetPortIdentity.clockIdentity,(buf+34),CLOCK_IDENTITY_LENGTH);
	signalingMsg->targetPortIdentity.portNumber = (UInteger16)get_be16(buf+42);

	tlv_type  	   = (UInteger16)get_be16(buf+44);
	tlv_organizationID = flip16(*(UInteger16*)(buf+48)) << 8;
	tlv_organizationID = flip16(*(UInteger16*)(buf+50)) >> 8  | tlv_organizationID;
	tlv_magicNumber    = 0xFF00 & (flip16(*(UInteger16*)(buf+50)) << 8);
	tlv_magicNumber    = flip16(*(UInteger16*)(buf+52)) >>  8 | tlv_magicNumber;
	tlv_versionNumber  = 0xFF & flip16(*(UInteger16*)(buf+52));

	if(tlv_type           != TLV_TYPE_ORG_EXTENSION)
	{
	  DBG("handle Signaling msg, failed, This is not organistion extensino TLV = 0x%x\n", tlv_type);
	  return;
	}  

	if(tlv_organizationID != WR_TLV_ORGANIZATION_ID)
	{
	  DBG("handle Signaling msg, failed, not CERN's OUI = 0x%x\n", tlv_organizationID);
	  return;
	}  
	
	if(tlv_magicNumber    != WR_TLV_MAGIC_NUMBER)
	{
	  DBG("handle Signaling msg, failed, not White Rabbit magic number = 0x%x\n", tlv_magicNumber);
	  return;
	} 	  
	if(tlv_versionNumber  != WR_TLV_WR_VERSION_NUMBER )
	{
	  DBG("handle Signaling msg, failed, not supported vesio Number = 0x%x\n", tlv_versionNumber);
	  return;
	} 
	*wrMessageID    = flip16(*(UInteger16*)(buf+54));



	DBGM("------------ msgUnpackWRSignalingMsg-------\n");
	DBGM(" target PortUuid............... %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n",
	    signalingMsg->targetPortIdentity.clockIdentity[0],
	    signalingMsg->targetPortIdentity.clockIdentity[1],
	    signalingMsg->targetPortIdentity.clockIdentity[2],
	    signalingMsg->targetPortIdentity.clockIdentity[3],
	    signalingMsg->targetPortIdentity.clockIdentity[4],
	    signalingMsg->targetPortIdentity.clockIdentity[5],
	    signalingMsg->targetPortIdentity.clockIdentity[6],
	    signalingMsg->targetPortIdentity.clockIdentity[7]
	    );
	DBGM(" target PortId................. %u\n", signalingMsg->targetPortIdentity.portNumber);
	DBGM(" tlv_type...................... 0x%x\n", tlv_type);
	DBGM(" tlv_length.................... %d\n",   len);
	DBGM(" tlv_organizID ................ 0x%x\n", tlv_organizationID);
	DBGM(" tlv_magicNumber............... 0x%x\n", tlv_magicNumber);
	DBGM(" tlv_versionNumber............. 0x%x\n", tlv_versionNumber);
	DBGM(" tlv_wrMessageID............... 0x%x\n", *wrMessageID);

	/*This is not nice way of doing it, need to be changed later !!!!!*/
	if(*wrMessageID == CALIBRATE || *wrMessageID == CALIBRATE)
	{
 	  switch(*wrMessageID)
 	  {

	    case CALIBRATE:

	      ptpClock->otherNodeCalibrationSendPattern= get_be16(buf+56);
	      ptpClock->otherNodeCalibrationPeriod     = get_be32(buf+58);
	      ptpClock->otherNodeCalibrationPattern    = get_be32(buf+62);
	      ptpClock->otherNodeCalibrationPatternLen = get_be16(buf+66);

	      if(ptpClock->otherNodeCalibrationSendPattern & SEND_CALIBRATION_PATTERN)
		DBGM(" calibrationSendPattern........ TRUE \n");
	      else
		DBGM(" calibrationSendPattern........ FALSE \n");


	      DBGM(" calibrationPeriod............. %u [us]\n", ptpClock->calibrationPeriod);
	      DBGM(" calibrationPattern............ %s \n", printf_bits(ptpClock->calibrationPattern));
	      DBGM(" calibrationPatternLen......... %u [bits]\n", ptpClock->calibrationPatternLen);

	      break;

	    case CALIBRATED:
	      /*delta TX*/
	      ptpClock->grandmasterDeltaTx.scaledPicoseconds.msb = get_be32(buf+56);
	      ptpClock->grandmasterDeltaTx.scaledPicoseconds.lsb = get_be32(buf+60);

	      /*delta RX*/
	      ptpClock->grandmasterDeltaRx.scaledPicoseconds.msb = get_be32(buf+64);
	      ptpClock->grandmasterDeltaRx.scaledPicoseconds.lsb = get_be32(buf+68);

	      DBGM(" deltaTx.scaledPicoseconds.msb. %d\n", (unsigned int)ptpClock->grandmasterDeltaTx.scaledPicoseconds.msb);
	      DBGM(" deltaTx.scaledPicoseconds.lsb. %d\n", (unsigned int)ptpClock->grandmasterDeltaTx.scaledPicoseconds.lsb);

	      DBGM(" deltaRx.scaledPicoseconds.msb. %d\n", (unsigned int)ptpClock->grandmasterDeltaRx.scaledPicoseconds.msb);
	      DBGM(" deltaRx.scaledPicoseconds.lsb. %d\n", (unsigned int)ptpClock->grandmasterDeltaRx.scaledPicoseconds.lsb);

	      break;

	    default:
	      //no data
	      break;
	  }
	}
	DBGM("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
	//DBG("WR management message: actionField = 0x%x tlv_type = 0x%x  wr_managementId = 0x%x\n",management->actionField, tlv_type, *wr_managementId);
}

#endif /*WRPTPv2*/

/*Unpack WR Management message from IN buffer of ptpClock to msgtmp.Announce*/
void msgUnpackWRManagement(void *buf,MsgManagement *management, Enumeration16 *wr_managementId, PtpClock *ptpClock )
{


	UInteger16 len = (UInteger16)get_be16(buf+2);

	memcpy(management->targetPortIdentity.clockIdentity,(buf+34),CLOCK_IDENTITY_LENGTH);
	management->targetPortIdentity.portNumber = (UInteger16)get_be16(buf+42);

	management->startingBoundaryHops = *(Integer8*)(buf+44);
	management->boundaryHops         = *(Integer8*)(buf+45);
	management->actionField          = *(Enumeration4*)(buf+46);

	if(management->actionField != WR_CMD)
	{
	  DBG("handle Management msg, failed, This is not a White Rabbit Command, actionField = 0x%x\n",management->actionField);
	  return;
	}

	Integer16 tlv_type = flip16(*(Integer16*)(buf+48));

	if(tlv_type != WR_TLV_TYPE)
	{
	  DBG("handle Management msg, failed, unrecognized TLV type in WR_CMD management, tlv_type = 0x%x \n",tlv_type);
	  return;
	}
	*wr_managementId = flip16(*(Enumeration16*)(buf+52));


	DBGM("------------ msgUnpackWRManagement-------\n");
	DBGM(" target PortUuid............... %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n",
	    management->targetPortIdentity.clockIdentity[0],
	    management->targetPortIdentity.clockIdentity[1],
	    management->targetPortIdentity.clockIdentity[2],
	    management->targetPortIdentity.clockIdentity[3],
	    management->targetPortIdentity.clockIdentity[4],
	    management->targetPortIdentity.clockIdentity[5],
	    management->targetPortIdentity.clockIdentity[6],
	    management->targetPortIdentity.clockIdentity[7]
	    );
	DBGM(" target PortId................. %u\n", management->targetPortIdentity.portNumber);
	DBGM(" management CMD................ %u\n", management->actionField);
	DBGM(" tlv type...................... 0x%x\n", tlv_type);
	DBGM(" management ID................. 0x%x\n", *wr_managementId);

	/*This is not nice way of doing it, need to be changed later !!!!!*/
	if(len > WR_MANAGEMENT_LENGTH)
	{
 	  switch(*wr_managementId)
 	  {
#ifdef NEW_SINGLE_WRFSM
	    case CALIBRATE:

	      ptpClock->otherNodeCalibrationSendPattern= get_be16(buf+54);
	      ptpClock->otherNodeCalibrationPeriod     = get_be32(buf+56);
	      ptpClock->otherNodeCalibrationPattern    = get_be32(buf+60);
	      ptpClock->otherNodeCalibrationPatternLen = get_be16(buf+64);

	      if(ptpClock->otherNodeCalibrationSendPattern & SEND_CALIBRATION_PATTERN)
		DBGM(" calibrationSendPattern........ TRUE \n");
	      else
		DBGM(" calibrationSendPattern........ FALSE \n");

#else
 	    case MASTER_CALIBRATE:
 	    case SLAVE_CALIBRATE:

	      ptpClock->otherNodeCalibrationPeriod     = get_be32(buf+54);
	      ptpClock->otherNodeCalibrationPattern    = get_be32(buf+58);
	      ptpClock->otherNodeCalibrationPatternLen = get_be16(buf+62);

#endif

	      DBGM(" calibrationPeriod............. %u [us]\n", ptpClock->calibrationPeriod);
	      DBGM(" calibrationPattern............ %s \n", printf_bits(ptpClock->calibrationPattern));
	      DBGM(" calibrationPatternLen......... %u [bits]\n", ptpClock->calibrationPatternLen);

	      break;

#ifdef NEW_SINGLE_WRFSM
	    case CALIBRATED:
#else
	    case MASTER_CALIBRATED:
	    case SLAVE_CALIBRATED:
#endif
	      /*delta TX*/
	      ptpClock->grandmasterDeltaTx.scaledPicoseconds.msb = get_be32(buf+54);
	      ptpClock->grandmasterDeltaTx.scaledPicoseconds.lsb = get_be32(buf+58);

	      /*delta RX*/
	      ptpClock->grandmasterDeltaRx.scaledPicoseconds.msb = get_be32(buf+62);
	      ptpClock->grandmasterDeltaRx.scaledPicoseconds.lsb = get_be32(buf+66);

	      DBGM(" deltaTx.scaledPicoseconds.msb. %d\n", (unsigned int)ptpClock->grandmasterDeltaTx.scaledPicoseconds.msb);
	      DBGM(" deltaTx.scaledPicoseconds.lsb. %d\n", (unsigned int)ptpClock->grandmasterDeltaTx.scaledPicoseconds.lsb);

	      DBGM(" deltaRx.scaledPicoseconds.msb. %d\n", (unsigned int)ptpClock->grandmasterDeltaRx.scaledPicoseconds.msb);
	      DBGM(" deltaRx.scaledPicoseconds.lsb. %d\n", (unsigned int)ptpClock->grandmasterDeltaRx.scaledPicoseconds.lsb);

	      break;

	    default:
	      //no data
	      break;
	  }
	}
	DBGM("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
	//DBG("WR management message: actionField = 0x%x tlv_type = 0x%x  wr_managementId = 0x%x\n",management->actionField, tlv_type, *wr_managementId);
}

