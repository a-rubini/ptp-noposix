/**
 *\author van Kempen Alexandre
 * \mainpage Ptpd v2 Documentation
 * \version 0.1
 * \date 12 may 2009
 * \section implementation Implementation
 * PTTdV2 is not a full implementation of 1588 - 2008 standard.
 * It is implemented only with use of Transparent Clock and Peer delay mechanism, according to 802.1AS requierements.
*/



/**
*\file
* \brief Main functions used in ptpdv2
* 
* This header file includes all others headers.
* It defines functions which are not dependant of the operating system.
 */

#ifndef PTPD_H_
#define PTPD_H_


#include "constants.h"
#include "limits.h"
#include "dep/constants_dep.h"
#include "dep/datatypes_dep.h"
#include "datatypes.h"
#include "dep/ptpd_dep.h"
#include "dep/trace.h"
#include "wr_protocol.h"
#include "ptpd_netif.h"



/** \name arith.c
 * -Timing management and arithmetic*/
 /**\{*/
/* arith.c */

/**
 * \brief Convert Integer64 into TimeInternal structure 
 */
void integer64_to_internalTime(Integer64,TimeInternal*);
/**
 * \brief Convert TimeInternal into Timestamp structure (defined by the spec)
 */
void fromInternalTime(TimeInternal*,Timestamp*);

/**
 * \brief Convert Timestamp to TimeInternal structure (defined by the spec)
 */
void toInternalTime(TimeInternal*,Timestamp*);

/**
 * \brief Use to normalize a TimeInternal structure 
 * 
 * The nanosecondsField member must always be less than 10⁹
 * This function is used after adding or substracting TimeInternal
 */
void normalizeTime(TimeInternal*);

/**
 * \brief Add two InternalTime structure and normalize
 */
void addTime(TimeInternal*,TimeInternal*,TimeInternal*);

/**
 * \brief Substract two InternalTime structure and normalize
 */
void subTime(TimeInternal*,TimeInternal*,TimeInternal*);
/** \}*/

/** \name bmc.c
 * -Best Master Clock Algorithm functions*/
 /**\{*/
/* bmc.c */
/**
 * \brief Compare data set of foreign masters and local data set
 * \return The recommended state for the port 
 */
UInteger8 bmc(ForeignMasterRecord*,RunTimeOpts*,PtpPortDS*);

/**
 * \brief Data Set comparision algorithm implementation
 * \return  
       A_better_by_topology_then_B 	(= -2)
      A_better_then_B 			(= -1)
      B_better_then_A 			(= 1)
      B_better_by_topology_then_A	(= 2)
      A_equals_B			(= 0)
      DSC_error				(= 0)
 */
Integer8 bmcDataSetComparison(MsgHeader*, MsgAnnounce*, UInteger16, MsgHeader*, \
			      MsgAnnounce*, UInteger16, PtpPortDS*);
/**
 * \brief Calculates Erbest - which foreign master is the best on a give port
 */
UInteger8 ErBest(ForeignMasterRecord *foreignMaster,PtpPortDS *ptpPortDS );

/**
 * \brief Calculates Ebest - the best foreign master for all the ports.
 */
UInteger8 EBest(PtpPortDS *ptpPortDS );


/**
 * \brief When recommended state is Master, copy local data into parent and grandmaster dataset
 */
void m1(PtpPortDS*);

/**
 * \brief When recommended state is Master, behave as defined in PTP standard
 */
void m3(PtpPortDS*);

/**
 * \brief When recommended state is Passive, behave as defined in PTP standard
 */
void p1(PtpPortDS*);

/**
 * \brief When recommended state is Slave, copy dataset of master into parent and grandmaster dataset
 */
void s1(MsgHeader*,MsgAnnounce*,PtpPortDS*);

/**
 * \brief When recommended state is Slave as a result of modified BMC, behave as described in WRSPEC
 */
void s2(MsgHeader*,MsgAnnounce*,PtpPortDS*);


/**
 * \brief Initialize port Data
 */
void initDataPort(RunTimeOpts*,PtpPortDS*);
/** \}*/

/**
 * \brief Initialize clock Data
 */
void initDataClock(RunTimeOpts*, PtpClockDS*);
/** \}*/


/** \name protocol.c
 * -Execute the protocol engine*/
 /**\{*/
