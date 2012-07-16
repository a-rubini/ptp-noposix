#ifndef CONSTANTS_H_
#define CONSTANTS_H_

/**
*\file
* \brief Default values and constants used in ptpdv2
*
* This header file includes all default values used during initialization
* and enumeration defined in the spec
 */
#undef PACKED
#include "ptpd_netif.h"

/**

*/
#define DAEMONE_MODE				1
#define NONDAEMONE_MODE				0


#define DEFAULT_STARTUP_MODE                    DAEMONE_MODE

#define MANUFACTURER_ID "CERNWhiteRabbit;2.0.0"

/* implementation specific constants */
#define DEFAULT_INBOUND_LATENCY			0       /* in nsec */
#define DEFAULT_OUTBOUND_LATENCY		0       /* in nsec */
#define DEFAULT_NO_RESET_CLOCK			FALSE
#define DEFAULT_DOMAIN_NUMBER			0
#define DEFAULT_DELAY_MECHANISM			E2E //P2P end to end not impl.
#define DEFAULT_AP				10
#define DEFAULT_AI				1000
#define DEFAULT_DELAY_S				6
#define DEFAULT_ANNOUNCE_INTERVAL		1 //0 in 802.1AS
#define DEFAULT_UTC_OFFSET			0
#define DEFAULT_UTC_VALID			FALSE
#define DEFAULT_PDELAYREQ_INTERVAL		1 //-4 in 802.1AS
#define DEFAULT_DELAYREQ_INTERVAL		0
#define DEFAULT_SYNC_INTERVAL			0 //-7 in 802.1AS
#define DEFAULT_SYNC_RECEIPT_TIMEOUT		3
#define DEFAULT_ANNOUNCE_RECEIPT_TIMEOUT	6 //6 // 3 by default
#define DEFAULT_QUALIFICATION_TIMEOUT		2
#define DEFAULT_FOREIGN_MASTER_TIME_WINDOW	4
#define DEFAULT_FOREIGN_MASTER_THRESHOLD	2
#define DEFAULT_CLOCK_ACCURACY			0xFE

#define DEFAULT_CLOCKCLASS_VALIDATE_TIMEOUT	5		//new staff

#define DEFAULT_CLOCK_CLASS			187

#define DEFAULT_PRIORITY1			128
#define DEFAULT_PRIORITY2			128
#define DEFAULT_CLOCK_VARIANCE			-4000 //To be determined in 802.1AS...so same value of ptpdv1 is used

/* In WR mode we need only one foreign master */
#ifdef WRPC_EXTRA_SLIM
#define DEFAULT_MAX_FOREIGN_RECORDS		1
#else
#define DEFAULT_MAX_FOREIGN_RECORDS		5
#endif

#define DEFAULT_PARENTS_STATS			FALSE
#define DEFAULT_DISABLE_FALLBACK_WHEN_WR_FAILS TRUE

/* features, only change to refelect changes in implementation */
#define NUMBER_PORTS		2
#define VERSION_PTP		2
#define TWO_STEP_FLAG		0x02
#define SLAVE_ONLY		FALSE
#define NO_ADJUST		FALSE

/** \name Packet length
 Minimal length values for each message.
 If TLV used length could be higher.*/
 /**\{*/
#define HEADER_LENGTH			34
#define ANNOUNCE_LENGTH			64
#define SYNC_LENGTH			44
#define FOLLOW_UP_LENGTH		44
#define PDELAY_REQ_LENGTH		54
#define DELAY_REQ_LENGTH		44
#define DELAY_RESP_LENGTH		54
#define PDELAY_RESP_LENGTH		54
#define PDELAY_RESP_FOLLOW_UP_LENGTH	54
#define MANAGEMENT_LENGTH		48
/** \}*/


/** \name White Rabbit staff
  Here comes all the constants needed in White Rabbit extension
.*/
 /**\{*/

  /*White Rabbit staff*/
  /*
   * if this defined, WR uses new implementation of timeouts (not using interrupt)
   */
//#define IRQ_LESS_TIMER

#define WR_NODE				0x80
#define WR_IS_CALIBRATED		0x04
#define WR_IS_WR_MODE			0x08
#define WR_NODE_MODE			0x03

# define WR_TLV_TYPE			0x2004

