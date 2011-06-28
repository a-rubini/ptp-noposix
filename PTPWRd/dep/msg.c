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
	header->sequenceId                      = get_be16(buf+30);
	header->controlField                    = (*(UInteger8*)(buf+32));
	header->logMessageInterval              = (*(Integer8*)(buf+33));
	
	DBGM(" correctionfield.msb........... %d\n", header->correctionfield.msb);
	DBGM(" correctionfield.lsb........... %d\n", (unsigned int)header->correctionfield.lsb);
	DBGM(" clockIdentity................. %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n",
	    header->sourcePortIdentity.clockIdentity[0], header->sourcePortIdentity.clockIdentity[1],
	    header->sourcePortIdentity.clockIdentity[2], header->sourcePortIdentity.clockIdentity[3],
	    header->sourcePortIdentity.clockIdentity[4], header->sourcePortIdentity.clockIdentity[5],
	    header->sourcePortIdentity.clockIdentity[6], header->sourcePortIdentity.clockIdentity[7]
	    );

}
/*Pack header message into OUT buffer of ptpPortDS*/
void msgPackHeader(void *buf, PtpPortDS *ptpPortDS)
{
	Nibble transport                        = 0x0; //0x80; //(spec annex D)
	*(UInteger8*)(buf+0)                    = transport;
	*(UInteger4*)(buf+1)                    = ptpPortDS->versionNumber;
	*(UInteger8*)(buf+4)                    = ptpPortDS->ptpClockDS->domainNumber;

	if (ptpPortDS->ptpClockDS->twoStepFlag)
		*(UInteger8*)(buf+6)            = TWO_STEP_FLAG;

	memset((buf+8),0,8);
	memcpy((buf+20),ptpPortDS->portIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH);

	put_be16(buf+28,ptpPortDS->portIdentity.portNumber);

	*(UInteger8*)(buf+33)                   = 0x7F; //Default value (spec Table 24)

	DBGM("------------ msgPackHeader --------\n");
	DBGM(" transportSpecific............. %u\n", transport);
	DBGM(" versionPTP.................... %u\n", ptpPortDS->versionNumber);
	DBGM(" domainNumber.................. %u\n", ptpPortDS->ptpClockDS->domainNumber);
	if (ptpPortDS->ptpClockDS->twoStepFlag)
	  DBGM(" flagField..................... %x\n", TWO_STEP_FLAG);
	else
	  DBGM(" flagField..................... %x\n", 0);
	DBGM(" clockIdentity................. %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n",
	    ptpPortDS->portIdentity.clockIdentity[0], ptpPortDS->portIdentity.clockIdentity[1],
	    ptpPortDS->portIdentity.clockIdentity[2], ptpPortDS->portIdentity.clockIdentity[3],
	    ptpPortDS->portIdentity.clockIdentity[4], ptpPortDS->portIdentity.clockIdentity[5],
	    ptpPortDS->portIdentity.clockIdentity[6], ptpPortDS->portIdentity.clockIdentity[7]);
	DBGM(" portNumber.................... %d\n", ptpPortDS->portIdentity.portNumber);
	DBGM(" logMessageInterval............ %d\n", 0x7F);
	DBGM("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");

}



/*Pack SYNC message into OUT buffer of ptpPortDS*/
void msgPackSync(void *buf,Timestamp *originTimestamp,PtpPortDS *ptpPortDS)
{
	/*changes in header*/
	*(char*)(buf+0)= *(char*)(buf+0) & 0xF0; //RAZ messageType
	*(char*)(buf+0)= *(char*)(buf+0) | 0x00; //Table 19
	put_be16(buf + 2, SYNC_LENGTH);
	*(UInteger16*)(buf+30)=flip16(ptpPortDS->sentSyncSequenceId);
	*(UInteger8*)(buf+32)=0x00; //Table 23
	*(Integer8*)(buf+33) = ptpPortDS->logSyncInterval;
	memset((buf+8),0,8);

	/*Sync message*/
	put_be16(buf+34, originTimestamp->secondsField.msb);
	put_be32(buf+36, originTimestamp->secondsField.lsb);
	put_be32(buf+40, originTimestamp->nanosecondsField);

	DBGM("------------ msgPackSync ----------\n");
	DBGM(" messageLength................. %u\n", SYNC_LENGTH);
	DBGM(" sentSyncSequenceId............ %u\n", ptpPortDS->sentSyncSequenceId);
	DBGM(" logSyncInterval............... %u\n", ptpPortDS->logSyncInterval);
	DBGM(" clockIdentity................. %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n",
	    ptpPortDS->portIdentity.clockIdentity[0], ptpPortDS->portIdentity.clockIdentity[1],
	    ptpPortDS->portIdentity.clockIdentity[2], ptpPortDS->portIdentity.clockIdentity[3],
	    ptpPortDS->portIdentity.clockIdentity[4], ptpPortDS->portIdentity.clockIdentity[5],
	    ptpPortDS->portIdentity.clockIdentity[6], ptpPortDS->portIdentity.clockIdentity[7]);
	DBGM(" portNumber.................... %d\n", ptpPortDS->portIdentity.portNumber);
	DBGM(" originTimestamp.secs.msb...... %d\n", originTimestamp->secondsField.msb);
	DBGM(" originTimestamp.secs.lsb...... %d\n", originTimestamp->secondsField.lsb);
	DBGM(" originTimestamp.nsecs......... %d\n",  originTimestamp->nanosecondsField);
	DBGM("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");

}

