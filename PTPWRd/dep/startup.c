/* startup.c */

#include <stdlib.h> /* exit etc */
#include <unistd.h> /* getopt etc */
#include <signal.h> /* SIGINT etc */
#include <fcntl.h> /* creat */

#include "../ptpd.h"

PtpClock *ptpClock;

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

  NOTIFY("shutdown on %s signal\n", s);

  exit(0);
}

void ptpdShutdown()
{

  int i;
  PtpClock * currentPtpdClockData;

  currentPtpdClockData = ptpClock;

  for (i=0; i < MAX_PORT_NUMBER; i++)
  {
     netShutdown(&currentPtpdClockData->netPath);
     if (currentPtpdClockData->foreign)
     {
       DBG("freeing: p=%d\n",i+1);
       free(currentPtpdClockData->foreign);
     }
     //netShutdown(&currentPtpdClockData->netPath);
     currentPtpdClockData++;
  }

  free(ptpClock);

}

PtpClock * ptpdStartup(int argc, char **argv, Integer16 *ret, RunTimeOpts *rtOpts)
{
  int c, fd = -1, nondaemon = 0, noclose = 0;

  /* parse command line arguments */
  while( (c = getopt(argc, argv, "?cf:dDMASxta:w:b:1:2:3:u:l:o:n:y:m:gv:r:s:p:q:i:eh")) != -1 ) {
    switch(c) {
    case '?':
      printf(
"\nUsage:  ptpv2d [OPTION]\n\n"
"Ptpv2d runs on UDP/IP , P2P mode by default\n"
"\n"
"-?                show this page\n"
"\n"
"-c                run in command line (non-daemon) mode\n"
"-f FILE           send output to FILE\n"
"-d                display stats\n"
"-D                display stats in .csv format\n"
"\n"
"-x                do not reset the clock if off by more than one second\n"
"-t                do not adjust the system clock\n"
"-a NUMBER,NUMBER  specify clock servo P and I attenuations\n"
"-w NUMBER         specify one way delay filter stiffness\n"
"\n"
"-b NAME           bind PTP to network interface NAME\n"
"-u ADDRESS        also send uni-cast to ADDRESS\n"
"-e                run in ethernet mode (level2) \n"
"-h                run in peer-to-peer \n"
"-l NUMBER,NUMBER  specify inbound, outbound latency in nsec\n"

"\n"
"-o NUMBER         specify current UTC offset\n"
"-i NUMBER         specify PTP domain number\n"

"\n"
"-n NUMBER         specify announce interval in 2^NUMBER sec\n"
"-y NUMBER         specify sync interval in 2^NUMBER sec\n"
"-m NUMBER         specify max number of foreign master records\n"
"\n"
"-g                run as slave only\n"
"-v NUMBER         specify system clock allen variance\n"
"-r NUMBER         specify system clock accuracy\n"
"-s NUMBER         specify system clock class\n"
"-p NUMBER         specify priority1 attribute\n"

  "-A                WR: hands free - multiport mode, autodetection of ports and interfaces, HAVE FUN !!!!\n"
  "-M                WR: run PTP node as WR Master\n"
  "-S                WR: run PTP node as WR Slave\n"
  "-q NUMBER         WR: if you want to use one eth interface for testing ptpd (run two which communicate) define here different port numbers (need to be > 1)\n"
  "-1 NAME           WR: network interface for port 1\n"
  "-2 NAME           WR: network interface for port 2\n"
  "-3 NAME           WR: network interface for port 3\n"

"\n"
//"-k NUMBER,NUMBER  send a management message of key, record, then exit\n"  implemented later..
"\n"
      );
      *ret = 0;
      return 0;

    case 'c':
      nondaemon = 1;
      break;


    case 'f':
      if((fd = creat(optarg, 0400)) != -1)
      {
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        noclose = 1;
      }
      else
        PERROR("could not open output file");
      break;

    case 'd':
#ifndef PTPD_DBG
      rtOpts->displayStats = TRUE;
#endif
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
      ptpd_wrap_memset(rtOpts->ifaceName[0], 0, IFACE_NAME_LENGTH);
      ptpd_wrap_strncpy(rtOpts->ifaceName[0], optarg, IFACE_NAME_LENGTH);
      rtOpts->portNumber = 1;
      break;

    case 'u':
      ptpd_wrap_strncpy(rtOpts->unicastAddress, optarg, NET_ADDRESS_LENGTH);
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

    case 'm':
      rtOpts->max_foreign_records = strtol(optarg, 0, 0);
      if(rtOpts->max_foreign_records < 1)
        rtOpts->max_foreign_records = 1;
      break;

    case 'g':
      rtOpts->slaveOnly = TRUE;
      break;

    case 'v':
      rtOpts->clockQuality.offsetScaledLogVariance = strtol(optarg, 0, 0);
      break;

    case 'r':
      rtOpts->clockQuality.clockAccuracy = strtol(optarg, 0, 0);
      break;

    case 's':
      rtOpts->clockQuality.clockClass = strtol(optarg, 0, 0);
      break;

    case 'p':
      rtOpts->priority1 = strtol(optarg, 0, 0);
      break;

    case 'q':

      rtOpts->overrideClockIdentity  = strtol(optarg, 0, 0);
      DBGNPI("WR, port clockIdentity overwritten !!\n");


      break;

   case 'e':
      rtOpts->ethernet_mode = TRUE;
      PERROR("Not implemented yet !");
      return 0;
      break;

   case 'h':
	   rtOpts->E2E_mode = FALSE;
	   break;


   case 'A':
	   DBGNPI("WR AUTO MODE\n");
	   rtOpts->portNumber = WR_PORT_NUMBER;

	   break;

   case 'S':
	   rtOpts->wrNodeMode = WR_SLAVE;

	   DBGNPI("WR Slave\n");
	   break;

   case 'M':
	   rtOpts->wrNodeMode = WR_MASTER;

	   DBGNPI("WR Master\n");
	   break;
    case '1':
      ptpd_wrap_memset(rtOpts->ifaceName[0], 0, IFACE_NAME_LENGTH);
      ptpd_wrap_strncpy(rtOpts->ifaceName[0], optarg, IFACE_NAME_LENGTH);
      rtOpts->portNumber = 1;
      break;

    case '2':
      ptpd_wrap_memset(rtOpts->ifaceName[1], 0, IFACE_NAME_LENGTH);
      ptpd_wrap_strncpy(rtOpts->ifaceName[1], optarg, IFACE_NAME_LENGTH);
      rtOpts->portNumber = 2;
      break;

    case '3':
      ptpd_wrap_memset(rtOpts->ifaceName[2], 0, IFACE_NAME_LENGTH);
      ptpd_wrap_strncpy(rtOpts->ifaceName[2], optarg, IFACE_NAME_LENGTH);
      rtOpts->portNumber  = 3;
      break;


    default:
      *ret = 1;
      return 0;
    }
  }

  ptpClock = (PtpClock*)__calloc(MAX_PORT_NUMBER, sizeof(PtpClock));

  PtpClock * currentPtpdClockData;

  if(!ptpClock)
  {
    PERROR("failed to allocate memory for protocol engine data");
    *ret = 2;
    return 0;
  }
  else
  {
    DBGNPI("allocated %d bytes for protocol engine data\n", (int)sizeof(PtpClock));

    currentPtpdClockData = ptpClock;
    int i;

    for(i = 0; i < MAX_PORT_NUMBER; i++)
    {
	currentPtpdClockData->portIdentity.portNumber = i + 1;
	currentPtpdClockData->foreign = (ForeignMasterRecord*)__calloc(rtOpts->max_foreign_records, sizeof(ForeignMasterRecord));
	if(!currentPtpdClockData->foreign)
	{
	    PERROR("failed to allocate memory for foreign master data");
	    *ret = 2;
	  //TODO:
	      free(ptpClock);
	    return 0;
	}
	else
	{
	    DBGNPI("allocated %d bytes for foreign master data @ port = %d\n",(int)(rtOpts->max_foreign_records*sizeof(ForeignMasterRecord)),currentPtpdClockData->portIdentity.portNumber);
	    /*Init to 0 net buffer*/

	}
	ptpd_wrap_memset(currentPtpdClockData->msgIbuf,0,PACKET_SIZE);
	ptpd_wrap_memset(currentPtpdClockData->msgObuf,0,PACKET_SIZE);

	currentPtpdClockData++;
    }
  }




#ifndef PTPD_NO_DAEMON
  if(!nondaemon)
  {
    if(daemon(0, noclose) == -1)
    {
      PERROR("failed to start as daemon");
      *ret = 3;
      return 0;
    }
    DBGNPI("running as daemon\n");
  }
#endif

  signal(SIGINT, catch_close);
  signal(SIGTERM, catch_close);
  signal(SIGHUP, catch_close);

  *ret = 0;

  return ptpClock;
}