/**
 * \brief Protocol engine
 */
/* protocol.c */
void protocol(RunTimeOpts*,PtpPortDS*);
/** \}*/


//Diplay functions usefull to debug
void displayRunTimeOpts(RunTimeOpts*);
void displayDefault (PtpPortDS*);
void displayCurrent (PtpPortDS*);
void displayParent (PtpPortDS*);
void displayGlobal (PtpPortDS*);
void displayPort (PtpPortDS*);
void displayForeignMaster (PtpPortDS*);
void displayOthers (PtpPortDS*);
void displayBuffer (PtpPortDS*);
void displayPtpPortDS (PtpPortDS*);
void displayConfigINFO(RunTimeOpts*);
void timeInternal_display(TimeInternal*);
void clockIdentity_display(ClockIdentity);
void netPath_display(NetPath*);
void intervalTimer_display(IntervalTimer*);
void integer64_display (Integer64*);
void timeInterval_display(TimeInterval*);
void portIdentity_display(PortIdentity*);
void clockQuality_display (ClockQuality*);
void iFaceName_display(Octet*);
void unicast_display(Octet*);

void msgHeader_display(MsgHeader*);
void msgAnnounce_display(MsgAnnounce*);
void msgSync_display(MsgSync *sync);
void msgFollowUp_display(MsgFollowUp*);
void msgPDelayReq_display(MsgPDelayReq*);


//added by ML
/*
 * implementation of multi-port daemon
 * each port independant 
 * at the moment:
 * - not much tested
 * TODO:
 * - only one port is allowed to calibrate at a time, need to implement some synch of that
 * - test 
 */
void multiProtocol(RunTimeOpts *rtOpts, PtpPortDS *ptpPortDS);

void clearForeignMasters(PtpPortDS *ptpPortDS);

void checkClockClassValidity(PtpClockDS *ptpClockDS);

Boolean globalBestForeignMastersUpdate(PtpPortDS*);
Boolean globalSecondSlavesUpdate(PtpPortDS *ptpPortDS);

int wr_servo_init(PtpPortDS *clock);
int wr_servo_got_sync(PtpPortDS *clock, TimeInternal t1, TimeInternal t2);
int wr_servo_got_delay(PtpPortDS *clock, Integer32 cf);
int wr_servo_update(PtpPortDS *clock);

/* What follows are the protopytes that were missing when I started (ARub) */
extern void do_irq_less_timing(PtpPortDS *ptpPortDS);
#if __STDC_HOSTED__
	extern void ptpd_handle_wripc(void);
#endif
extern void handleFollowUp(MsgHeader *header, Octet *msgIbuf,
			   ssize_t length, Boolean isFromSelf,
			   RunTimeOpts *rtOpts, PtpPortDS *ptpPortDS);
extern void msgUnpackDelayResp(void *buf,MsgDelayResp *resp);

extern void msgPackDelayReq(void *buf,Timestamp *originTimestamp,
			    PtpPortDS *ptpPortDS);
extern void msgPackDelayResp(void *buf,MsgHeader *header,PtpPortDS *ptpPortDS);

extern void toState(UInteger8 state, RunTimeOpts *rtOpts, PtpPortDS *ptpPortDS);
extern void handle(RunTimeOpts*,PtpPortDS*);



void msgUnpackWRSignalingMsg(void *buf,MsgSignaling *signalingMsg, Enumeration16 *wrMessageID, 
			     PtpPortDS *ptpPortDS );
extern UInteger16 msgPackWRSignalingMsg(void *buf,PtpPortDS *ptpPortDS, Enumeration16 wrMessageID);

extern void issueWRSignalingMsg(Enumeration16 wrMessageID,RunTimeOpts *rtOpts,PtpPortDS *ptpPortDS);

			      
#if __STDC_HOSTED__
	extern void ptpd_init_exports(void);
#endif

extern void wr_servo_enable_tracking(int enable);
extern int wr_servo_man_adjust_phase(int phase);

/* The code used pow(2, ...) but we don't want floating point here (ARub) */
static inline unsigned long pow_2(int exp)
{
	return 1 << exp;
}


void singlePortLoop(RunTimeOpts *rtOpts, PtpPortDS *ptpPortDS, int portIndex);
void sharedPortsLoop(PtpPortDS *ptpPortDS);

#endif /*PTPD_H_*/

