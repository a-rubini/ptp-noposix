#ifndef PTPD_DEP_H_
#define PTPD_DEP_H_

/**
*\file
* \brief Functions used in ptpdv2 which are platform-dependent
 */

#include <stdlib.h>
#include <stdio.h>
//#include <string.h>
#include <unistd.h>
#include <math.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <limits.h>
#include <sys/time.h>
#include <sys/timex.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>


#include "ptpd_netif.h"


 /** \name System messages*/
 /**\{*/

#define ERROR(x, ...)  fprintf(stderr, "(ptpd error) " x, ##__VA_ARGS__)
#define PERROR(x, ...) fprintf(stderr, "(ptpd error) " x ": %m\n", ##__VA_ARGS__)
#define NOTIFY(x, ...) fprintf(stderr, "(ptpd notice) " x, ##__VA_ARGS__)
/** \}*/

/** \name Debug messages*/
 /**\{*/

#ifdef PTPD_DBGV
#define PTPD_DBG
#define DBGV(x, ...) fprintf(stderr, "(DBG [%s()]) " x,__func__, ##__VA_ARGS__)
#else
#define DBGV(x, ...)
#endif



//this DBG we use when there is no PtpClock* in the function
#ifdef PTPD_DBG
#define DBGNPI(x, ...)  fprintf(stderr, "(ptpd debug) " x, ##__VA_ARGS__)
#else
#define DBGNPI(x, ...)
#endif

/** \}*/

/** \name Endian corrections*/
 /**\{*/

#if defined(PTPD_MSBF)
#define shift8(x,y)   ( (x) << ((3-y)<<3) )
#define shift16(x,y)  ( (x) << ((1-y)<<4) )
#elif defined(PTPD_LSBF)
#define shift8(x,y)   ( (x) << ((y)<<3) )
#define shift16(x,y)  ( (x) << ((y)<<4) )
#endif

#define flip16(x) htons(x)
#define flip32(x) htonl(x)



/* i don't know any target platforms that do not have htons and htonl,
   but here are generic funtions just in case */
/*
#if defined(PTPD_MSBF)
#define flip16(x) (x)
#define flip32(x) (x)
#elif defined(PTPD_LSBF)
static inline Integer16 flip16(Integer16 x)
{
   return (((x) >> 8) & 0x00ff) | (((x) << 8) & 0xff00);
}

static inline Integer32 flip32(x)
{
  return (((x) >> 24) & 0x000000ff) | (((x) >> 8 ) & 0x0000ff00) |
         (((x) << 8 ) & 0x00ff0000) | (((x) << 24) & 0xff000000);
}
#endif
*/

/** \}*/


/** \name Bit array manipulations*/
 /**\{*/

#define getFlag(x,y)  !!( *(UInteger8*)((x)+((y)<8?1:0)) &   (1<<((y)<8?(y):(y)-8)) )
#define setFlag(x,y)    ( *(UInteger8*)((x)+((y)<8?1:0)) |=   1<<((y)<8?(y):(y)-8)  )
#define clearFlag(x,y)  ( *(UInteger8*)((x)+((y)<8?1:0)) &= ~(1<<((y)<8?(y):(y)-8)) )
/** \}*/

/** \name msg.c
 *-Pack and unpack PTP messages */
 /**\{*/

void msgUnpackHeader(void*,MsgHeader*);
void msgUnpackAnnounce (void*,MsgAnnounce*, MsgHeader*);
void msgUnpackSync(void*,MsgSync*);
void msgUnpackFollowUp(void*,MsgFollowUp*);
void msgUnpackPDelayReq(void*,MsgPDelayReq*);
void msgUnpackPDelayResp(void*,MsgPDelayResp*);
void msgUnpackPDelayRespFollowUp(void*,MsgPDelayRespFollowUp*);
void msgUnpackManagement(void*,MsgManagement*);
UInteger8 msgUnloadManagement(void*,MsgManagement*,PtpClock*,RunTimeOpts*);
void msgUnpackManagementPayload(void *buf, MsgManagement *manage);
void msgPackHeader(void*,PtpClock*);
void msgPackAnnounce(void*,PtpClock*);
void msgPackSync(void*,Timestamp*,PtpClock*);
void msgPackFollowUp(void*,PtpClock*);
void msgPackPDelayReq(void*,Timestamp*,PtpClock*);
void msgPackPDelayResp(void*,MsgHeader*,Timestamp*,PtpClock*);

