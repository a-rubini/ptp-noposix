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

	PTPD_TRACE(TRACE_MSG,"------------ msgUnpackHeader ------\n");
	PTPD_TRACE(TRACE_MSG," transportSpecific............. %u\n", header->transportSpecific);
	PTPD_TRACE(TRACE_MSG," messageType................... %u\n", header->messageType);
	PTPD_TRACE(TRACE_MSG," versionPTP.................... %u\n", header->versionPTP);
	PTPD_TRACE(TRACE_MSG," messageLength................. %u\n", header->messageLength);
	PTPD_TRACE(TRACE_MSG," domainNumber.................. %u\n", header->domainNumber);
	PTPD_TRACE(TRACE_MSG," flagField..................... %02hhx %02hhx\n",
	    header->flagField[0],
	    header->flagField[1]
	    );
	PTPD_TRACE(TRACE_MSG," correctionfield.msb........... %d\n", header->correctionfield.msb);
	PTPD_TRACE(TRACE_MSG," correctionfield.lsb........... %d\n", (unsigned int)header->correctionfield.lsb);
	PTPD_TRACE(TRACE_MSG," clockIdentity................. %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n",
	    header->sourcePortIdentity.clockIdentity[0], header->sourcePortIdentity.clockIdentity[1],
	    header->sourcePortIdentity.clockIdentity[2], header->sourcePortIdentity.clockIdentity[3],
	    header->sourcePortIdentity.clockIdentity[4], header->sourcePortIdentity.clockIdentity[5]
	    );
	PTPD_TRACE(TRACE_MSG," portNumber.................... %d\n", header->sourcePortIdentity.portNumber);
	PTPD_TRACE(TRACE_MSG," sequenceId.................... %d\n", header->sequenceId);
	PTPD_TRACE(TRACE_MSG," control....................... %d\n", header->controlField);
	PTPD_TRACE(TRACE_MSG," logMessageInterval............ %d\n", header->logMessageInterval);
	PTPD_TRACE(TRACE_MSG,"^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");

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

	PTPD_TRACE(TRACE_MSG,"------------ msgPackHeader --------\n");
	PTPD_TRACE(TRACE_MSG," transportSpecific............. %u\n", transport);
	PTPD_TRACE(TRACE_MSG," versionPTP.................... %u\n", ptpClock->versionNumber);
	PTPD_TRACE(TRACE_MSG," domainNumber.................. %u\n", ptpClock->domainNumber);
	if (ptpClock->twoStepFlag)
	  PTPD_TRACE(TRACE_MSG," flagField..................... %x\n", TWO_STEP_FLAG)
	else
	  PTPD_TRACE(TRACE_MSG," flagField..................... %x\n", 0)
	PTPD_TRACE(TRACE_MSG," clockIdentity................. %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n",
	    ptpClock->portIdentity.clockIdentity[0], ptpClock->portIdentity.clockIdentity[1],
	    ptpClock->portIdentity.clockIdentity[2], ptpClock->portIdentity.clockIdentity[3],
	    ptpClock->portIdentity.clockIdentity[4], ptpClock->portIdentity.clockIdentity[5]);
	PTPD_TRACE(TRACE_MSG," portNumber.................... %d\n", ptpClock->portIdentity.portNumber);
	PTPD_TRACE(TRACE_MSG," logMessageInterval............ %d\n", 0x7F);
	PTPD_TRACE(TRACE_MSG,"^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
}



