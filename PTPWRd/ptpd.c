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
   .currentUtcOffset = DEFAULT_UTC_OFFSET,
   .noResetClock = DEFAULT_NO_RESET_CLOCK,
   .noAdjust = NO_ADJUST,
   .inboundLatency.nanoseconds = DEFAULT_INBOUND_LATENCY,
   .outboundLatency.nanoseconds = DEFAULT_OUTBOUND_LATENCY,
   .s = DEFAULT_DELAY_S,
   .ap = DEFAULT_AP,
   .ai = DEFAULT_AI,
   .max_foreign_records = DEFAULT_MAX_FOREIGN_RECORDS,
   .autoPortDiscovery  	= TRUE,
   .primarySource	= FALSE,

   /**************** White Rabbit *************************/
   .portNumber 		= NUMBER_PORTS,
   .calPeriod     	= WR_DEFAULT_CAL_PERIOD,
   .E2E_mode 		= TRUE,
   .wrConfig		= WR_MODE_AUTO, //autodetection
   .wrStateRetry	= WR_DEFAULT_STATE_REPEAT,
   .wrStateTimeout	= WR_DEFAULT_STATE_TIMEOUT_MS,
   .phyCalibrationRequired		= WR_DEFAULT_PHY_CALIBRATION_REQUIRED,
   .masterOnly		= FALSE,
	.disableFallbackIfWRFails = DEFAULT_DISABLE_FALLBACK_WHEN_WR_FAILS
   /********************************************************/
};

int main(int argc, char **argv)
{
   PtpPortDS *ptpPortDS;
   PtpClockDS ptpClockDS;
   Integer16 ret;

   /* start netif */
   if( !netStartup())
     return -1;

   /*Initialize run time options with command line arguments*/
   if( !(ptpPortDS = ptpdStartup(argc, argv, &ret, &rtOpts, &ptpClockDS)) )
     return ret;
    
   ptpd_init_exports();//from Tomeks'
   
   /* initialize data sets common to entire boundary clock*/
   initDataClock(&rtOpts, &ptpClockDS);
   
   /* show the options you are starting with*/
   displayConfigINFO(&rtOpts);
	
    /* do the protocol engine */
   if(rtOpts.portNumber > 0) 
     multiProtocol(&rtOpts, ptpPortDS); //forever loop for any number of ports
   else
     ptpdShutdown();

  return 1;
}
