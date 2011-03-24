/* timer.c */

#include "../ptpd.h"

#define TIMER_INTERVAL 1000
unsigned int elapsed[MAX_PORT_NUMBER];
char *timerNames[] = {"PDELAYREQ","DELAYREQ","SYNC","ANNOUNCE","ANNOUNCE_RECEIPT","ANNOUNCE_INTERVAL"};

void do_irq_less_timing(PtpClock *ptpClock)
{

  struct timeval tv;
  time_t        sec_diff;
  suseconds_t  usec_diff;
  UInteger16 portN = ptpClock->portIdentity.portNumber - 1;

  gettimeofday(&tv, 0);

  sec_diff = tv.tv_sec  - ptpClock->last_update.tv_sec;


  if(tv.tv_usec >= ptpClock->last_update.tv_usec)
  {
     usec_diff = tv.tv_usec - ptpClock->last_update.tv_usec;
  }
  else
  {
     usec_diff = 1000000 + tv.tv_usec - ptpClock->last_update.tv_usec;
     sec_diff--;
  }

  if((usec_diff > TIMER_INTERVAL) | (sec_diff > 0))
  {

    elapsed[portN] = elapsed[portN] + (sec_diff * 1000 + usec_diff/1000); //[ms]

    ptpClock->last_update.tv_sec  = tv.tv_sec;
    ptpClock->last_update.tv_usec = tv.tv_usec;
  }
  //DBG("timing[eclapsed = %d]: sec_diff = %lld, usec_diff =  %lld \n",elapsed, (unsigned long long)sec_diff,(unsigned long long)usec_diff);

}


void catch_alarm(int sig)
{
  //elapsed++;
  DBGV("catch_alarm: elapsed %d\n", elapsed[0]);
  //DBG("catch_alarm: elapsed %d\n", elapsed);
}

void initTimer(PtpClock *ptpClock)
{
  DBG("initTimer\n");
  elapsed[ptpClock->portIdentity.portNumber - 1] = 0;

#ifdef IRQ_LESS_TIMER

  gettimeofday(&ptpClock->last_update, 0);

#else
  struct itimerval itimer;

  signal(SIGALRM, SIG_IGN);

  itimer.it_value.tv_sec = itimer.it_interval.tv_sec = 0;
  itimer.it_value.tv_usec = itimer.it_interval.tv_usec = TIMER_INTERVAL;


  signal(SIGALRM, catch_alarm);
  setitimer(ITIMER_REAL, &itimer, 0);
#endif

}

void timerUpdate(IntervalTimer *itimer,UInteger16 portN)
{
  //DBG("WR: %s, eclapsed: %d\n",__func__,elapsed);
  int i, delta;

  portN = portN - 1;

  delta = elapsed[portN];
  elapsed[portN] = 0;

  if(delta <= 0)
    return;

  for(i = 0; i < TIMER_ARRAY_SIZE; ++i)
  {

    if((itimer[i].interval) > 0 && ((itimer[i].left) -= delta) <= 0)
    {
      itimer[i].left = itimer[i].interval;
      itimer[i].expire = TRUE;
      DBGV("timerUpdate: timer %s expired\n", timerNames[i]);
    }
  }

}

void timerStop(UInteger16 index, IntervalTimer *itimer)
{
  if(index >= TIMER_ARRAY_SIZE)
    return;

  itimer[index].interval = 0;
}

void timerStart(UInteger16 index, float interval, IntervalTimer *itimer)
{
  if(index >= TIMER_ARRAY_SIZE)
    return;

  itimer[index].expire = FALSE;
  itimer[index].left = interval*1000; //Factor 1000 used because resolution time is ms for the variable "elasped"

  itimer[index].interval = itimer[index].left;

  DBGNPI("timerStart: set timer %s to %f\n", timerNames[index], interval);

}

Boolean timerExpired(UInteger16 index, IntervalTimer *itimer, UInteger16 portN)
{
  timerUpdate(itimer,portN);

  if(index >= TIMER_ARRAY_SIZE)
    return FALSE;

  if(!itimer[index].expire)
    return FALSE;

  itimer[index].expire = FALSE;

  return TRUE;
}