/*Pack SYNC message into OUT buffer of ptpClock*/
void msgPackSync(void *buf,Timestamp *originTimestamp,PtpClock *ptpClock)
{
	/*changes in header*/
	*(char*)(buf+0)= *(char*)(buf+0) & 0xF0; //RAZ messageType
	*(char*)(buf+0)= *(char*)(buf+0) | 0x00; //Table 19
	put_be16(buf + 2, SYNC_LENGTH);
	put_be16(buf+30 , ptpClock->sentSyncSequenceId);
	*(UInteger8*)(buf+32)=0x00; //Table 23
	*(Integer8*)(buf+33) = ptpClock->logSyncInterval;
	memset((buf+8),0,8);

	/*Sync message*/
	put_be16(buf+34, originTimestamp->secondsField.msb);
	put_be32(buf+36, originTimestamp->secondsField.lsb);
	put_be32(buf+40, originTimestamp->nanosecondsField);

	PTPD_TRACE(TRACE_MSG,"------------ msgPackSync ----------\n");
	PTPD_TRACE(TRACE_MSG," messageLength................. %u\n", SYNC_LENGTH);
	PTPD_TRACE(TRACE_MSG," sentSyncSequenceId............ %u\n", ptpClock->sentSyncSequenceId);
	PTPD_TRACE(TRACE_MSG," logSyncInterval............... %u\n", ptpClock->logSyncInterval);
	PTPD_TRACE(TRACE_MSG," clockIdentity................. %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n",
	    ptpClock->portIdentity.clockIdentity[0], ptpClock->portIdentity.clockIdentity[1],
	    ptpClock->portIdentity.clockIdentity[2], ptpClock->portIdentity.clockIdentity[3],
	    ptpClock->portIdentity.clockIdentity[4], ptpClock->portIdentity.clockIdentity[5]);
	PTPD_TRACE(TRACE_MSG," portNumber.................... %d\n", ptpClock->portIdentity.portNumber);
	PTPD_TRACE(TRACE_MSG," originTimestamp.secs.msb...... %d\n", originTimestamp->secondsField.msb);
	PTPD_TRACE(TRACE_MSG," originTimestamp.secs.lsb...... %d\n", originTimestamp->secondsField.lsb);
	PTPD_TRACE(TRACE_MSG," originTimestamp.nsecs......... %d\n",  originTimestamp->nanosecondsField);
	PTPD_TRACE(TRACE_MSG,"^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");

}

/*Unpack Sync message from IN buffer */
void msgUnpackSync(void *buf,MsgSync *sync)
{
	sync->originTimestamp.secondsField.msb = get_be16(buf+34);
	sync->originTimestamp.secondsField.lsb = get_be32(buf+36);
	sync->originTimestamp.nanosecondsField = get_be32(buf+40);

	PTPD_TRACE(TRACE_MSG,"------------ msgUnpackSync ----------\n");
	PTPD_TRACE(TRACE_MSG," originTimestamp.secs.msb...... %d\n", sync->originTimestamp.secondsField.msb);
	PTPD_TRACE(TRACE_MSG," originTimestamp.secs.lsb...... %d\n", sync->originTimestamp.secondsField.lsb);
	PTPD_TRACE(TRACE_MSG," originTimestamp.nsecs......... %d\n", sync->originTimestamp.nanosecondsField);
	PTPD_TRACE(TRACE_MSG,"^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");

}