void msgPackPDelayRespFollowUp(void*,MsgHeader*,Timestamp*,PtpClock*);
UInteger16 msgPackManagement(void*,MsgManagement*,PtpClock*);
UInteger16 msgPackManagementResponse(void*,MsgHeader*,MsgManagement*,PtpClock*);
/** \}*/

/** \name net.c (Linux API dependent)
 * -Init network stuff, send and receive datas*/
 /**\{*/

Boolean netStartup();
Boolean netInit(NetPath*,RunTimeOpts*,PtpClock*);
Boolean netShutdown(NetPath*);
int netSelect(TimeInternal*,NetPath*);

ssize_t netRecvMsg(Octet*, NetPath*, wr_timestamp_t*);
ssize_t netSendEvent(Octet*, UInteger16, NetPath*, wr_timestamp_t*);
ssize_t netSendGeneral(Octet*,UInteger16,NetPath*);
ssize_t netSendPeerGeneral(Octet*,UInteger16,NetPath*);
ssize_t netSendPeerEvent(Octet*,UInteger16,NetPath*,wr_timestamp_t*);
/** \}*/

/** \name servo.c
 * -Clock servo*/
 /**\{*/

void initClock(RunTimeOpts*,PtpClock*);
void updatePeerDelay (one_way_delay_filter*, RunTimeOpts*,PtpClock*,TimeInternal*,Boolean);
void updateDelay (one_way_delay_filter*, RunTimeOpts*, PtpClock*,TimeInternal*);
void updateOffset(TimeInternal*,TimeInternal*,
  offset_from_master_filter*,RunTimeOpts*,PtpClock*,TimeInternal*);
void updateClock(RunTimeOpts*,PtpClock*);
/** \}*/

/** \name startup.c (Linux API dependent)
 * -Handle with runtime options*/
 /**\{*/
PtpClock * ptpdStartup(int,char**,Integer16*,RunTimeOpts*);
void ptpdShutdown(void);
/** \}*/

/** \name sys.c (Linux API dependent) 
 * -Manage timing system API*/
 /**\{*/
void displayStats(RunTimeOpts *rtOpts, PtpClock *ptpClock);
Boolean nanoSleep(TimeInternal*);
void getTime(TimeInternal*);
void setTime(TimeInternal*);
double getRand();
Boolean adjFreq(Integer32);
/** \}*/

/** \name timer.c (Linux API dependent) 
 * -Handle with timers*/
 /**\{*/
//void initTimer(void);
void initTimer(PtpClock*);
void timerUpdate(IntervalTimer*,UInteger16);
void timerStop(UInteger16,IntervalTimer*);
void timerStart(UInteger16,float,IntervalTimer*);
Boolean timerExpired(UInteger16,IntervalTimer*, UInteger16);
/** \}*/


/*Test functions*/


/*
 *
 *
 ***************** White Rabbit *****************************
 *
 *
 *
 */

/* includes */



/* debugging facilities */
//this DBG we use when there is PtpClock* in the function
#ifdef PTPD_DBG
# ifdef PTPD_DBG_S_FUN
#  define DBG(x, ...)  fprintf(stderr, "(DBG [p=%d, %15s()]) " x, ptpClock->portIdentity.portNumber,__func__, ##__VA_ARGS__)
# else
#  define DBG(x, ...)  fprintf(stderr, "(DBG [p=%d]) " x, ptpClock->portIdentity.portNumber, ##__VA_ARGS__)
# endif
#else
# define DBG(x, ...)
#endif


#ifdef PTPD_DBGWR
#define DBGWR(x, ...) fprintf(stderr, "(PTPWRd  debug) " x, ##__VA_ARGS__)
#else
#define DBGWR(x, ...)
#endif

#ifdef PTPD_DBGMSG
#define DBGM(x, ...) fprintf(stderr, "(PTPWRd msg) " x, ##__VA_ARGS__)
#else
#define DBGM(x, ...)
#endif


#endif /*PTPD_DEP_H_*/
