#ifndef DATATYPES_H_
#define DATATYPES_H_

/*Struct defined in spec*/


/**
*\file
* \brief Main structures used in ptpdv2
* 
* This header file defines structures defined by the spec, 
main program data structure, and all messages structures
 */


/** 
* \brief The TimeInterval type represents time intervals
 */
typedef struct {
	Integer64 scaledNanoseconds;
} TimeInterval;

/** 
* \brief The Fixed Delay type represents time intervals [White Rabbit]
 */
typedef struct {
	UInteger64 scaledPicoseconds;
} FixedDelta;

/**
* \brief The Timestamp type represents a positive time with respect to the epoch
 */
typedef struct  {
	UInteger48 secondsField;
	UInteger32 nanosecondsField;
} Timestamp;

/**
* \brief The ClockIdentity type identifies a clock
 */
typedef Octet ClockIdentity[CLOCK_IDENTITY_LENGTH];

/**
* \brief The PortIdentity identifies a PTP port.
 */
typedef struct {
	ClockIdentity clockIdentity;
	UInteger16 portNumber;
} PortIdentity;

/**
* \brief The PortAdress type represents the protocol address of a PTP port
 */
typedef struct {
	Enumeration16 networkProtocol;
	UInteger16 adressLength;
	Octet* adressField;
} PortAdress;

/**
* \brief The ClockQuality represents the quality of a clock
 */
typedef struct {
	UInteger8 clockClass;
	Enumeration8 clockAccuracy;
	UInteger16 offsetScaledLogVariance;
} ClockQuality;

/**
* \brief The TLV type represents TLV extension fields
 */
typedef struct {
	Enumeration16 tlvType;
	UInteger16 lengthField;
	Octet* valueField;
} TLV;

/**
* \brief The PTPText data type is used to represent textual material in PTP messages
 */
typedef struct {
	UInteger8 lengthField;
	Octet* textField;
} PTPText;

/**
* \brief The FaultRecord type is used to construct fault logs
 */
typedef struct {
	UInteger16 faultRecordLength;
	Timestamp faultTime;
	Enumeration8 severityCode;
	PTPText faultName;
	PTPText faultValue;
	PTPText faultDescription;
} FaultRecord;


/** 
* \brief The common header for all PTP messages (Table 18 of the spec)
 */
/* Message header */
typedef struct {
	Nibble transportSpecific;
	Enumeration4 messageType;
	UInteger4 versionPTP;
	UInteger16 messageLength;
	UInteger8 domainNumber;
	Octet flagField[2];
	Integer64 correctionfield;
	PortIdentity sourcePortIdentity;
	UInteger16 sequenceId;
	UInteger8 controlField;
	Integer8 logMessageInterval;
} MsgHeader;

/** 
* \brief TLV sufixed to Announce message for White Rabbit (WR extension)
 */
/*Announce Message */

typedef Octet AnnounceWRtlv[WR_ANNOUNCE_TLV_LENGTH];


/** 
* \brief Announce message fields (Table 25 of the spec)[White Rabbit]
 */
/*Announce Message */
typedef struct {
	Timestamp originTimestamp;
	Integer16 currentUtcOffset;
	UInteger8 grandmasterPriority1;
	ClockQuality grandmasterClockQuality;
	UInteger8 grandmasterPriority2;
	ClockIdentity grandmasterIdentity;
	UInteger16 stepsRemoved;
	Enumeration8 timeSource;

	//White Rabbit flags
	UInteger8 wr_flags;

}MsgAnnounce;


/** 
* \brief Sync message fields (Table 26 of the spec)
 */
/*Sync Message */
typedef struct {
	Timestamp originTimestamp;
}MsgSync;

/**
* \brief DelayReq message fields (Table 26 of the spec)
 */
/*DelayReq Message */
typedef struct {
	Timestamp originTimestamp;
}MsgDelayReq;

/**
* \brief DelayResp message fields (Table 30 of the spec)
 */
/*delayResp Message*/
typedef struct {
	Timestamp receiveTimestamp;
	PortIdentity requestingPortIdentity;
}MsgDelayResp;