/*Pack Announce message into OUT buffer of ptpClock*/
void msgPackAnnounce(void *buf,PtpClock *ptpClock)
{
	/*changes in header*/
	*(char*)(buf+0)= *(char*)(buf+0) & 0xF0; //RAZ messageType
	*(char*)(buf+0)= *(char*)(buf+0) | 0x0B; //Table 19

	if (ptpClock->wrNodeMode != NON_WR)
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
	if (ptpClock->wrNodeMode != NON_WR)
	{
	  *(UInteger16*)(buf+64) = flip16(WR_TLV_TYPE);
	  *(UInteger16*)(buf+66) = flip16(WR_ANNOUNCE_TLV_LENGTH);



	  wr_flags = wr_flags | ptpClock->wrNodeMode;

	  if (ptpClock->isCalibrated)
	    wr_flags = WR_IS_CALIBRATED | wr_flags;

	  if (ptpClock->isWRmode)
	    wr_flags = WR_IS_WR_MODE | wr_flags;

	  *(UInteger16*)(buf+68) = flip16(wr_flags);

	}

	PTPD_TRACE(TRACE_MSG,"------------ msgPackAnnounce ----------\n");
	if (ptpClock->wrNodeMode != NON_WR)
	  PTPD_TRACE(TRACE_MSG," messageLength................. %u\n", WR_ANNOUNCE_LENGTH)
	else
	  PTPD_TRACE(TRACE_MSG," messageLength................. %u\n", ANNOUNCE_LENGTH)
	PTPD_TRACE(TRACE_MSG," sentSyncSequenceId............ %u\n", ptpClock->sentAnnounceSequenceId);
	PTPD_TRACE(TRACE_MSG," logSyncInterval............... %u\n", ptpClock->logAnnounceInterval);
	PTPD_TRACE(TRACE_MSG," clockIdentity................. %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n",
	    ptpClock->portIdentity.clockIdentity[0], ptpClock->portIdentity.clockIdentity[1],
	    ptpClock->portIdentity.clockIdentity[2], ptpClock->portIdentity.clockIdentity[3],
	    ptpClock->portIdentity.clockIdentity[4], ptpClock->portIdentity.clockIdentity[5]);
	PTPD_TRACE(TRACE_MSG," portNumber.................... %d\n", ptpClock->portIdentity.portNumber);
	PTPD_TRACE(TRACE_MSG," currentUtcOffset.............. %d\n", ptpClock->currentUtcOffset);
	PTPD_TRACE(TRACE_MSG," grandmasterPriority1.......... %d\n", ptpClock->grandmasterPriority1);
	PTPD_TRACE(TRACE_MSG," clockClass.................... %d\n", ptpClock->clockQuality.clockClass);
	PTPD_TRACE(TRACE_MSG," clockAccuracy................. %d\n", ptpClock->clockQuality.clockAccuracy);
	PTPD_TRACE(TRACE_MSG," offsetScaledLogVariance....... %d\n", ptpClock->clockQuality.offsetScaledLogVariance);
	PTPD_TRACE(TRACE_MSG," grandmasterPriority2.......... %d\n", ptpClock->grandmasterPriority2);
	PTPD_TRACE(TRACE_MSG," grandmasterIdentity........... %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n",
	    ptpClock->grandmasterIdentity[0], ptpClock->grandmasterIdentity[1],
	    ptpClock->grandmasterIdentity[2], ptpClock->grandmasterIdentity[3],
	    ptpClock->grandmasterIdentity[4], ptpClock->grandmasterIdentity[5]);
	PTPD_TRACE(TRACE_MSG," stepsRemoved.................. %d\n", ptpClock->stepsRemoved);
	PTPD_TRACE(TRACE_MSG," timeSource.................... %d\n", ptpClock->timeSource);
	if (ptpClock->wrNodeMode != NON_WR)
	{
	  PTPD_TRACE(TRACE_MSG," [WR suffix] tlv_type.......... 0x%x\n", WR_TLV_TYPE);
	  PTPD_TRACE(TRACE_MSG," [WR suffix] tlv_length........ %d\n", WR_ANNOUNCE_TLV_LENGTH);
	  PTPD_TRACE(TRACE_MSG," [WR suffix] wr_flags.......... 0x%x\n", wr_flags);
	}
	PTPD_TRACE(TRACE_MSG,"^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");


}

