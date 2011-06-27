/* ptpd.c */

#include "ptpd.h"

/* Statically allocated run-time configuration data.
 * This should better be a set of functions, as some could be
 * defined as "const" for freestanding code, thus saving space in
 * associated code (if() are known at compile time).
 * Currently, I only removed the init code to compile-time stuff
 * (bss-ing it to then copy values is bigger than precompiling) -- ARub
 */
RunTimeOpts rtOpts = {
   .announceInterval = DEFAULT_ANNOUNCE_INTERVAL,
   .syncInterval = DEFAULT_SYNC_INTERVAL,
   .clockQuality.clockAccuracy = DEFAULT_CLOCK_ACCURACY,
   .clockQuality.clockClass = DEFAULT_CLOCK_CLASS,
   .clockQuality.offsetScaledLogVariance = DEFAULT_CLOCK_VARIANCE,
   .priority1 = DEFAULT_PRIORITY1,
   .priority2 = DEFAULT_PRIORITY2,
   .domainNumber = DEFAULT_DOMAIN_NUMBER,
   .slaveOnly = SLAVE_ONLY,
   .currentUtcOffset = DEFAULT_UTC_OFFSET,
   .noResetClock = DEFAULT_NO_RESET_CLOCK,
   .noAdjust = NO_ADJUST,
   .inboundLatency.nanoseconds = DEFAULT_INBOUND_LATENCY,
   .outboundLatency.nanoseconds = DEFAULT_OUTBOUND_LATENCY,
   .s = DEFAULT_DELAY_S,
   .ap = DEFAULT_AP,
   .ai = DEFAULT_AI,
   .max_foreign_records = DEFAULT_MAX_FOREIGN_RECORDS,

   /**************** White Rabbit *************************/
   .portNumber 		= NUMBER_PORTS,
   .wrNodeMode 		= NON_WR,
   .calibrationPeriod     = WR_DEFAULT_CAL_PERIOD,
   .calibrationPattern    = WR_DEFAULT_CAL_PATTERN,
   .calibrationPatternLen = WR_DEFAULT_CAL_PATTERN_LEN,
   .E2E_mode 		= TRUE,
   /********************************************************/
};

int main(int argc, char **argv)
{
   PtpClock *ptpClock;
   Integer16 ret;
   int i;

   netStartup();

  /*Initialize run time options with command line arguments*/
   if( !(ptpClock = ptpdStartup(argc, argv, &ret, &rtOpts)) )
     return ret;

    /* White rabbit debugging info*/
    if(rtOpts.E2E_mode)
    else

    for(i = 0; i < rtOpts.portNumber; i++)


    for(i = 0; i < rtOpts.portNumber; i++)
    {

      if(i == 0 && rtOpts.wrNodeMode == WR_SLAVE)
      else if(rtOpts.wrNodeMode != NON_WR)
      else
    }
    if(rtOpts.portNumber == 1)
    else

    if(rtOpts.wrNodeMode == WR_SLAVE)
    	ptpd_init_exports();

  /* do the protocol engine */
   if(rtOpts.portNumber == 1)
     protocol(&rtOpts, ptpClock);	 //forever loop..
   else if(rtOpts.portNumber > 1)
     multiProtocol(&rtOpts, ptpClock); 	//forever loop when many ports (not fully implemented/tested)
   else

   ptpdShutdown();


  return 1;
}