/** 
* \brief FollowUp message fields (Table 27 of the spec)
 */
/*Follow-up Message*/
typedef struct {
	Timestamp preciseOriginTimestamp;
}MsgFollowUp;

/**
* \brief PDelayReq message fields (Table 29 of the spec)
 */
/*PdelayReq Message*/
typedef struct {
	Timestamp originTimestamp;

}MsgPDelayReq;

/**
* \brief PDelayResp message fields (Table 30 of the spec)
 */
/*PdelayResp Message*/
typedef struct {
	Timestamp requestReceiptTimestamp;
	PortIdentity requestingPortIdentity;
}MsgPDelayResp;

/**
* \brief PDelayRespFollowUp message fields (Table 31 of the spec)
 */
/*PdelayRespFollowUp Message*/
typedef struct {
	Timestamp responseOriginTimestamp;
	PortIdentity requestingPortIdentity;
}MsgPDelayRespFollowUp;

/**
* \brief Signaling message fields (Table 33 of the spec)
 */
/*Signaling Message*/
typedef struct {
	PortIdentity targetPortIdentity;
	char* tlv;
}MsgSignaling;

/**
* \brief Management message fields (Table 37 of the spec)
 */
/*management Message*/
typedef struct {
	PortIdentity targetPortIdentity;
	UInteger8 startingBoundaryHops;
	UInteger8 boundaryHops;
	Enumeration4 actionField;
	char* tlv;
}MsgManagement;

/**
* \brief Time structure to handle Linux time information. Fixed for WR compliance
 */
typedef struct {
	Integer32 seconds;
	Integer32 nanoseconds;
	Integer32 phase;
} TimeInternal;

/**
 * \brief Structure used as a timer
 */
typedef struct {
	Integer32  interval;
	Integer32  left;
	Boolean expire;
} IntervalTimer;


/**
 * \brief ForeignMasterRecord is used to manage foreign masters
 */
typedef struct
{
	PortIdentity foreignMasterPortIdentity;
	UInteger16 foreignMasterAnnounceMessages;

	//This one is not in the spec
	MsgAnnounce  announce;
	MsgHeader    header;

} ForeignMasterRecord;


/**
 * \struct PtpClock
 * \brief Main program data structure
 */