/*Unpack Announce message from IN buffer of ptpClock to msgtmp.Announce*/
void msgUnpackAnnounce(void *buf,MsgAnnounce *announce,  MsgHeader *header)
{
	UInteger16 tlv_type;
	announce->originTimestamp.secondsField.msb = get_be16(buf+34);
	announce->originTimestamp.secondsField.lsb = get_be32(buf+36);
	announce->originTimestamp.nanosecondsField = get_be32(buf+40);
	announce->currentUtcOffset = get_be16(buf+44);


	announce->grandmasterPriority1 = *(UInteger8*)(buf+47);
	announce->grandmasterClockQuality.clockClass = *(UInteger8*)(buf+48);
	announce->grandmasterClockQuality.clockAccuracy = *(Enumeration8*)(buf+49);
	announce->grandmasterClockQuality.offsetScaledLogVariance = get_be16(buf+50);
	announce->grandmasterPriority2 = *(UInteger8*)(buf+52);
	memcpy(announce->grandmasterIdentity, buf+53 , CLOCK_IDENTITY_LENGTH);
	announce->stepsRemoved = get_be16(buf+61);
	announce->timeSource = *(Enumeration8*)(buf+63);

	/*White Rabbit- only flags in a reserved space of announce message*/
	UInteger16  messageLen = (UInteger16)get_be16(buf + 2);
	//check if there is msg suffix
	if(messageLen > ANNOUNCE_LENGTH)
	{
	  tlv_type   = (UInteger16)get_be16(buf+64);

	  if(tlv_type == WR_TLV_TYPE)
	  {
	    announce->wr_flags   = (UInteger16)get_be16(buf+68);
			PTPD_TRACE(TRACE_MSG,"GotWRFlags!\n");
	  }

	}

	PTPD_TRACE(TRACE_MSG,"------------ msgUnpackAnnounce ----------\n");
	PTPD_TRACE(TRACE_MSG," messageLength................. %u\n", messageLen);
	PTPD_TRACE(TRACE_MSG," currentUtcOffset.............. %d\n", announce->currentUtcOffset);
	PTPD_TRACE(TRACE_MSG," grandmasterPriority1.......... %d\n", announce->grandmasterPriority1);
	PTPD_TRACE(TRACE_MSG," clockClass.................... %d\n", announce->grandmasterClockQuality.clockClass);
	PTPD_TRACE(TRACE_MSG," clockAccuracy................. %d\n", announce->grandmasterClockQuality.clockAccuracy);
	PTPD_TRACE(TRACE_MSG," offsetScaledLogVariance....... %d\n", announce->grandmasterClockQuality.offsetScaledLogVariance);
	PTPD_TRACE(TRACE_MSG," grandmasterPriority2.......... %d\n", announce->grandmasterPriority2);
	PTPD_TRACE(TRACE_MSG," grandmasterIdentity........... %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n",
	    announce->grandmasterIdentity[0], announce->grandmasterIdentity[1],
	    announce->grandmasterIdentity[2], announce->grandmasterIdentity[3],
	    announce->grandmasterIdentity[4], announce->grandmasterIdentity[5]);
	PTPD_TRACE(TRACE_MSG," stepsRemoved.................. %d\n", announce->stepsRemoved);
	PTPD_TRACE(TRACE_MSG," timeSource.................... %d\n", announce->timeSource);
	if (messageLen > ANNOUNCE_LENGTH)
	{
	  PTPD_TRACE(TRACE_MSG," [WR suffix] tlv_type.......... 0x%x\n", tlv_type);
	  PTPD_TRACE(TRACE_MSG," [WR suffix] tlv_length........ %d\n", (UInteger16)get_be16(buf+66));
	  PTPD_TRACE(TRACE_MSG," [WR suffix] wr_flags.......... 0x%x\n", announce->wr_flags );
	}
	PTPD_TRACE(TRACE_MSG,"^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");

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

	PTPD_TRACE(TRACE_MSG,"------------ msgPackFollowUp-------\n");
	PTPD_TRACE(TRACE_MSG," syncSequenceId ............... %u\n", ptpClock->sentSyncSequenceId-1);
	PTPD_TRACE(TRACE_MSG," logMinDelayReqInterval ....... %u\n", ptpClock->logSyncInterval);
	PTPD_TRACE(TRACE_MSG," syncTransTimestamp.secs.hi.... %d\n", 0xFFFF & (ptpClock->synch_tx_ts.utc >> 32));
	PTPD_TRACE(TRACE_MSG," syncTransTimestamp.secs.lo.... %d\n", 0xFFFFFFFF & ptpClock->synch_tx_ts.utc);
	PTPD_TRACE(TRACE_MSG," syncTransTimestamp.nsecs...... %d\n", ptpClock->synch_tx_ts.nsec);
	PTPD_TRACE(TRACE_MSG,"^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
}

/*Unpack Follow_up message from IN buffer of ptpClock to msgtmp.follow*/
void msgUnpackFollowUp(void *buf,MsgFollowUp *follow)
{
	follow->preciseOriginTimestamp.secondsField.msb = get_be16(buf+34);
	follow->preciseOriginTimestamp.secondsField.lsb = get_be32(buf+36);
	follow->preciseOriginTimestamp.nanosecondsField = get_be32(buf+40);

	PTPD_TRACE(TRACE_MSG,"------------ msgUnpackFollowUp-------\n");
	PTPD_TRACE(TRACE_MSG," preciseOriginTimestamp.secs.hi.%d\n", follow->preciseOriginTimestamp.secondsField.msb);
	PTPD_TRACE(TRACE_MSG," preciseOriginTimestamp.secs.lo %d\n", follow->preciseOriginTimestamp.secondsField.lsb);
	PTPD_TRACE(TRACE_MSG," preciseOriginTimestamp.nsecs.. %d\n", follow->preciseOriginTimestamp.nanosecondsField);
	PTPD_TRACE(TRACE_MSG,"^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");

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
	put_be16(buf+30, ptpClock->sentDelayReqSequenceId);
	*(UInteger8*)(buf+32) = 0x01; //Table 23
	*(Integer8*)(buf+33) = 0x7F; //Table 24
	memset((buf+8),0,8);

	/*delay_req message*/
	put_be16(buf+34, originTimestamp->secondsField.msb);
	put_be32(buf+36, originTimestamp->secondsField.lsb);
	put_be32(buf+40, originTimestamp->nanosecondsField);

	PTPD_TRACE(TRACE_MSG,"------------ msgPackDelayReq-------\n");
	PTPD_TRACE(TRACE_MSG," delayReqSequenceId ........... %u\n", ptpClock->sentDelayReqSequenceId);
	PTPD_TRACE(TRACE_MSG," originTimestamp.secs.msb...... %d\n", originTimestamp->secondsField.msb);
	PTPD_TRACE(TRACE_MSG," originTimestamp.secs.lsb...... %d\n", originTimestamp->secondsField.lsb);
	PTPD_TRACE(TRACE_MSG," originTimestamp.nsecs......... %d\n", originTimestamp->nanosecondsField);
	PTPD_TRACE(TRACE_MSG,"^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");


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


	PTPD_TRACE(TRACE_MSG,"------------ msgPackDelayResp-------\n");
	PTPD_TRACE(TRACE_MSG," correctionfield.msb .......... %d\n", header->correctionfield.msb);
	PTPD_TRACE(TRACE_MSG," correctionfield.lsb........... %d\n", header->correctionfield.lsb);
	PTPD_TRACE(TRACE_MSG," sequenceId ................... %u\n", header->sequenceId);
	PTPD_TRACE(TRACE_MSG," logMinDelayReqInterval ....... %u\n", ptpClock->logMinDelayReqInterval);
	PTPD_TRACE(TRACE_MSG," delayReceiptTimestamp.secs.hi. %d\n", 0xFFFF & (ptpClock->current_rx_ts.utc >> 32));
	PTPD_TRACE(TRACE_MSG," delayReceiptTimestamp.secs.lo. %d\n", 0xFFFFFFFF & ptpClock->current_rx_ts.utc);
	PTPD_TRACE(TRACE_MSG," delayReceiptTimestamp.nsecs... %d\n", ptpClock->current_rx_ts.nsec);
	PTPD_TRACE(TRACE_MSG," requestingSourceUuid.......... %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n",
	    header->sourcePortIdentity.clockIdentity[0],
	    header->sourcePortIdentity.clockIdentity[1],
	    header->sourcePortIdentity.clockIdentity[2],
	    header->sourcePortIdentity.clockIdentity[3],
	    header->sourcePortIdentity.clockIdentity[4],
	    header->sourcePortIdentity.clockIdentity[5]
	    );
	PTPD_TRACE(TRACE_MSG," requestingSourcePortId........ %u\n", header->sourcePortIdentity.portNumber);
	PTPD_TRACE(TRACE_MSG,"^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");

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
	delayreq->originTimestamp.secondsField.msb = get_be16(buf+34);
	delayreq->originTimestamp.secondsField.lsb = get_be32(buf+36);
	delayreq->originTimestamp.nanosecondsField = get_be32(buf+40);

	PTPD_TRACE(TRACE_MSG,"------------ msgUnpackDelayReq-------\n");
	PTPD_TRACE(TRACE_MSG," preciseOriginTimestamp.secs.hi.%d\n", delayreq->originTimestamp.secondsField.msb);
	PTPD_TRACE(TRACE_MSG," preciseOriginTimestamp.secs.lo %d\n", delayreq->originTimestamp.secondsField.lsb);
	PTPD_TRACE(TRACE_MSG," preciseOriginTimestamp.nsecs.. %d\n", delayreq->originTimestamp.nanosecondsField);
	PTPD_TRACE(TRACE_MSG,"^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");

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
	resp->receiveTimestamp.secondsField.msb = get_be16(buf+34);
	resp->receiveTimestamp.secondsField.lsb = get_be32(buf+36);
	resp->receiveTimestamp.nanosecondsField = get_be32(buf+40);
	memcpy(resp->requestingPortIdentity.clockIdentity,(buf+44),CLOCK_IDENTITY_LENGTH);
	resp->requestingPortIdentity.portNumber = (UInteger16)get_be16(buf+52);

	PTPD_TRACE(TRACE_MSG,"------------ msgUnpackDelayResp-------\n");
	PTPD_TRACE(TRACE_MSG," receiveTimestamp.secs.msb......%d\n", resp->receiveTimestamp.secondsField.msb);
	PTPD_TRACE(TRACE_MSG," receiveTimestamp.secs.lsb..... %d\n", resp->receiveTimestamp.secondsField.lsb);
	PTPD_TRACE(TRACE_MSG," receiveTimestamp.nsecs........ %d\n", resp->receiveTimestamp.nanosecondsField);
	PTPD_TRACE(TRACE_MSG," requestingPortUuid.......... %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n",
	    resp->requestingPortIdentity.clockIdentity[0],
	    resp->requestingPortIdentity.clockIdentity[1],
	    resp->requestingPortIdentity.clockIdentity[2],
	    resp->requestingPortIdentity.clockIdentity[3],
	    resp->requestingPortIdentity.clockIdentity[4],
	    resp->requestingPortIdentity.clockIdentity[5]
	    );
	PTPD_TRACE(TRACE_MSG," requestingSourcePortId........ %u\n", resp->requestingPortIdentity.portNumber);
	PTPD_TRACE(TRACE_MSG,"^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");

}


/*Unpack PdelayResp message from IN buffer of ptpClock to msgtmp.presp*/
void msgUnpackPDelayResp(void *buf,MsgPDelayResp *presp)
{
// 	presp->requestReceiptTimestamp.secondsField.msb = flip16(*(UInteger16*)(buf+34));
// 	presp->requestReceiptTimestamp.secondsField.lsb = flip32(*(UInteger32*)(buf+36));
// 	presp->requestReceiptTimestamp.nanosecondsField = flip32(*(UInteger32*)(buf+40));

	presp->requestReceiptTimestamp.secondsField.msb = get_be16(buf+34);
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

	if (ptpClock->wrNodeMode == NON_WR)
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

	put_be16(buf+48, WR_TLV_TYPE);

	PTPD_TRACE(TRACE_MSG,"------------ msgPackWRManagement-------\n");
	PTPD_TRACE(TRACE_MSG," recipient's PortUuid.......... %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n",
	    ptpClock->parentPortIdentity.clockIdentity[0],
	    ptpClock->parentPortIdentity.clockIdentity[1],
	    ptpClock->parentPortIdentity.clockIdentity[2],
	    ptpClock->parentPortIdentity.clockIdentity[3],
	    ptpClock->parentPortIdentity.clockIdentity[4],
	    ptpClock->parentPortIdentity.clockIdentity[5]
	    );
	PTPD_TRACE(TRACE_MSG," recipient's PortId............ %u\n", ptpClock->parentPortIdentity.portNumber);
	PTPD_TRACE(TRACE_MSG," management CMD................ %u\n", WR_CMD);
	PTPD_TRACE(TRACE_MSG," management ID................. 0x%x\n", wr_managementId);

 	UInteger16 len = 0;
 	switch(wr_managementId)
 	{
	  case CALIBRATE: //new fsm



	    if(ptpClock->isCalibrated)
	    {
	      put_be16(buf+54, 0x0000);
	      PTPD_TRACE(TRACE_MSG," calibrationSendPattern........ FALSE \n");
	    }
	    else
	    {
	      put_be16(buf+54, 0x0001);
	      PTPD_TRACE(TRACE_MSG," calibrationSendPattern........ TRUE \n");
	    }
	    put_be32(buf+56, ptpClock->calibrationPeriod);
	    put_be32(buf+60, ptpClock->calibrationPattern);
	    put_be16(buf+64, ptpClock->calibrationPatternLen);
	    len = 12;


	    PTPD_TRACE(TRACE_MSG," calibrationPeriod............. %u [us]\n", ptpClock->calibrationPeriod);
	    PTPD_TRACE(TRACE_MSG," calibrationPattern............ %s \n", printf_bits(ptpClock->calibrationPattern));
	    PTPD_TRACE(TRACE_MSG," calibrationPatternLen......... %u [bits]\n", ptpClock->calibrationPatternLen);



	    break;

	  case CALIBRATED: //new fsm


	    /*delta TX*/
	    put_be32(buf+54, ptpClock->deltaTx.scaledPicoseconds.msb);
	    put_be32(buf+58, ptpClock->deltaTx.scaledPicoseconds.lsb);

	    /*delta RX*/
	    put_be32(buf+62, ptpClock->deltaRx.scaledPicoseconds.msb);
	    put_be32(buf+66, ptpClock->deltaRx.scaledPicoseconds.lsb);

	    PTPD_TRACE(TRACE_MSG," deltaTx.scaledPicoseconds.msb. %d\n", (unsigned int)ptpClock->deltaTx.scaledPicoseconds.msb);
	    PTPD_TRACE(TRACE_MSG," deltaTx.scaledPicoseconds.lsb. %d\n", (unsigned int)ptpClock->deltaTx.scaledPicoseconds.lsb);

	    PTPD_TRACE(TRACE_MSG," deltaRx.scaledPicoseconds.msb. %d\n", (unsigned int)ptpClock->deltaRx.scaledPicoseconds.msb);
	    PTPD_TRACE(TRACE_MSG," deltaRx.scaledPicoseconds.lsb. %d\n", (unsigned int)ptpClock->deltaRx.scaledPicoseconds.lsb);


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
	put_be16(buf+50, WR_MANAGEMENT_TLV_LENGTH + len);
	put_be16(buf+52, wr_managementId);

	PTPD_TRACE(TRACE_MSG," messageLength................. %u\n", WR_MANAGEMENT_LENGTH + len);
	PTPD_TRACE(TRACE_MSG," wr management len............. %u\n", WR_MANAGEMENT_TLV_LENGTH + len);


	PTPD_TRACE(TRACE_MSG,"^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");


	return (WR_MANAGEMENT_LENGTH + len);
}

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
	  PTPD_TRACE(TRACE_MSG,"handle Management msg, failed, This is not a White Rabbit Command, actionField = 0x%x\n",management->actionField);
	  return;
	}

	Integer16 tlv_type = get_be16(buf+48);

	if(tlv_type != WR_TLV_TYPE)
	{
	  PTPD_TRACE(TRACE_MSG,"handle Management msg, failed, unrecognized TLV type in WR_CMD management, tlv_type = 0x%x \n",tlv_type);
	  return;
	}
	*wr_managementId = get_be16(buf+52);


	PTPD_TRACE(TRACE_MSG,"------------ msgUnpackWRManagement-------\n");
	PTPD_TRACE(TRACE_MSG," target PortUuid............... %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n",
	    management->targetPortIdentity.clockIdentity[0],
	    management->targetPortIdentity.clockIdentity[1],
	    management->targetPortIdentity.clockIdentity[2],
	    management->targetPortIdentity.clockIdentity[3],
	    management->targetPortIdentity.clockIdentity[4],
	    management->targetPortIdentity.clockIdentity[5]
	    );
	PTPD_TRACE(TRACE_MSG," target PortId................. %u\n", management->targetPortIdentity.portNumber);
	PTPD_TRACE(TRACE_MSG," management CMD................ %u\n", management->actionField);
	PTPD_TRACE(TRACE_MSG," tlv type...................... 0x%x\n", tlv_type);
	PTPD_TRACE(TRACE_MSG," management ID................. 0x%x\n", *wr_managementId);

	/*This is not nice way of doing it, need to be changed later !!!!!*/
	if(len > WR_MANAGEMENT_LENGTH)
	{
 	  switch(*wr_managementId)
 	  {
	    case CALIBRATE:

	      ptpClock->otherNodeCalibrationSendPattern= get_be16(buf+54);
	      ptpClock->otherNodeCalibrationPeriod     = get_be32(buf+56);
	      ptpClock->otherNodeCalibrationPattern    = get_be32(buf+60);
	      ptpClock->otherNodeCalibrationPatternLen = get_be16(buf+64);

	      if(ptpClock->otherNodeCalibrationSendPattern & SEND_CALIBRATION_PATTERN)
					PTPD_TRACE(TRACE_MSG," calibrationSendPattern........ TRUE \n")
	      else
					PTPD_TRACE(TRACE_MSG," calibrationSendPattern........ FALSE \n")

	      PTPD_TRACE(TRACE_MSG," calibrationPeriod............. %u [us]\n", ptpClock->calibrationPeriod);
	      PTPD_TRACE(TRACE_MSG," calibrationPattern............ %s \n", printf_bits(ptpClock->calibrationPattern));
	      PTPD_TRACE(TRACE_MSG," calibrationPatternLen......... %u [bits]\n", ptpClock->calibrationPatternLen);

	      break;

	    case CALIBRATED:
	      /*delta TX*/
	      ptpClock->grandmasterDeltaTx.scaledPicoseconds.msb = get_be32(buf+54);
	      ptpClock->grandmasterDeltaTx.scaledPicoseconds.lsb = get_be32(buf+58);

	      /*delta RX*/
	      ptpClock->grandmasterDeltaRx.scaledPicoseconds.msb = get_be32(buf+62);
	      ptpClock->grandmasterDeltaRx.scaledPicoseconds.lsb = get_be32(buf+66);

	      PTPD_TRACE(TRACE_MSG," deltaTx.scaledPicoseconds.msb. %d\n", (unsigned int)ptpClock->grandmasterDeltaTx.scaledPicoseconds.msb);
	      PTPD_TRACE(TRACE_MSG," deltaTx.scaledPicoseconds.lsb. %d\n", (unsigned int)ptpClock->grandmasterDeltaTx.scaledPicoseconds.lsb);

	      PTPD_TRACE(TRACE_MSG," deltaRx.scaledPicoseconds.msb. %d\n", (unsigned int)ptpClock->grandmasterDeltaRx.scaledPicoseconds.msb);
	      PTPD_TRACE(TRACE_MSG," deltaRx.scaledPicoseconds.lsb. %d\n", (unsigned int)ptpClock->grandmasterDeltaRx.scaledPicoseconds.lsb);

	      break;

	    default:
	      //no data
	      break;
	  }
	}

	PTPD_TRACE(TRACE_MSG,"^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
}