#define WR_MASTER_PRIORITY1		6
#define WR_DEFAULT_CAL_PERIOD		3000     //[us]
#define WR_DEFAULT_CAL_PATTERN		0x3E0     //1111100000
#define WR_DEFAULT_CAL_PATTERN_LEN	0xA       //10 bits

#define WR_DEFAULT_STATE_TIMEOUT_MS	300 //[ms]
#define WR_DEFAULT_STATE_REPEAT		3
#define WR_DEFAULT_INIT_REPEAT		3

/*White Rabbit package Size*/

#define WR_ANNOUNCE_TLV_LENGTH		0x0A

#define WR_ANNOUNCE_LENGTH		(ANNOUNCE_LENGTH + WR_ANNOUNCE_TLV_LENGTH + 4)
#define WR_MANAGEMENT_TLV_LENGTH	 6
#define WR_MANAGEMENT_LENGTH	 	(MANAGEMENT_LENGTH + WR_MANAGEMENT_TLV_LENGTH)

/* memory footprint tweak for WRPC */
#ifdef WRPC_EXTRA_SLIM
#define MAX_PORT_NUMBER			1
#else
#define MAX_PORT_NUMBER			32
#endif

#define MIN_PORT_NUMBER			1

#define WR_PORT_NUMBER			32

#define WR_SLAVE_CLOCK_CLASS		248
#define WR_MASTER_CLOCK_CLASS		5

#define WR_MASTER_ONLY_CLOCK_CLASS	70

///// new staff for WRPTPv2

#define TLV_TYPE_ORG_EXTENSION 		0x0003 //organization specific 

#define WR_PRIORITY1                    64

#define WR_TLV_ORGANIZATION_ID		0x080030
#define WR_TLV_MAGIC_NUMBER		0xDEAD
#define WR_TLV_WR_VERSION_NUMBER	0x01

#define WR_SIGNALING_MSG_BASE_LENGTH	48  //=length( header ) + lenght( targetPortId ) + length (tlvType) + lenght(lenghtField) 
					    //      34          +           10           +         2        +     2 

#define WR_DEFAULT_PHY_CALIBRATION_REQUIRED FALSE

#define     SEND_CALIBRATION_PATTERN 	0X0001
#define NOT_SEND_CALIBRATION_PATTERN 	0X0000


/** \}*/

/*Enumeration defined in tables of the spec*/

/**
 * \brief Domain Number (Table 2 in the spec)*/

enum {
	DFLT_DOMAIN_NUMBER = 0,
	ALT1_DOMAIN_NUMBER,
	ALT2_DOMAIN_NUMBER,
	ALT3_DOMAIN_NUMBER
};

/**
 * \brief Network Protocol  (Table 3 in the spec)*/
enum {
	UDP_IPV4=1,
	UDP_IPV6,IEE_802_3,
	DeviceNet,
	ControlNet,
	PROFINET
};

/**
 * \brief Time Source (Table 7 in the spec)*/
enum {
	ATOMIC_CLOCK=0x10,
	GPS=0x20,
	TERRESTRIAL_RADIO=0x30,
	PTP=0x40,
	NTP=0x50,
	HAND_SET=0x60,
	OTHER=0x90,
	INTERNAL_OSCILLATOR=0xA0
};


/**
 * \brief PTP State (Table 8 in the spec)*/
enum {
	INITIALIZING=1,
	FAULTY,
	DISABLED,
	LISTENING,
	PRE_MASTER,
	MASTER,
	PASSIVE,
	UNCALIBRATED,
	SLAVE
};

/**
 * \brief Delay mechanism (Table 9 in the spec)*/
enum {
	E2E=1,
	P2P=2,
	DELAY_DISABLED=0xFE
};


/**
 * \brief PTP timers
 */
enum {
	PDELAYREQ_INTERVAL_TIMER=0,/**<\brief Timer handling the PdelayReq Interval*/
	DELAYREQ_INTERVAL_TIMER,/**<\brief Timer handling the delayReq Interva*/
	SYNC_INTERVAL_TIMER,/**<\brief Timer handling Interval between master sends two Syncs messages */
	ANNOUNCE_RECEIPT_TIMER,/**<\brief Timer handling announce receipt timeout*/
	ANNOUNCE_INTERVAL_TIMER, /**<\brief Timer handling interval before master sends two announce messages*/
	TIMER_ARRAY_SIZE  /* this one is non-spec */
};