/*Unpack Sync message from IN buffer */
void msgUnpackSync(void *buf,MsgSync *sync)
{
	sync->originTimestamp.secondsField.msb = get_be16(buf+34);
	sync->originTimestamp.secondsField.lsb = get_be32(buf+36);
	sync->originTimestamp.nanosecondsField = get_be32(buf+40);


}



/*Pack Announce message into OUT buffer of ptpPortDS*/
void msgPackAnnounce(void *buf,PtpPortDS *ptpPortDS)
{
	/*changes in header*/
	*(char*)(buf+0)= *(char*)(buf+0) & 0xF0; //RAZ messageType
	*(char*)(buf+0)= *(char*)(buf+0) | 0x0B; //Table 19
	if (ptpPortDS->wrConfig != NON_WR && ptpPortDS->wrConfig != WR_S_ONLY)
	   put_be16(buf + 2,WR_ANNOUNCE_LENGTH);
	else
	   put_be16(buf + 2,ANNOUNCE_LENGTH);

	*(UInteger16*)(buf+30)=flip16(ptpPortDS->sentAnnounceSequenceId);
	*(UInteger8*)(buf+32)=0x05; //Table 23
	*(Integer8*)(buf+33) = ptpPortDS->logAnnounceInterval;

	/*Announce message*/
	memset((buf+34),0,10);
	*(Integer16*)(buf+44)=flip16(ptpPortDS->ptpClockDS->currentUtcOffset);

	*(UInteger8*)(buf+47)=ptpPortDS->ptpClockDS->grandmasterPriority1;
	*(UInteger8*)(buf+48)=ptpPortDS->ptpClockDS->clockQuality.clockClass;
	*(Enumeration8*)(buf+49)=ptpPortDS->ptpClockDS->clockQuality.clockAccuracy;
	*(UInteger16*)(buf+50)=flip16(ptpPortDS->ptpClockDS->clockQuality.offsetScaledLogVariance);
	*(UInteger8*)(buf+52)=ptpPortDS->ptpClockDS->grandmasterPriority2;
	memcpy((buf+53),ptpPortDS->ptpClockDS->grandmasterIdentity,CLOCK_IDENTITY_LENGTH);
	
	/* I had a problem with  stepRemoved overwriting the last word of grandmasterIdentity, 
	 * now it's maybe not nice but it works :),
	 */
	*(UInteger8*)(buf+61)= 0xFF & (ptpPortDS->ptpClockDS->stepsRemoved);//flip16(ptpPortDS->ptpClockDS->stepsRemoved);
	*(UInteger8*)(buf+62)= 0xFF & (ptpPortDS->ptpClockDS->stepsRemoved >> 8);//flip16(ptpPortDS->ptpClockDS->stepsRemoved);
	*(Enumeration8*)(buf+63)=ptpPortDS->ptpClockDS->timeSource;

	/*
	 * White rabbit message in the suffix of PTP announce message
	 */
	UInteger16 wr_flags = 0;
	if (ptpPortDS->wrConfig != NON_WR && ptpPortDS->wrConfig != WR_S_ONLY)	
	{

  	  *(UInteger16*)(buf+64) = flip16(TLV_TYPE_ORG_EXTENSION);
	  *(UInteger16*)(buf+66) = flip16(WR_ANNOUNCE_TLV_LENGTH);
	  // CERN's OUI: WR_TLV_ORGANIZATION_ID, how to flip bits?
	  *(UInteger16*)(buf+68) = flip16((WR_TLV_ORGANIZATION_ID >> 8));
	  *(UInteger16*)(buf+70) = flip16((0xFFFF & (WR_TLV_ORGANIZATION_ID << 8 | WR_TLV_MAGIC_NUMBER >> 8)));
	  *(UInteger16*)(buf+72) = flip16((0xFFFF & (WR_TLV_MAGIC_NUMBER    << 8 | WR_TLV_WR_VERSION_NUMBER)));
	  //wrMessageId
	  *(UInteger16*)(buf+74) = flip16(ANN_SUFIX);
	  wr_flags = wr_flags | ptpPortDS->wrConfig;

	  

	  if (ptpPortDS->calibrated)
	    wr_flags = WR_IS_CALIBRATED | wr_flags;

	  if (ptpPortDS->wrModeON)
	    wr_flags = WR_IS_WR_MODE | wr_flags;
	  *(UInteger16*)(buf+76) = flip16(wr_flags);
	}

	DBGM("------------ msgPackAnnounce ----------\n");
	if (ptpPortDS->wrConfig != NON_WR && ptpPortDS->wrConfig != WR_S_ONLY)
	  DBGM(" messageLength................. %u\n", WR_ANNOUNCE_LENGTH);
	else
	  DBGM(" messageLength................. %u\n", ANNOUNCE_LENGTH);
	DBGM(" sentSyncSequenceId............ %u\n", ptpPortDS->sentAnnounceSequenceId);
	DBGM(" logSyncInterval............... %u\n", ptpPortDS->logAnnounceInterval);
	DBGM(" clockIdentity................. %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n",
	    ptpPortDS->portIdentity.clockIdentity[0], ptpPortDS->portIdentity.clockIdentity[1],
	    ptpPortDS->portIdentity.clockIdentity[2], ptpPortDS->portIdentity.clockIdentity[3],
	    ptpPortDS->portIdentity.clockIdentity[4], ptpPortDS->portIdentity.clockIdentity[5],
	    ptpPortDS->portIdentity.clockIdentity[6], ptpPortDS->portIdentity.clockIdentity[7]);
	DBGM(" portNumber.................... %d\n", ptpPortDS->portIdentity.portNumber);
	DBGM(" currentUtcOffset.............. %d\n", ptpPortDS->ptpClockDS->currentUtcOffset);
	DBGM(" grandmasterPriority1.......... %d\n", ptpPortDS->ptpClockDS->grandmasterPriority1);
	DBGM(" clockClass.................... %d\n", ptpPortDS->ptpClockDS->clockQuality.clockClass);
	DBGM(" clockAccuracy................. %d\n", ptpPortDS->ptpClockDS->clockQuality.clockAccuracy);
	DBGM(" offsetScaledLogVariance....... %d\n", ptpPortDS->ptpClockDS->clockQuality.offsetScaledLogVariance);
	DBGM(" grandmasterPriority2.......... %d\n", ptpPortDS->ptpClockDS->grandmasterPriority2);
	DBGM(" grandmasterIdentity........... %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n",
	    ptpPortDS->ptpClockDS->grandmasterIdentity[0], ptpPortDS->ptpClockDS->grandmasterIdentity[1],
	    ptpPortDS->ptpClockDS->grandmasterIdentity[2], ptpPortDS->ptpClockDS->grandmasterIdentity[3],
	    ptpPortDS->ptpClockDS->grandmasterIdentity[4], ptpPortDS->ptpClockDS->grandmasterIdentity[5],
	    ptpPortDS->ptpClockDS->grandmasterIdentity[6], ptpPortDS->ptpClockDS->grandmasterIdentity[7]);
	DBGM(" stepsRemoved.................. %d\n", ptpPortDS->ptpClockDS->stepsRemoved);
	DBGM(" timeSource.................... %d\n", ptpPortDS->ptpClockDS->timeSource);
	if (ptpPortDS->wrConfig != NON_WR && ptpPortDS->wrConfig != WR_S_ONLY)
	{

	  DBGM(" [WR suffix] tlv_type.......... 0x%x\n", TLV_TYPE_ORG_EXTENSION);
	  DBGM(" [WR suffix] tlv_length........ %d\n",   WR_ANNOUNCE_TLV_LENGTH);
	  DBGM(" [WR suffix] tlv_organizID .....0x%x\n", WR_TLV_ORGANIZATION_ID);
	  DBGM(" [WR suffix] tlv_magicNumber... 0x%x\n", WR_TLV_MAGIC_NUMBER);
	  DBGM(" [WR suffix] tlv_versionNumber. 0x%x\n", WR_TLV_WR_VERSION_NUMBER);
	  DBGM(" [WR suffix] tlv_wrMessageID... 0x%x\n", ANN_SUFIX);
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


}

/*Unpack Announce message from IN buffer of ptpPortDS to msgtmp.Announce*/
void msgUnpackAnnounce(void *buf,MsgAnnounce *announce,  MsgHeader *header)
{
	UInteger16 tlv_type;
	UInteger32 tlv_organizationID;
	UInteger16 tlv_magicNumber;
	UInteger16 tlv_versionNumber;
	UInteger16 tlv_wrMessageID;

	announce->originTimestamp.secondsField.msb = flip16(*(UInteger16*)(buf+34));
	announce->originTimestamp.secondsField.lsb = flip32(*(UInteger32*)(buf+36));
	announce->originTimestamp.nanosecondsField = flip32(*(UInteger32*)(buf+40));
	announce->currentUtcOffset = flip16(*(UInteger16*)(buf+44));

	announce->grandmasterPriority1 = *(UInteger8*)(buf+47);
	announce->grandmasterClockQuality.clockClass = *(UInteger8*)(buf+48);
	announce->grandmasterClockQuality.clockAccuracy = *(Enumeration8*)(buf+49);
	announce->grandmasterClockQuality.offsetScaledLogVariance = get_be16(buf+50);
	announce->grandmasterPriority2 = *(UInteger8*)(buf+52);
	memcpy(announce->grandmasterIdentity,(buf+53),CLOCK_IDENTITY_LENGTH);
	//
	announce->stepsRemoved = 0xFFFF & ((*(UInteger8*)(buf+61)) | (*(UInteger8*)(buf+62) << 8));
	announce->timeSource = *(Enumeration8*)(buf+63);

	/*White Rabbit- only flags in a reserved space of announce message*/
	UInteger16  messageLen = (UInteger16)get_be16(buf + 2);
	//check if there is msg suffix
	if(messageLen > ANNOUNCE_LENGTH)
	{
	  tlv_type   = (UInteger16)get_be16(buf+64);
	  tlv_organizationID = flip16(*(UInteger16*)(buf+68)) << 8;
	  tlv_organizationID = flip16(*(UInteger16*)(buf+70)) >> 8  | tlv_organizationID;
	  tlv_magicNumber    = 0xFF00 & (flip16(*(UInteger16*)(buf+70)) << 8);
	  tlv_magicNumber    = flip16(*(UInteger16*)(buf+72)) >>  8 | tlv_magicNumber;
	  tlv_versionNumber  = 0xFF & flip16(*(UInteger16*)(buf+72));
	  tlv_wrMessageID    = flip16(*(UInteger16*)(buf+74));

	  if(tlv_type           == TLV_TYPE_ORG_EXTENSION   && \
	     tlv_organizationID == WR_TLV_ORGANIZATION_ID   && \
	     tlv_magicNumber    == WR_TLV_MAGIC_NUMBER      && \
	     tlv_versionNumber  == WR_TLV_WR_VERSION_NUMBER && \
	     tlv_wrMessageID    == ANN_SUFIX)
	  {
	    announce->wr_flags   = (UInteger16)get_be16(buf+76);
	  }

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
	  DBGM(" [WR suffix] tlv_organizID .....0x%x\n", tlv_organizationID);
	  DBGM(" [WR suffix] tlv_magicNumber... 0x%x\n", tlv_magicNumber);
	  DBGM(" [WR suffix] tlv_versionNumber. 0x%x\n", tlv_versionNumber);
	  DBGM(" [WR suffix] tlv_wrMessageID... 0x%x\n", tlv_wrMessageID);
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

}


/*pack Follow_up message into OUT buffer of ptpPortDS*/
void msgPackFollowUp(void *buf,PtpPortDS *ptpPortDS)
//void msgPackFollowUp(void *buf,Timestamp *preciseOriginTimestamp,PtpPortDS *ptpPortDS)
{
	/*changes in header*/
	*(char*)(buf+0)= *(char*)(buf+0) & 0xF0; //RAZ messageType
	*(char*)(buf+0)= *(char*)(buf+0) | 0x08; //Table 19
	put_be16(buf + 2, FOLLOW_UP_LENGTH);
	*(UInteger16*)(buf+30)=flip16(ptpPortDS->sentSyncSequenceId-1);//sentSyncSequenceId has already been incremented in "issueSync"
	*(UInteger8*)(buf+32)=0x02; //Table 23
	*(Integer8*)(buf+33) = ptpPortDS->logSyncInterval;

	/*Follow_up message*/
	*(UInteger16*)(buf+34) = flip16(0xFFFF & (ptpPortDS->synch_tx_ts.utc >> 32));
	put_be32(buf+36,0xFFFFFFFF & ptpPortDS->synch_tx_ts.utc);
	put_be32(buf+40, ptpPortDS->synch_tx_ts.nsec);

	/*
	 * by ML: follow up msg can also have correction field,
	 * it's just not implemented here
	 */

	DBGM("------------ msgPackFollowUp-------\n");
	DBGM(" syncSequenceId ............... %u\n", ptpPortDS->sentSyncSequenceId-1);
	DBGM(" logMinDelayReqInterval ....... %u\n", ptpPortDS->logSyncInterval);
	DBGM(" syncTransTimestamp.secs.hi.... %d\n", (int)(0xFFFF & (ptpPortDS->synch_tx_ts.utc >> 32)));
	DBGM(" syncTransTimestamp.secs.lo.... %d\n", (int)(0xFFFFFFFF & ptpPortDS->synch_tx_ts.utc));
	DBGM(" syncTransTimestamp.nsecs...... %d\n", ptpPortDS->synch_tx_ts.nsec);
	DBGM("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");

}

/*Unpack Follow_up message from IN buffer of ptpPortDS to msgtmp.follow*/
void msgUnpackFollowUp(void *buf,MsgFollowUp *follow)
{
	follow->preciseOriginTimestamp.secondsField.msb = get_be16(buf+34);
	follow->preciseOriginTimestamp.secondsField.lsb = get_be32(buf+36);
	follow->preciseOriginTimestamp.nanosecondsField = get_be32(buf+40);


}


/*pack PdelayReq message into OUT buffer of ptpPortDS*/
void msgPackPDelayReq(void *buf,Timestamp *originTimestamp,PtpPortDS *ptpPortDS)
{
	/*changes in header*/
	*(char*)(buf+0)= *(char*)(buf+0) & 0xF0; //RAZ messageType
	*(char*)(buf+0)= *(char*)(buf+0) | 0x02; //Table 19
	put_be16(buf + 2, PDELAY_REQ_LENGTH);
	*(UInteger16*)(buf+30)= flip16(ptpPortDS->sentPDelayReqSequenceId);
	*(UInteger8*)(buf+32) = 0x05; //Table 23
	*(Integer8*)(buf+33) = 0x7F; //Table 24
	memset((buf+8),0,8);

	/*Pdelay_req message*/
	*(UInteger16*)(buf+34) = flip16(originTimestamp->secondsField.msb);
	*(UInteger32*)(buf+36) = flip32(originTimestamp->secondsField.lsb);
	*(UInteger32*)(buf+40) = flip32(originTimestamp->nanosecondsField);

	memset((buf+44),0,10); // RAZ reserved octets
}

/*pack delayReq message into OUT buffer of ptpPortDS*/
void msgPackDelayReq(void *buf,Timestamp *originTimestamp,PtpPortDS *ptpPortDS)
{
	/*changes in header*/
	*(char*)(buf+0)= *(char*)(buf+0) & 0xF0; //RAZ messageType
	*(char*)(buf+0)= *(char*)(buf+0) | 0x01; //Table 19
	put_be16(buf + 2, DELAY_REQ_LENGTH);
	*(UInteger16*)(buf+30)= flip16(ptpPortDS->sentDelayReqSequenceId);

	*(UInteger8*)(buf+32) = 0x01; //Table 23
	*(Integer8*)(buf+33) = 0x7F; //Table 24
	memset((buf+8),0,8);

	/*delay_req message*/
	put_be16(buf+34, originTimestamp->secondsField.msb);
	put_be32(buf+36, originTimestamp->secondsField.lsb);
	put_be32(buf+40, originTimestamp->nanosecondsField);

	DBGM("------------ msgPackDelayReq-------\n");
	DBGM(" delayReqSequenceId ........... %u\n", ptpPortDS->sentDelayReqSequenceId);
	DBGM(" originTimestamp.secs.msb...... %d\n", originTimestamp->secondsField.msb);
	DBGM(" originTimestamp.secs.lsb...... %d\n", originTimestamp->secondsField.lsb);
	DBGM(" originTimestamp.nsecs......... %d\n", originTimestamp->nanosecondsField);
	DBGM("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");


}

/*pack delayResp message into OUT buffer of ptpPortDS*/
void msgPackDelayResp(void *buf,MsgHeader *header,PtpPortDS *ptpPortDS)
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
	*(Integer8*)(buf+33) = ptpPortDS->logMinDelayReqInterval; //Table 24

	/*Pdelay_resp message*/
	*(UInteger16*)(buf+34) = flip16(0xFFFF & (ptpPortDS->current_rx_ts.utc >> 32));
	put_be32(buf+36, 0xFFFFFFFF & ptpPortDS->current_rx_ts.utc);
	put_be32(buf+40, ptpPortDS->current_rx_ts.nsec);

	memcpy((buf+44),header->sourcePortIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH);

	put_be16(buf + 52, header->sourcePortIdentity.portNumber);


	DBGM("------------ msgPackDelayResp-------\n");
	DBGM(" correctionfield.msb .......... %d\n", header->correctionfield.msb);
	DBGM(" correctionfield.lsb........... %d\n", header->correctionfield.lsb);
	DBGM(" sequenceId ................... %u\n", header->sequenceId);
	DBGM(" logMinDelayReqInterval ....... %u\n", ptpPortDS->logMinDelayReqInterval);
	DBGM(" delayReceiptTimestamp.secs.hi. %d\n", (int)(0xFFFF & (ptpPortDS->current_rx_ts.utc >> 32)));
	DBGM(" delayReceiptTimestamp.secs.lo. %d\n", (int)(0xFFFFFFFF & ptpPortDS->current_rx_ts.utc));
	DBGM(" delayReceiptTimestamp.nsecs... %d\n", ptpPortDS->current_rx_ts.nsec);
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

}

/*pack PdelayResp message into OUT buffer of ptpPortDS*/
void msgPackPDelayResp(void *buf,MsgHeader *header,Timestamp *requestReceiptTimestamp,PtpPortDS *ptpPortDS)
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

	*(UInteger16*)(buf+34) = flip16(0xFFFF & (ptpPortDS->current_rx_ts.utc >> 32 ));
	put_be32(buf+36,0xFFFFFFFF & ptpPortDS->current_rx_ts.utc);
	put_be32(buf+40, ptpPortDS->current_rx_ts.nsec);

	memcpy((buf+44),header->sourcePortIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH);
	put_be16(buf + 52, header->sourcePortIdentity.portNumber);

}


