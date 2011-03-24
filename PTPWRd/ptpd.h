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
UInteger8 bmc(ForeignMasterRecord*,RunTimeOpts*,PtpClock*);

/**
 * \brief When recommended state is Master, copy local data into parent and grandmaster dataset
 */
void m1(PtpClock*);

/**
 * \brief When recommended state is Slave, copy dataset of master into parent and grandmaster dataset
 */
void s1(MsgHeader*,MsgAnnounce*,PtpClock*);

/**
 * \brief Initialize datas
 */
void initData(RunTimeOpts*,PtpClock*);
/** \}*/


/** \name protocol.c
 * -Execute the protocol engine*/
 /**\{*/
/**
 * \brief Protocol engine
 */
/* protocol.c */
void protocol(RunTimeOpts*,PtpClock*);
/** \}*/


//Diplay functions usefull to debug
void displayRunTimeOpts(RunTimeOpts*);
void displayDefault (PtpClock*);
void displayCurrent (PtpClock*);
void displayParent (PtpClock*);
void displayGlobal (PtpClock*);
void displayPort (PtpClock*);
void displayForeignMaster (PtpClock*);
void displayOthers (PtpClock*);
void displayBuffer (PtpClock*);
void displayPtpClock (PtpClock*);
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
void multiProtocol(RunTimeOpts *rtOpts, PtpClock *ptpClock);




int wr_servo_init(PtpClock *clock);
int wr_servo_got_sync(PtpClock *clock, TimeInternal t1, TimeInternal t2);
int wr_servo_got_delay(PtpClock *clock, Integer32 cf);
int wr_servo_update(PtpClock *clock);

/* What follows are the protopytes that were missing when I started (ARub) */
extern void do_irq_less_timing(PtpClock *ptpClock);
extern void ptpd_handle_wripc(void);
extern void handleFollowUp(MsgHeader *header, Octet *msgIbuf,
			   ssize_t length, Boolean isFromSelf,
			   RunTimeOpts *rtOpts, PtpClock *ptpClock);
extern void msgUnpackDelayResp(void *buf,MsgDelayResp *resp);
extern void msgUnpackWRManagement(void *buf,MsgManagement *management,
				  Enumeration16 *wr_managementId,
				  PtpClock *ptpClock );
extern void msgPackDelayReq(void *buf,Timestamp *originTimestamp,
			    PtpClock *ptpClock);
extern void msgPackDelayResp(void *buf,MsgHeader *header,PtpClock *ptpClock);
extern UInteger16 msgPackWRManagement(void *buf,PtpClock *ptpClock,
					   Enumeration16 wr_managementId);
extern void toState(UInteger8 state, RunTimeOpts *rtOpts, PtpClock *ptpClock);
extern void handle(RunTimeOpts*,PtpClock*);
extern void issueWRManagement(Enumeration16 wr_managementId,RunTimeOpts*,
			      PtpClock*);
extern void ptpd_init_exports(void);
extern void wr_servo_enable_tracking(int enable);
extern int wr_servo_man_adjust_phase(int phase);


#endif /*PTPD_H_*/
