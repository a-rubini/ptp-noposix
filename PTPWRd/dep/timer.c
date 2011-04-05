/* timer.c */

#include "../ptpd.h"

void timerInit(IntervalTimer *itimer, const char *name)
{
#ifndef WRPC_EXTRA_SLIM
  strncpy(itimer -> name, name, sizeof(itimer->name));
#endif

  itimer->interval = 0;
}

/* interval used to be seconds as float value, it is now milliseconds */
void timerStart(IntervalTimer *itimer, int interval)
{
  itimer->t_start = ptpd_netif_get_msec_tics();
  itimer->interval = interval;
}

void timerStop(IntervalTimer *itimer)
{
  itimer->interval = 0;
}

Boolean timerExpired(IntervalTimer *itimer)
{
  uint64_t tics, t_expire;

  tics = ptpd_netif_get_msec_tics();
  t_expire = itimer->t_start + (uint64_t) itimer->interval;

  if(itimer->interval != 0 && tics >= t_expire)
    {
     
     itimer->t_start += (uint64_t) itimer->interval;

     if(itimer->t_start < tics)
       itimer->t_start = tics;

     return TRUE;
    }
 
  return FALSE;
}