/* main program data structure */
typedef struct {
/***** Default data set ******/
	NetPath netPath;

	/*Static members*/
	Boolean twoStepFlag;
	ClockIdentity clockIdentity;
	UInteger16 numberPorts;

	/*Dynamic members*/
	ClockQuality clockQuality;

	/*Configurable members*/
	UInteger8 priority1;
	UInteger8 priority2;
	UInteger8 domainNumber;
	Boolean slaveOnly;

/***** Current data set ******/
	/*Dynamic members*/
	UInteger16 stepsRemoved;
	TimeInternal offsetFromMaster;
	TimeInternal meanPathDelay;

/******* Parent data set *******/

	/*Dynamic members*/
	PortIdentity parentPortIdentity;
	Boolean parentStats;
	UInteger16 observedParentOffsetScaledLogVariance;
	Integer32 observedParentClockPhaseChangeRate;
	ClockIdentity grandmasterIdentity;
	ClockQuality grandmasterClockQuality;
	UInteger8 grandmasterPriority1;
	UInteger8 grandmasterPriority2;

	/*
   ******* White Rabbit *******
   *       (parentDS)
   */
	Boolean grandmasterIsWRnode;
	Boolean grandmasterIsWRmode;
	Boolean grandmasterIsCalibrated;
	Enumeration8 grandmasterWrNodeMode;

	FixedDelta grandmasterDeltaTx;
	FixedDelta grandmasterDeltaRx;

/******* Global time properties data set *********/

	/*Dynamic members*/
	Integer16 currentUtcOffset;
	Boolean currentUtcOffsetValid;
	Boolean leap59;
	Boolean leap61;
	Boolean timeTraceable;
	Boolean frequencyTraceable;
	Boolean ptpTimescale;
	Enumeration8 timeSource;

/****** Port configuration data set ***********/

	/*Static members*/
	PortIdentity portIdentity;

	/*Dynamic members*/
	Enumeration8 portState;
	Integer8 logMinDelayReqInterval;
	TimeInternal peerMeanPathDelay;

	/*Configurable members*/
	Integer8 logAnnounceInterval;
	UInteger8 announceReceiptTimeout;
	Integer8 logSyncInterval;
	Enumeration8 delayMechanism;
	Integer8 logMinPdelayReqInterval;
	UInteger4 versionNumber;



	/* Foreign master data set */
	ForeignMasterRecord *foreign;

	/* Other things we need for the protocol */
	UInteger16 number_foreign_records;
	Integer16  max_foreign_records;
	Integer16  foreign_record_i;
	Integer16  foreign_record_best;
	Boolean  record_update;


	MsgHeader msgTmpHeader;

	union {
		MsgSync  sync;
		MsgFollowUp  follow;
		MsgDelayReq  req;
		MsgDelayResp resp;
		MsgPDelayReq  preq;
		MsgPDelayResp  presp;
		MsgPDelayRespFollowUp  prespfollow;
		MsgManagement  manage;
		MsgAnnounce  announce;
		MsgSignaling signaling;
	} msgTmp;


	Octet msgObuf[PACKET_SIZE];
	Octet msgIbuf[PACKET_SIZE];

	TimeInternal  master_to_slave_delay;
	TimeInternal  slave_to_master_delay;
	Integer32 	observed_drift;

	TimeInternal  pdelay_req_receive_time;
	TimeInternal  pdelay_req_send_time;
	TimeInternal  pdelay_resp_receive_time;
	TimeInternal  pdelay_resp_send_time;
	TimeInternal  sync_receive_time;
	TimeInternal  delay_req_send_time;
	TimeInternal  delay_req_receive_time;
	MsgHeader		PdelayReqHeader;
	MsgHeader		delayReqHeader;
	TimeInternal	pdelayMS;
	TimeInternal	pdelaySM;
	TimeInternal  delayMS;
	TimeInternal	delaySM;
	TimeInternal  lastSyncCorrectionField;
	TimeInternal  lastPdelayRespCorrectionField;

	double  R;

	Boolean  sentPDelayReq;
	UInteger16  sentPDelayReqSequenceId;
	UInteger16  sentDelayReqSequenceId;
	UInteger16  sentSyncSequenceId;
	UInteger16  sentAnnounceSequenceId;
	UInteger16  recvPDelayReqSequenceId;
	UInteger16  recvSyncSequenceId;
	Boolean  waitingForFollow;

	offset_from_master_filter  ofm_filt;
	one_way_delay_filter  owd_filt;

	Boolean message_activity;

	IntervalTimer  itimer[TIMER_ARRAY_SIZE];


	/*Usefull to init network stuff*/
	UInteger8 port_communication_technology;
	Octet port_uuid_field[PTP_UUID_LENGTH];

	wr_servo_state_t wr_servo;

	/***********White Rabbit ***************/

	/*
	 * white rabbit FSM state
	 */
	Enumeration8  wrPortState;

	/*
	 * stores current managementId
	 * it's set to null when used
	 */
	Enumeration16 msgTmpManagementId;

	/*
	 * This says whether PTPd is run for:
	 * - non-WR node,
	 * - WR Slave
	 * - WR Master
	 *
	 * Its important that the node knows what it is,
	 * by default PTPd runs in NON_WR
	 */
	Enumeration8 wrNodeMode; //copied to new

	/*
	 * tell us whether we work in WR
	 * mode at the moment
	 * starts with FALSE
	 */
	Boolean isWRmode;

	/*
	 * If port is aware of it's
	 * fixed delays (they are measured and
	 * stored in deltaTx and deltaRx)
	 * it's TRUE
	 */
	Boolean isCalibrated;

	/*
	 * Fixed elays
	 */
	FixedDelta deltaTx;
	FixedDelta deltaRx;

	/*
	 * Calibration parameters of the
	 * current port
	 */

	UInteger32 calibrationPeriod;//[us]
	UInteger32 calibrationPattern;
	UInteger16 calibrationPatternLen;

	UInteger16 otherNodeCalibrationSendPattern;
	UInteger32 otherNodeCalibrationPeriod;
	UInteger32 otherNodeCalibrationPattern;
	UInteger16 otherNodeCalibrationPatternLen;

	/*
	 * used to implemetn two-step clock
	 * this is implemented in WR differently than
	 * in original deamon (in original they used errored
	 * self message to read timestam and know that
	 * follow up should be read)
	 */
	Enumeration8   pending_follow_up;

	/*
	 * Alpha parameter, represents physical
	 * medium correlation
	 * used to obtan asymmetry
	 */
	UInteger32 scalled_alpha;

	/******White rabbit HW timestamps *******/

	/*
	 * if any Tx timestamps should
	 * be read (any pending) it's true
	 */
	Boolean pending_tx_ts;

	/*
	 * pending flags for each kind
	 * of tx message (not needed for Rx
	 */
	Boolean pending_Synch_tx_ts;
	Boolean pending_DelayReq_tx_ts;
	Boolean pending_PDelayReq_tx_ts;
	Boolean pending_PDelayResp_tx_ts;

	/*
	 * for storing frame_tags which keep
	 * track of which timestamp is read from HW
	 * for each msg
	 */
//  wr_frame_tag_t synch_tx_tag;
//  wr_frame_tag_t delayReq_tx_tag;
//  wr_frame_tag_t pDelayReq_tx_tag;
//  wr_frame_tag_t pDelayResp_tx_tag;


	/*
	 * store timestamp for each msg
	 */
	wr_timestamp_t synch_tx_ts;
	wr_timestamp_t delayReq_tx_ts;
	wr_timestamp_t pDelayReq_tx_ts;
	wr_timestamp_t pDelayResp_tx_ts;

	/*
	 * stores current Rx timestamp
	 */
	wr_timestamp_t current_rx_ts;
	wr_timestamp_t current_tx_ts;

	/******White rabbit timers *******/

	/*
	 * holds info how many times
	 * current WR state has been repeated
	 */
	UInteger8 currentWRstateCnt;

	/*
	 * stores eclapsed time for each timer
	 */
	IntervalTimer wrtimer[WR_TIMER_ARRAY_SIZE];

	/*
	 * stores timeout for each timer
	 */
	UInteger16 wrTimeouts[WR_TIMER_ARRAY_SIZE];

	/*
	 * used to calculate eclapsed time,
	 * we don't use interrapts, but periodically
	 * call do_irq_less_timing()
	 * and check time since last it's call
	 */
	struct timeval last_update;

	//wr_servo_state_t servo

} PtpClock;

