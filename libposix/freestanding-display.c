#include <ptpd.h>

void timeInternal_display(TimeInternal *timeInternal) {
}

void displayConfigINFO(RunTimeOpts* rtOpts)
{

    int i;
   /* White rabbit debugging info*/
    PTPD_TRACE(TRACE_PTPD_MAIN, NULL,"------------- INFO ----------------------\n\n");
    if(rtOpts->E2E_mode)
      PTPD_TRACE(TRACE_PTPD_MAIN, NULL,"E2E_mode ........................ TRUE\n")
    else
      PTPD_TRACE(TRACE_PTPD_MAIN, NULL,"P2P_mode ........................ TRUE\n")
    
    PTPD_TRACE(TRACE_PTPD_MAIN, NULL,"portNumber  ..................... %d\n",rtOpts->portNumber);
    
    if(rtOpts->portNumber == 1 && rtOpts->autoPortDiscovery == FALSE)
     PTPD_TRACE(TRACE_PTPD_MAIN, NULL,"running as ...................... single port node (forced, no auto port number discovery)\n")
    else if(rtOpts->portNumber == 1 && rtOpts->autoPortDiscovery == TRUE)
     PTPD_TRACE(TRACE_PTPD_MAIN, NULL,"running as ...................... single port node (auto port number discovery)\n")
    else if(rtOpts->portNumber > 1 && rtOpts->autoPortDiscovery == TRUE)
     PTPD_TRACE(TRACE_PTPD_MAIN, NULL,"running as ....................... multi port node [%d] (auto port number discovery)\n",rtOpts->portNumber )
    else if(rtOpts->portNumber > 1 && rtOpts->autoPortDiscovery == FALSE)
     PTPD_TRACE(TRACE_PTPD_MAIN, NULL,"running as ....................... multi port node [%d] (forced, no auto port number discovery)\n",rtOpts->portNumber )
    else
     PTPD_TRACE(TRACE_PTPD_MAIN, NULL,"running as ....................... ERROR,should not get here\n")

    for(i = 0; i < rtOpts->portNumber; i++)
    {
      if(rtOpts->autoPortDiscovery == FALSE) //so the interface is forced, thus in rtOpts
      PTPD_TRACE(TRACE_PTPD_MAIN, NULL,"net ifaceName [port = %d] ........ %s\n",i+1,rtOpts->ifaceName[i]);
      
      if(rtOpts->wrConfig == WR_MODE_AUTO)
	PTPD_TRACE(TRACE_PTPD_MAIN, NULL,"wrConfig  [port = %d] ............ Autodetection (ptpx-implementation-specific) \n",i+1)
      else if(rtOpts->wrConfig == WR_M_AND_S)
	PTPD_TRACE(TRACE_PTPD_MAIN, NULL,"wrConfig  [port = %d] ............ Master and Slave \n",i+1)
      else if(rtOpts->wrConfig == WR_SLAVE)
	PTPD_TRACE(TRACE_PTPD_MAIN, NULL,"wrConfig  [port = %d] ............ Slave \n",i+1)
      else if(rtOpts->wrConfig == WR_MASTER)
	PTPD_TRACE(TRACE_PTPD_MAIN, NULL,"wrConfig  [port = %d] ............ Master \n",i+1)     
      else if(rtOpts->wrConfig == NON_WR)
	PTPD_TRACE(TRACE_PTPD_MAIN, NULL,"wrConfig  [port = %d] ............ NON_WR\n",i+1)
      else
	PTPD_TRACE(TRACE_PTPD_MAIN, NULL,"wrConfig  [port = %d] ............ ERROR\n",i+1)
    }    
    
    PTPD_TRACE(TRACE_PTPD_MAIN, NULL,"----------- now the fun ------------\n\n")     

    PTPD_TRACE(TRACE_PTPD_MAIN, NULL,"clockClass ....................... %d\n",rtOpts->clockQuality.clockClass);

}