/*Unpack delayReq message from IN buffer of ptpPortDS to msgtmp.req*/
void msgUnpackDelayReq(void *buf,MsgDelayReq *delayreq)
{
	delayreq->originTimestamp.secondsField.msb = get_be16(buf+34);
	delayreq->originTimestamp.secondsField.lsb = get_be32(buf+36);
	delayreq->originTimestamp.nanosecondsField = get_be32(buf+40);


}


/*Unpack PdelayReq message from IN buffer of ptpPortDS to msgtmp.req*/
void msgUnpackPDelayReq(void *buf,MsgPDelayReq *pdelayreq)
{
	pdelayreq->originTimestamp.secondsField.msb = flip16(*(UInteger16*)(buf+34));
	pdelayreq->originTimestamp.secondsField.lsb = get_be32(buf+36);
	pdelayreq->originTimestamp.nanosecondsField = get_be32(buf+40);
}


/*Unpack delayResp message from IN buffer of ptpPortDS to msgtmp.presp*/
void msgUnpackDelayResp(void *buf,MsgDelayResp *resp)
{
	resp->receiveTimestamp.secondsField.msb = get_be16(buf+34);
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

}


/*Unpack PdelayResp message from IN buffer of ptpPortDS to msgtmp.presp*/
void msgUnpackPDelayResp(void *buf,MsgPDelayResp *presp)
{
	presp->requestReceiptTimestamp.secondsField.msb = flip16(*(UInteger16*)(buf+34));
	presp->requestReceiptTimestamp.secondsField.lsb = get_be32(buf+36);
	presp->requestReceiptTimestamp.nanosecondsField = get_be32(buf+40);

	memcpy(presp->requestingPortIdentity.clockIdentity,(buf+44),CLOCK_IDENTITY_LENGTH);
	presp->requestingPortIdentity.portNumber = (UInteger16)get_be16(buf+52);



}

