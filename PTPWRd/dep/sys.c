/* sys.c */

#include "../ptpd.h"

void displayStats(RunTimeOpts *rtOpts, PtpPortDS *ptpPortDS)
{
#ifndef WRPC_EXTRA_SLIM
  static int start = 1;
  static char sbuf[SCREEN_BUFSZ];
  char *s;
  int len = 0;

  if(start && rtOpts->csvStats)
  {
    start = 0;
    PTPD_TRACE(TRACE_MSG, NULL, "state, one way delay, offset from master, drift");
    fflush(stdout);
  }

  memset(sbuf, ' ', SCREEN_BUFSZ);

  switch(ptpPortDS->portState)
  {
  case PTP_INITIALIZING:  s = "init";  break;
  case PTP_FAULTY:        s = "flt";   break;
  case PTP_LISTENING:     s = "lstn";  break;
  case PTP_PASSIVE:       s = "pass";  break;
  case PTP_UNCALIBRATED:  s = "uncl";  break;
  case PTP_SLAVE:         s = "slv";   break;
  case PTP_PRE_MASTER:    s = "pmst";  break;
  case PTP_MASTER:        s = "mst";   break;
  case PTP_DISABLED:      s = "dsbl";  break;
  default:                s = "?";     break;
  }

  len += sprintf(sbuf + len, "%s%s", rtOpts->csvStats ? "\n": "\rstate: ", s);

  if(ptpPortDS->portState == PTP_SLAVE)
  {
    len += sprintf(sbuf + len,
      ", %s%d.%09d" ", %s%d.%09d",
      rtOpts->csvStats ? "" : "owd: ",
      ptpPortDS->ptpClockDS->meanPathDelay.seconds,
      ptpPortDS->ptpClockDS->meanPathDelay.nanoseconds,
      //abs(ptpPortDS->meanPathDelay.nanoseconds),
      rtOpts->csvStats ? "" : "ofm: ",
      ptpPortDS->ptpClockDS->offsetFromMaster.seconds,
      ptpPortDS->ptpClockDS->offsetFromMaster.nanoseconds);
      //abs(ptpPortDS->offsetFromMaster.nanoseconds));

    len += sprintf(sbuf + len,
      ", %s%d" ,
      rtOpts->csvStats ? "" : "drift: ", ptpPortDS->observed_drift);
  }

  write(1, sbuf, rtOpts->csvStats ? len : SCREEN_MAXSZ + 1);
#endif
}