/**
 * \struct RunTimeOpts
 * \brief Program options set at run-time
 */
/* program options set at run-time */
typedef struct {

	Integer8	announceInterval;
	Integer8	syncInterval;
	ClockQuality	clockQuality;
	UInteger8	priority1;
	UInteger8	priority2;
	UInteger8	domainNumber;
	Boolean		slaveOnly;
	Integer16	currentUtcOffset;
	Octet		ifaceName[MAX_PORT_NUMBER][IFACE_NAME_LENGTH];
	Boolean		noResetClock;
	Boolean		noAdjust;
	Boolean		displayStats;
	Boolean		csvStats;
	Octet		unicastAddress[NET_ADDRESS_LENGTH];
	Integer16	ap, ai;
	Integer16	s;
	TimeInternal	inboundLatency, outboundLatency;
	Integer1	max_foreign_records;
	Boolean		ethernet_mode;
	Boolean		E2E_mode;
	Boolean		offset_first_updated;

	/********* White Rabbit ********/
	UInteger16	portNumber;
	Enumeration8	wrNodeMode;
	UInteger32	calibrationPeriod;
	UInteger32	calibrationPattern;
	UInteger16	calibrationPatternLen;
	//tmp
	UInteger8	overrideClockIdentity;

} RunTimeOpts;

#endif /*DATATYPES_H_*/