/*pack PdelayRespfollowup message into OUT buffer of ptpPortDS*/
void msgPackPDelayRespFollowUp(void *buf,MsgHeader *header,Timestamp *responseOriginTimestamp,PtpPortDS *ptpPortDS)
{
	/*changes in header*/
	*(char*)(buf+0)= *(char*)(buf+0) & 0xF0; //RAZ messageType
	*(char*)(buf+0)= *(char*)(buf+0) | 0x0A; //Table 19
	put_be16(buf + 2, PDELAY_RESP_FOLLOW_UP_LENGTH);
	*(UInteger16*)(buf+30)= flip16(ptpPortDS->PdelayReqHeader.sequenceId);
	*(UInteger8*)(buf+32) = 0x05; //Table 23
	*(Integer8*)(buf+33) = 0x7F; //Table 24

	/*Copy correctionField of PdelayReqMessage*/
	*(Integer32*)(buf+8) = flip32(header->correctionfield.msb);
	*(Integer32*)(buf+12) = flip32(header->correctionfield.lsb);

	*(UInteger16*)(buf+34) = flip16(0xFFFF & (ptpPortDS->pDelayResp_tx_ts.utc >> 32));
	put_be32(buf+36, 0xFFFFFFFF &  ptpPortDS->pDelayResp_tx_ts.utc);
	put_be32(buf+40, ptpPortDS->pDelayResp_tx_ts.nsec);

	memcpy((buf+44),header->sourcePortIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH);
	put_be16(buf + 52, header->sourcePortIdentity.portNumber);
}

