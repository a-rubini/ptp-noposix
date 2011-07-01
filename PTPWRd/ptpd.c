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
   .autoPortDiscovery  	= TRUE,
   .primarySource	= FALSE,
   
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
   .masterOnly		= FALSE,
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
    PTPD_TRACE(TRACE_PTPD_MAIN, NULL,"------------- INFO ----------------------\n\n");
    if(rtOpts.E2E_mode)
      PTPD_TRACE(TRACE_PTPD_MAIN, NULL,"E2E_mode ........................ TRUE\n")
    else
      PTPD_TRACE(TRACE_PTPD_MAIN, NULL,"P2P_mode ........................ TRUE\n")
    
    PTPD_TRACE(TRACE_PTPD_MAIN, NULL,"portNumber  ..................... %d\n",rtOpts.portNumber);
    
    if(rtOpts.portNumber == 1 && rtOpts.autoPortDiscovery == FALSE)
     PTPD_TRACE(TRACE_PTPD_MAIN, NULL,"running as ...................... single port node (forced, no auto port number discovery)\n")
    else if(rtOpts.portNumber == 1 && rtOpts.autoPortDiscovery == TRUE)
     PTPD_TRACE(TRACE_PTPD_MAIN, NULL,"running as ...................... single port node (auto port number discovery)\n")
    else if(rtOpts.portNumber > 1 && rtOpts.autoPortDiscovery == TRUE)
     PTPD_TRACE(TRACE_PTPD_MAIN, NULL,"running as ....................... multi port node [%d] (auto port number discovery)\n",rtOpts.portNumber )
    else if(rtOpts.portNumber > 1 && rtOpts.autoPortDiscovery == FALSE)
     PTPD_TRACE(TRACE_PTPD_MAIN, NULL,"running as ....................... multi port node [%d] (forced, no auto port number discovery)\n",rtOpts.portNumber )
    else
     PTPD_TRACE(TRACE_PTPD_MAIN, ptpPortDS,"running as ....................... ERROR,should not get here\n")

    for(i = 0; i < rtOpts.portNumber; i++)
    {
      if(rtOpts.autoPortDiscovery == FALSE) //so the interface is forced, thus in rtOpts
      PTPD_TRACE(TRACE_PTPD_MAIN, NULL,"net ifaceName [port = %d] ........ %s\n",i+1,rtOpts.ifaceName[i]);
      
      if(rtOpts.wrConfig == WR_MODE_AUTO)
	PTPD_TRACE(TRACE_PTPD_MAIN, NULL,"wrConfig  [port = %d] ............ Autodetection (ptpx-implementation-specific) \n",i+1)
      else if(rtOpts.wrConfig == WR_M_AND_S)
	PTPD_TRACE(TRACE_PTPD_MAIN, NULL,"wrConfig  [port = %d] ............ Master and Slave \n",i+1)
      else if(rtOpts.wrConfig == WR_SLAVE)
	PTPD_TRACE(TRACE_PTPD_MAIN, NULL,"wrConfig  [port = %d] ............ Slave \n",i+1)
      else if(rtOpts.wrConfig == WR_MASTER)
	PTPD_TRACE(TRACE_PTPD_MAIN, NULL,"wrConfig  [port = %d] ............ Master \n",i+1)     
      else if(rtOpts.wrConfig == NON_WR)
	PTPD_TRACE(TRACE_PTPD_MAIN, NULL,"wrConfig  [port = %d] ............ NON_WR\n",i+1)
      else
	PTPD_TRACE(TRACE_PTPD_MAIN, NULL,"wrConfig  [port = %d] ............ ERROR\n",i+1)
    }    
    
    PTPD_TRACE(TRACE_PTPD_MAIN, NULL,"----------- now the fun ------------\n\n")     
   
   ptpd_init_exports();//from Tomeks'
   initDataClock(&rtOpts, &ptpClockDS);
      
   PTPD_TRACE(TRACE_PTPD_MAIN, NULL,"clockClass ....................... %d\n",ptpClockDS.clockQuality.clockClass);
	
    /* do the protocol engine */
   if(rtOpts.portNumber == 1)
     protocol(&rtOpts, ptpPortDS);		 //forever loop for single port
   else if(rtOpts.portNumber > 1)
     multiProtocol(&rtOpts, ptpPortDS); 	//forever loop when many ports
   else

   ptpdShutdown();


  return 1;
}
