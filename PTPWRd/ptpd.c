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
   .calPeriod     	= WR_DEFAULT_CAL_PERIOD,
   .E2E_mode 		= TRUE,
   .wrConfig		= WR_MODE_AUTO, //autodetection
   .wrStateRetry	= WR_DEFAULT_STATE_REPEAT,
   .wrStateTimeout	= WR_DEFAULT_STATE_TIMEOUT_MS,
   .deltasKnown		= WR_DEFAULT_DELTAS_KNOWN,
   .knownDeltaTx	= WR_DEFAULT_DELTA_TX,
   .knownDeltaRx	= WR_DEFAULT_DELTA_RX,

   /********************************************************/
};

int main(int argc, char **argv)
{
   PtpPortDS *ptpPortDS;
   PtpClockDS ptpClockDS;
   Integer16 ret;
   int i;

   netStartup();

  /*Initialize run time options with command line arguments*/
   if( !(ptpPortDS = ptpdStartup(argc, argv, &ret, &rtOpts, &ptpClockDS)) )
     return ret;

    /* White rabbit debugging info*/
    DBG("------------- INFO ----------------------\n\n");
    if(rtOpts.E2E_mode)
      DBG("E2E_mode ........................ TRUE\n");
    else
      DBG("P2P_mode ........................ TRUE\n");

    DBG("portNumber  ..................... %d\n",rtOpts.portNumber);
    for(i = 0; i < rtOpts.portNumber; i++)
      DBG("net ifaceName [port = %d] ........ %s\n",i+1,rtOpts.ifaceName[i]);


    for(i = 0; i < rtOpts.portNumber; i++)
    {

      if(rtOpts.wrConfig == WR_MODE_AUTO)
	DBG("wrConfig  [port = %d] ............ Autodetection (ptpx-implementation-specific) \n",i+1);
      else if(rtOpts.wrConfig == WR_M_AND_S)
	DBG("wrConfig  [port = %d] ............ Master and Slave \n",i+1);
      else if(rtOpts.wrConfig == WR_SLAVE)
	DBG("wrConfig  [port = %d] ............ Slave \n",i+1);
      else if(rtOpts.wrConfig == WR_MASTER)
	DBG("wrConfig  [port = %d] ............ Master \n",i+1);      
      else if(rtOpts.wrConfig == NON_WR)
	DBG("wrConfig  [port = %d] ............ NON_WR\n",i+1);
      else
	DBG("wrConfig  [port = %d] ............ ERROR\n",i+1);
    }
    if(rtOpts.portNumber == 1)
	DBG("running as ...................... single port node\n");
    else
	DBG("running as ....................... multi port node [%d]\n",rtOpts.portNumber );

    DBG("----------- now the fun ------------\n\n");




//    if(rtOpts.wrMode == WR_SLAVE)
//    	ptpd_init_exports();
    initDataClock(&rtOpts, &ptpClockDS);
  /* do the protocol engine */
   if(rtOpts.portNumber == 1)
     protocol(&rtOpts, ptpPortDS);	 //forever loop..
   else if(rtOpts.portNumber > 1)
     multiProtocol(&rtOpts, ptpPortDS); 	//forever loop when many ports (not fully implemented/tested)
   else
     ERROR("Not appropriate portNumber\n");

   ptpdShutdown();

   NOTIFY("self shutdown, probably due to an error\n");

  return 1;
}