/**
 * \brief PTP states
 */
enum {
	PTP_INITIALIZING=0,	PTP_FAULTY,		PTP_DISABLED,
	PTP_LISTENING,		PTP_PRE_MASTER,		PTP_MASTER,
	PTP_PASSIVE,		PTP_UNCALIBRATED,	PTP_SLAVE
};



/**
 * \brief PTP Messages
 */
enum {
	SYNC=0x0,
	DELAY_REQ,
	PDELAY_REQ,
	PDELAY_RESP,
	FOLLOW_UP=0x8,
	DELAY_RESP,
	PDELAY_RESP_FOLLOW_UP,
	ANNOUNCE,
	SIGNALING,
	MANAGEMENT,
};

enum {
	PTP_ETHER,
	PTP_DEFAULT
};


/**
 * \brief Indicates if a port is configured as White Rabbit, and what kind (master/slave) [White Rabbit]
 */
/*White Rabbit node */
enum{
	NON_WR      = 0x0,
	WR_S_ONLY   = 0x2, 
	WR_M_ONLY   = 0x1,
	WR_M_AND_S  = 0x3,
	WR_MODE_AUTO= 0x4, // only for ptpx - not in the spec
};

/**
 * \brief Indicate current White Rabbit mode of a given port (non wr/wr master/wr slave) [White Rabbit]
 */
/*White Rabbit node */
enum{
	//NON_WR = 0x0,
	// below tric used in calling e.g.: ptpd_netif_locking_enable()
	WR_SLAVE  = PTPD_NETIF_RX, // just for convenient useage with ptpd_netif interface
	WR_MASTER = PTPD_NETIF_TX, // just for convenient useage with ptpd_netif interface
};

/**
 * \brief Values of Management Actions (extended for WR), see table 38 [White Rabbit]
 */
enum{
	GET,
	SET,
	RESPONSE,
	COMMAND,
	ACKNOWLEDGE,
	WR_CMD, //White Rabbit
};


/**
 * \brief WR PTP states (new, single FSM) [White Rabbit]
 */
enum {
	WRS_PRESENT = 0,  WRS_S_LOCK, WRS_M_LOCK,  WRS_LOCKED,
	WRS_CALIBRATION,  WRS_CALIBRATED,  WRS_RESP_CALIB_REQ ,WRS_WR_LINK_ON,
	/*
	  each WR main state (except IDLE) has an associated timetout
	  we use state names to manage timeouts as well
	*/
	WR_TIMER_ARRAY_SIZE, //number of states which has timeouts
	WRS_IDLE,
	/* here are substates*/
	WRS_S_LOCK_1,
	WRS_S_LOCK_2,
	WRS_CALIBRATION_1,
	WRS_CALIBRATION_2,
	WRS_CALIBRATION_3,
	WRS_CALIBRATION_4,
	WRS_CALIBRATION_5,
	WRS_CALIBRATION_6,
	WRS_CALIBRATION_7,
	WRS_CALIBRATION_8,
	WRS_RESP_CALIB_REQ_1,
	WRS_RESP_CALIB_REQ_2,
	WRS_RESP_CALIB_REQ_3,

};


/**
 * \brief White Rabbit commands (for new implementation, single FSM), see table 38 [White Rabbit]
 */
enum{
	
	NULL_WR_TLV = 0x0000,
	SLAVE_PRESENT	= 0x1000,
	LOCK,
	LOCKED,
	CALIBRATE,
	CALIBRATED,
	WR_MODE_ON,	
	ANN_SUFIX = 0x2000,
};


/**
 * \brief White Rabbit slave port's role 
 */

enum{
	NON_SLAVE	= 0x0,
	PRIMARY_SLAVE 	,
	SECONDARY_SLAVE ,
};

/**
 * \brief White Rabbit data initialization  mode
 */

enum{
	INIT,
	RE_INIT,
};

/** \name Best Master Clock 
  Here are constants used in the full implementation of BMC.
.*/
 /**\{*/

#define A_better_by_topology_then_B 	-2
#define A_better_then_B 		-1
#define B_better_then_A 		1
#define B_better_by_topology_then_A	2

#define A_equals_B			0
#define DSC_error			0

/** \}*/


#endif /*CONSTANTS_H_*/
