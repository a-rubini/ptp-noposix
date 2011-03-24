/* ptpd.c */

#include "ptpd.h"

RunTimeOpts rtOpts;  /* statically allocated run-time configuration data */

void init_default_rtopts(RunTimeOpts *rto)
{
   rto->announceInterval = DEFAULT_ANNOUNCE_INTERVAL;
   rto->syncInterval = DEFAULT_SYNC_INTERVAL;
   rto->clockQuality.clockAccuracy = DEFAULT_CLOCK_ACCURACY;
   rto->clockQuality.clockClass = DEFAULT_CLOCK_CLASS;
   rto->clockQuality.offsetScaledLogVariance = DEFAULT_CLOCK_VARIANCE;
   rto->priority1 = DEFAULT_PRIORITY1;
   rto->priority2 = DEFAULT_PRIORITY2;
   rto->domainNumber = DEFAULT_DOMAIN_NUMBER;
   rto->slaveOnly = SLAVE_ONLY;
   rto->currentUtcOffset = DEFAULT_UTC_OFFSET;
   rto->noResetClock = DEFAULT_NO_RESET_CLOCK;
   rto->noAdjust = NO_ADJUST;
   rto->inboundLatency.nanoseconds = DEFAULT_INBOUND_LATENCY;
   rto->outboundLatency.nanoseconds = DEFAULT_OUTBOUND_LATENCY;
   rto->s = DEFAULT_DELAY_S;
   rto->ap = DEFAULT_AP;
   rto->ai = DEFAULT_AI;
   rto->max_foreign_records = DEFAULT_MAX_FOREIGN_RECORDS;

   /**************** White Rabbit *************************/
   rto->portNumber 		= NUMBER_PORTS;
   rto->wrNodeMode 		= NON_WR;
   rto->calibrationPeriod     = WR_DEFAULT_CAL_PERIOD;
   rto->calibrationPattern    = WR_DEFAULT_CAL_PATTERN;
   rto->calibrationPatternLen = WR_DEFAULT_CAL_PATTERN_LEN;
   rto->E2E_mode 		= TRUE;
   /********************************************************/ 

}


int main(int argc, char **argv)
{
   PtpClock *ptpClock;
   Integer16 ret;
   int i;
   /* initialize run-time options to default values */ 

   init_default_rtopts(&rtOpts);
   
   netStartup();

  /*Initialize run time options with command line arguments*/
   if( !(ptpClock = ptpdStartup(argc, argv, &ret, &rtOpts)) )
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
    
      if(i == 0 && rtOpts.wrNodeMode == WR_SLAVE)
	DBG("wrNodeMode    [port = %d] ........ Slave \n",i+1);   
      else if(rtOpts.wrNodeMode != NON_WR) 
	DBG("wrNodeMode    [port = %d] ........ Master\n",i+1);
      else
	DBG("wrNodeMode    [port = %d] ........ NON WR\n",i+1); 
    }
    if(rtOpts.portNumber == 1)
	DBG("running as ...................... single port node\n"); 
    else
	DBG("running as ....................... multi port node [%d]\n",rtOpts.portNumber ); 
    
    DBG("----------- now the fun ------------\n\n");
  



//    if(rtOpts.wrNodeMode == WR_SLAVE)
//    	ptpd_init_exports();
   
  /* do the protocol engine */
   if(rtOpts.portNumber == 1)
     protocol(&rtOpts, ptpClock);	 //forever loop..
   else if(rtOpts.portNumber > 1)
     multiProtocol(&rtOpts, ptpClock); 	//forever loop when many ports (not fully implemented/tested) 
   else
     ERROR("Not appropriate portNumber\n");
 
   ptpdShutdown();
   
   NOTIFY("self shutdown, probably due to an error\n");

  return 1;
}
