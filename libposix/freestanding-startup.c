
/* startup.c, for freestanding application */
#include <ptpd.h>

static PtpPortDS ptpPortDS[MAX_PORT_NUMBER];
static ForeignMasterRecord foreign[MAX_PORT_NUMBER][DEFAULT_MAX_FOREIGN_RECORDS];


PtpPortDS * ptpdStartup(int argc, char **argv, Integer16 *ret,
		       RunTimeOpts *rtOpts,PtpClockDS *ptpClockDS)
{
	PtpPortDS * currentPtpdClockData;
	int i;

	/* When running freestanding, configuration is compiled-in
	 * Status of the various options:
	 *
	 * KILLED: -c       run in command line (non-daemon) mode
	 * KILLED: -f FILE  send output to FILE
	 * KILLED: -d       display stats
	 * KILLED: -d       display stats in .csv format
	 * KILLED: -x       do not reset the clock if off by more than 1s
	 * KILLED: -t       do not adjust the system clock
	 * KILLED: -a N,N   specify clock servo P and I attenuations
	 * KILLED: -w N     specify one way delay filter stiffness
	 * KILLED: -u ADDR  also send uni-cast to ADDR
	 * KILLED: -e       run in ethernet mode (level2) (REALLY?)
	 * KILLED: -h       in peer-to-peer
	 * KILLED: -l N,N   specify inbound, outbound latency in nsec
	 * KILLED: -o N     specify current UTC offset
	 * KILLED: -i N     specify PTP domain number
	 * KILLED: -n N     specify announce interval in 2^N sec
	 * KILLED: -y N     specify sync interval in 2^N sec
	 * KILLED: -m N     specify max number of foreign master records
	 * KILLED: -v N     specify system clock allen variance
	 * KILLED: -r N     specify system clock accuracy
	 * KILLED: -s N     specify system clock class
	 * KILLED: -p N     specify priority1 attribute
	 * KILLED: -A       WR: hands free - autodetection of ports ...
	 * KILLED: -M       WR: run PTP node as WR Master
	 * KILLED: -S       WR: run PTP node as WR Slave
	 * KILLED: -q N     WR: N > 1, different on each test machine
	 * KILLED: -1 NAME  WR: network interface for port 1
	 * KILLED: -2 NAME  WR: network interface for port 2
	 * KILLED: -3 NAME  WR: network interface for port 3
	 *
	 * This is preserved, but the value is constant here
	 *
	 *         -b NAME           bind PTP to network interface NAME
	 *         -g                run as slave only
	 */
	strcpy(rtOpts->ifaceName[0], "wru1");

	currentPtpdClockData = ptpPortDS;
	for(i = 0; i < MAX_PORT_NUMBER; i++) {
		currentPtpdClockData->portIdentity.portNumber = i + 1;
		currentPtpdClockData->foreign = foreign[i];
		/* This memset is repeated for no reason. Bah! */
		memset(currentPtpdClockData->msgIbuf,0,PACKET_SIZE);
		memset(currentPtpdClockData->msgObuf,0,PACKET_SIZE);
		currentPtpdClockData->ptpClockDS = ptpClockDS; // common data
		currentPtpdClockData++;
	}

	*ret = 0;
	return ptpPortDS;
}

void ptpdShutdown(void)
{}