/*Unpack PdelayResp message from IN buffer of ptpPortDS to msgtmp.presp*/
void msgUnpackPDelayRespFollowUp(void *buf,MsgPDelayRespFollowUp *prespfollow)
{
	prespfollow->responseOriginTimestamp.secondsField.msb = flip16(*(UInteger16*)(buf+34));
	prespfollow->responseOriginTimestamp.secondsField.lsb = get_be32(buf+36);
	prespfollow->responseOriginTimestamp.nanosecondsField = get_be32(buf+40);

	memcpy(prespfollow->requestingPortIdentity.clockIdentity,(buf+44),CLOCK_IDENTITY_LENGTH);
	prespfollow->requestingPortIdentity.portNumber = (UInteger16)get_be16(buf+52);
}


/* White Rabbit: packing WR Signaling messages*/
UInteger16 msgPackWRSignalingMsg(void *buf,PtpPortDS *ptpPortDS, Enumeration16 wrMessageID)
{

	if (ptpPortDS->wrMode == NON_WR || \
	    wrMessageID          == ANN_SUFIX)
	  return 0;

	/*changes in header*/
	*(char*)(buf+0)= *(char*)(buf+0) & 0xF0; //RAZ messageType
	*(char*)(buf+0)= *(char*)(buf+0) | 0x0C; //Table 19 -> signaling

	*(UInteger8*)(buf+32)=0x05; //Table 23 -> all other

	//target portIdentity
	memcpy((buf+34),ptpPortDS->ptpClockDS->parentPortIdentity.clockIdentity,CLOCK_IDENTITY_LENGTH);
	put_be16(buf + 42,ptpPortDS->ptpClockDS->parentPortIdentity.portNumber);


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
	    ptpPortDS->ptpClockDS->parentPortIdentity.clockIdentity[0],
	    ptpPortDS->ptpClockDS->parentPortIdentity.clockIdentity[1],
	    ptpPortDS->ptpClockDS->parentPortIdentity.clockIdentity[2],
	    ptpPortDS->ptpClockDS->parentPortIdentity.clockIdentity[3],
	    ptpPortDS->ptpClockDS->parentPortIdentity.clockIdentity[4],
	    ptpPortDS->ptpClockDS->parentPortIdentity.clockIdentity[5],
	    ptpPortDS->ptpClockDS->parentPortIdentity.clockIdentity[6],
	    ptpPortDS->ptpClockDS->parentPortIdentity.clockIdentity[7]
	    );
	DBGM(" recipient's PortId............ %u\n", ptpPortDS->ptpClockDS->parentPortIdentity.portNumber);
	DBGM(" tlv_type...................... 0x%x\n", TLV_TYPE_ORG_EXTENSION);
	//DBGM(" tlv_length.................... %d\n",   WR_ANNOUNCE_TLV_LENGTH);
	DBGM(" tlv_organizID ................ 0x%x\n", WR_TLV_ORGANIZATION_ID);
	DBGM(" tlv_magicNumber............... 0x%x\n", WR_TLV_MAGIC_NUMBER);
	DBGM(" tlv_versionNumber............. 0x%x\n", WR_TLV_WR_VERSION_NUMBER);
	DBGM(" tlv_wrMessageID............... 0x%x\n", wrMessageID);	

 	UInteger16 len = 0;
 	switch(wrMessageID)
 	{
	  case CALIBRATE: 

	    if(ptpPortDS->calibrated)
	    {
	      put_be16(buf+56, 0x0000);
	      DBGM(" calibrationSendPattern........ FALSE \n");
	    }
	    else
	    {
	      put_be16(buf+56, 0x0001);
	      DBGM(" calibrationSendPattern........ TRUE \n");
	    }
	    put_be32(buf+58, ptpPortDS->calPeriod);
	    len = 20;


	    DBGM(" calPeriod..................... %u [us]\n", ptpPortDS->calPeriod);


	    break;

	  case CALIBRATED: //new fsm


	    /*delta TX*/
	    put_be32(buf+56, ptpPortDS->deltaTx.scaledPicoseconds.msb);
	    put_be32(buf+60, ptpPortDS->deltaTx.scaledPicoseconds.lsb);

	    /*delta RX*/
	    put_be32(buf+64, ptpPortDS->deltaRx.scaledPicoseconds.msb);
	    put_be32(buf+68, ptpPortDS->deltaRx.scaledPicoseconds.lsb);

	    DBGM(" deltaTx.scaledPicoseconds.msb. %d\n", (unsigned int)ptpPortDS->deltaTx.scaledPicoseconds.msb);
	    DBGM(" deltaTx.scaledPicoseconds.lsb. %d\n", (unsigned int)ptpPortDS->deltaTx.scaledPicoseconds.lsb);

	    DBGM(" deltaRx.scaledPicoseconds.msb. %d\n", (unsigned int)ptpPortDS->deltaRx.scaledPicoseconds.msb);
	    DBGM(" deltaRx.scaledPicoseconds.lsb. %d\n", (unsigned int)ptpPortDS->deltaRx.scaledPicoseconds.lsb);

	    len = 24;

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



	return (WR_SIGNALING_MSG_BASE_LENGTH + len);
}
/* White Rabbit: unpacking wr signaling messages*/
void msgUnpackWRSignalingMsg(void *buf,MsgSignaling *signalingMsg, Enumeration16 *wrMessageID, PtpPortDS *ptpPortDS )
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
	if(*wrMessageID == CALIBRATE || *wrMessageID == CALIBRATED)
	{
 	  switch(*wrMessageID)
 	  {

	    case CALIBRATE:

	      ptpPortDS->otherNodeCalSendPattern= get_be16(buf+56);
	      ptpPortDS->otherNodeCalPeriod     = get_be32(buf+58);
	      if(ptpPortDS->otherNodeCalSendPattern & SEND_CALIBRATION_PATTERN)
		DBGM(" calibrationSendPattern........ TRUE \n");
	      else
		DBGM(" calibrationSendPattern........ FALSE \n");


	      DBGM(" calPeriod..................... %u [us]\n", ptpPortDS->otherNodeCalPeriod);
	      break;

	    case CALIBRATED:
	      /*delta TX*/
	      ptpPortDS->otherNodeDeltaTx.scaledPicoseconds.msb = get_be32(buf+56);
	      ptpPortDS->otherNodeDeltaTx.scaledPicoseconds.lsb = get_be32(buf+60);

	      /*delta RX*/
	      ptpPortDS->otherNodeDeltaRx.scaledPicoseconds.msb = get_be32(buf+64);
	      ptpPortDS->otherNodeDeltaRx.scaledPicoseconds.lsb = get_be32(buf+68);

	      DBGM(" deltaTx.scaledPicoseconds.msb. %d\n", (unsigned int)ptpPortDS->otherNodeDeltaTx.scaledPicoseconds.msb);
	      DBGM(" deltaTx.scaledPicoseconds.lsb. %d\n", (unsigned int)ptpPortDS->otherNodeDeltaTx.scaledPicoseconds.lsb);

	      DBGM(" deltaRx.scaledPicoseconds.msb. %d\n", (unsigned int)ptpPortDS->otherNodeDeltaRx.scaledPicoseconds.msb);
	      DBGM(" deltaRx.scaledPicoseconds.lsb. %d\n", (unsigned int)ptpPortDS->otherNodeDeltaRx.scaledPicoseconds.lsb);

	      break;

	    default:
	      //no data
	      break;
	  }
	}
	DBGM("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");

}


