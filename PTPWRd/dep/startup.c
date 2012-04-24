/* startup.c */

#include <stdlib.h> /* exit etc */
#include <unistd.h> /* getopt etc */
#include <signal.h> /* SIGINT etc */
#include <fcntl.h> /* creat */

#include "../ptpd.h"

PtpPortDS *ptpPortDS;

void catch_close(int sig)
{
  char *s;

  ptpdShutdown();

  switch(sig)
  {
  case SIGINT:
    s = "interrupt";
    break;

  case SIGTERM:
    s = "terminate";
    break;

  case SIGHUP:
    s = "hangup";
    break;

  default:
    s = "?";
  }


  exit(0);
}

void ptpdShutdown()
{

  int i;
  PtpPortDS * currentPtpdClockData;

  currentPtpdClockData = ptpPortDS;

  for (i=0; i < MAX_PORT_NUMBER; i++)
  {
     if (currentPtpdClockData->foreign)
     {
       free(currentPtpdClockData->foreign);
     }

     currentPtpdClockData++;
  }

  free(ptpPortDS);

}

PtpPortDS * ptpdStartup(int argc, char **argv, Integer16 *ret, RunTimeOpts *rtOpts,PtpClockDS *ptpClockDS)
{
  int c, fd = -1, /*nondaemon = 1,*/ noclose = 0;
  int startupMode = DEFAULT_STARTUP_MODE; //1=> daemon, 0=>nondaemon

  /* parse command line arguments */
  while( (c = getopt(argc, argv, "?cf:dDABMSNPxta:w:M:b:1:2:3:u:l:o:n:y:m:g:v:r:s:p:q:i:eh")) != -1 ) {
    switch(c) {
    case '?':
      printf(
"\nUsage:  ptpv2d [OPTION]\n\n"
"\n"
"-?                show this page\n"
"\n"
"-f FILE           send output to FILE\n"
"-D                display stats in .csv format\n"
"\n"
"\n"
"-b NAME           bind PTP to network interface NAME\n"
"-u ADDRESS        also send uni-cast to ADDRESS\n"
"-i NUMBER         specify PTP domain number\n"
"-n NUMBER         specify announce interval in 2^NUMBER sec\n"
"-y NUMBER         specify sync interval in 2^NUMBER sec\n"
"-r NUMBER         specify system clock accuracy\n"
"-v NUMBER         specify system clock class\n"
"-p NUMBER         specify priority1 attribute\n"
"-g [0/1]  			   enable/disable fallback to standard PTP mode in case of WR initialization failure\n"
"-d                run in daemon mode !!! \n"
"-c                run in non-daemon mode\n"
"-A                WR: hands free - multiport mode, autodetection of ports and interfaces ,"
		    "[default startup configuration]\n"
  "-q NUMBER         WR: override clock identity -- if you want to run two ptpd on the same machine"
			  " and they are supposed to cummunicate, this enables you to differentiated "
			  " their clock ID, so the ptpds think they are on different machiens\n"

"\n"
//"-k NUMBER,NUMBER  send a management message of key, record, then exit\n"  implemented later..
"\n"
      );
      *ret = 0;
      return 0;



    case 'f':
      if((fd = creat(optarg, 0400)) != -1)
      {
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        noclose = 1;
      }
      else
        PTPD_TRACE(TRACE_ERROR, NULL,"could not open output file");
      break;


    case 'd':
        startupMode = DAEMONE_MODE;

    case 'c':

        startupMode = NONDAEMONE_MODE;
      //nondaemon = 1;
      break;
/*
#ifndef PTPD_DBG
      rtOpts->displayStats = TRUE;
#endif
*/
      break;

    case 'D':
#ifndef PTPD_DBG
      rtOpts->displayStats = TRUE;
      rtOpts->csvStats = TRUE;
#endif
      break;

    case 'x':
      rtOpts->noResetClock = TRUE;
      break;

    case 't':
      rtOpts->noAdjust = TRUE;
      break;

    case 'a':
      rtOpts->ap = strtol(optarg, &optarg, 0);
      if(optarg[0])
        rtOpts->ai = strtol(optarg+1, 0, 0);
      break;

    case 'w':
      rtOpts->s = strtol(optarg, &optarg, 0);
      break;

    case 'b':
      memset(rtOpts->ifaceName[0], 0, IFACE_NAME_LENGTH);
      strncpy(rtOpts->ifaceName[0], optarg, IFACE_NAME_LENGTH);
      rtOpts->portNumber = 1;
      rtOpts->autoPortDiscovery = FALSE;
      break;

    case 'u':
      strncpy(rtOpts->unicastAddress, optarg, NET_ADDRESS_LENGTH);
      break;

    case 'l':
      rtOpts->inboundLatency.nanoseconds = strtol(optarg, &optarg, 0);
      if(optarg[0])
        rtOpts->outboundLatency.nanoseconds = strtol(optarg+1, 0, 0);
      break;

    case 'o':
      rtOpts->currentUtcOffset = strtol(optarg, &optarg, 0);
      break;

    case 'i':
      rtOpts->domainNumber = strtol(optarg, &optarg, 0);
      break;

    case 'y':
      rtOpts->syncInterval = strtol(optarg, 0, 0);
      break;

     case 'n':
     rtOpts->announceInterval=strtol(optarg, 0, 0);
     break;

//     case 'm':
//       rtOpts->max_foreign_records = strtol(optarg, 0, 0);
//       if(rtOpts->max_foreign_records < 1)
//         rtOpts->max_foreign_records = 1;
//       break;

    case 'g':
     rtOpts->disableFallbackIfWRFails=strtol(optarg, 0, 0) ? TRUE : FALSE;

      break;

//     case 's':
//       rtOpts->clockQuality.offsetScaledLogVariance = strtol(optarg, 0, 0);
//       break;

    case 'r':
      rtOpts->clockQuality.clockAccuracy = strtol(optarg, 0, 0);
      break;

    case 'v':
      rtOpts->clockQuality.clockClass = strtol(optarg, 0, 0);
      break;

    case 'p':
      rtOpts->priority1 = strtol(optarg, 0, 0);
      break;

    case 'q':

      rtOpts->overrideClockIdentity  = strtol(optarg, 0, 0);


      break;

   case 'e':
      rtOpts->ethernet_mode = TRUE;

      return 0;
      break;

   case 'h':
	   rtOpts->E2E_mode = FALSE;
	   break;


   case 'A':
	   PTPD_TRACE(TRACE_STARTUP, NULL, "WR AUTO MODE\n");
	   rtOpts->portNumber 		= WR_PORT_NUMBER;
	   rtOpts->wrConfig 		= WR_MODE_AUTO;
	   rtOpts->autoPortDiscovery 	= TRUE;
	   break;

   case 'M':
	    PTPD_TRACE(TRACE_STARTUP, NULL,"WR Master-only\n");
	   rtOpts->autoPortDiscovery 	= FALSE;
	   rtOpts->masterOnly 		= TRUE;
	   rtOpts->portNumber		= 1;
	   rtOpts->wrConfig 		= WR_M_ONLY; //only for ordinary clock
	   break;
   case 'm':

	   memset(rtOpts->ifaceName[0], 0, IFACE_NAME_LENGTH);
	   strncpy(rtOpts->ifaceName[0], optarg, IFACE_NAME_LENGTH);
	   rtOpts->autoPortDiscovery 	= FALSE;
	   rtOpts->masterOnly 		= TRUE;
	   rtOpts->portNumber		= 1;
	   rtOpts->wrConfig 		= WR_M_ONLY; //only for ordinary clock

	   PTPD_TRACE(TRACE_STARTUP, NULL,"WR Master-only : interface specified: %s\n", rtOpts->ifaceName[0]);
	   break;

   case 'B':
	   PTPD_TRACE(TRACE_STARTUP, NULL,"WR Master and Slave\n");
	   rtOpts->wrConfig = WR_M_AND_S;
	   break;

   case 'N':
	   PTPD_TRACE(TRACE_STARTUP, NULL,"NON_WR wrMode !! \n");
	   rtOpts->wrConfig = NON_WR;
	   break;

   case 'P':
	   PTPD_TRACE(TRACE_STARTUP, NULL,"Primary Source of time (Timing Master) \n");
	   rtOpts->primarySource = TRUE;
	   break;

   case 'S':
	   PTPD_TRACE(TRACE_STARTUP, NULL,"WR wrConfig=WR_S_ONLY\n");
	   rtOpts->autoPortDiscovery 	= FALSE;
	   rtOpts->portNumber		= 1;
	   rtOpts->wrConfig 		= WR_S_ONLY; //only for ordinary clock
	   break;
   case 's':

	   memset(rtOpts->ifaceName[0], 0, IFACE_NAME_LENGTH);
	   strncpy(rtOpts->ifaceName[0], optarg, IFACE_NAME_LENGTH);
	   rtOpts->autoPortDiscovery 	= FALSE;
	   rtOpts->wrConfig 		= WR_S_ONLY; //only for ordinary clock


	   PTPD_TRACE(TRACE_STARTUP, NULL,"WR wrConfig=WR_S_ONLY: interface specified: %s\n", rtOpts->ifaceName[0]);

    case '1':
      memset(rtOpts->ifaceName[0], 0, IFACE_NAME_LENGTH);
      strncpy(rtOpts->ifaceName[0], optarg, IFACE_NAME_LENGTH);
      rtOpts->portNumber = 1;
      rtOpts->autoPortDiscovery = FALSE;
      break;

    case '2':
      memset(rtOpts->ifaceName[1], 0, IFACE_NAME_LENGTH);
      strncpy(rtOpts->ifaceName[1], optarg, IFACE_NAME_LENGTH);
      rtOpts->portNumber = 2;
      rtOpts->autoPortDiscovery = FALSE;
      break;

    case '3':
      memset(rtOpts->ifaceName[2], 0, IFACE_NAME_LENGTH);
      strncpy(rtOpts->ifaceName[2], optarg, IFACE_NAME_LENGTH);
      rtOpts->portNumber  = 3;
      rtOpts->autoPortDiscovery = FALSE;
      break;


    default:
      *ret = 1;
      return 0;
    }
  }

  ptpPortDS  = (PtpPortDS*)calloc(MAX_PORT_NUMBER, sizeof(PtpPortDS));


  PtpPortDS * currentPtpdClockData;

  if(!ptpPortDS)
  {
    *ret = 2;
    return 0;
  }
  else
  {
    if(startupMode == NONDAEMONE_MODE)
       PTPD_TRACE(TRACE_STARTUP, NULL,"allocated %d bytes for protocol engine data\n", (int)sizeof(PtpPortDS));

    if(rtOpts->autoPortDiscovery == TRUE)
      rtOpts->portNumber = autoPortNumberDiscovery();

    currentPtpdClockData = ptpPortDS;
    int i;

    for(i = 0; i < MAX_PORT_NUMBER; i++)
    {
	currentPtpdClockData->portIdentity.portNumber = i + 1;
	currentPtpdClockData->foreign = (ForeignMasterRecord*)calloc(rtOpts->max_foreign_records, sizeof(ForeignMasterRecord));

	if(!currentPtpdClockData->foreign)
	{
	    PTPD_TRACE(TRACE_ERROR, NULL,"failed to allocate memory for foreign master data");
	    *ret = 2;
	  //TODO:
	      free(ptpPortDS);
	    return 0;
	}
	else
	{
	    if(startupMode == NONDAEMONE_MODE)
	      PTPD_TRACE(TRACE_STARTUP, NULL,"allocated %d bytes for foreign master data @ port = %d\n",
		      (int)(rtOpts->max_foreign_records*sizeof(ForeignMasterRecord)),
		       currentPtpdClockData->portIdentity.portNumber);
	    /*Init to 0 net buffer*/

	}
	memset(currentPtpdClockData->msgIbuf,0,PACKET_SIZE);
	memset(currentPtpdClockData->msgObuf,0,PACKET_SIZE);

	currentPtpdClockData->ptpClockDS = ptpClockDS; // common data

	currentPtpdClockData++;
    }

    if(rtOpts->portNumber >1 && rtOpts->masterOnly == TRUE)
    {
      PTPD_TRACE(TRACE_ERROR, NULL,"ERROR: boundary clock cannot be masterOnly\n");
      return 0;
    }

  }

//#ifndef PTPD_NO_DAEMON
  //if(!nondaemon)
  PTPD_TRACE(TRACE_STARTUP, NULL,"WRPTP.v2 daemon started. HAVE FUN !!! \n");
  if(startupMode == DAEMONE_MODE)
  {
    if(daemon(0, noclose) == -1)
    {
      PTPD_TRACE(TRACE_ERROR, NULL,"failed to start as daemon");
      *ret = 3;
      return 0;
    }
  }
//#endif

  signal(SIGINT, catch_close);
  signal(SIGTERM, catch_close);
  signal(SIGHUP, catch_close);

  *ret = 0;

  return ptpPortDS;
}
