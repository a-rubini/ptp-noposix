#ifndef WR_PROTOCOL_H_
#define WR_PROTOCOL_H_

#ifndef NEW_SINGLE_WRFSM
    /*
      handle actions and events for 'wrPortState'
    */
    void doWRSlaveState(RunTimeOpts *rtOpts, PtpClock *ptpClock);

    /*
      perform actions required when leaving 'wrPortState' and entering 'state'
    */
    void toWRSlaveState(UInteger8 state, RunTimeOpts *rtOpts, PtpClock *ptpClock);

    /*
      handle actions and events for 'wrPortState'
    */
    void doWRMasterState(RunTimeOpts *rtOpts, PtpClock *ptpClock);

    /*
      perform actions required when leaving 'wrPortState' and entering 'state'
    */
    void toWRMasterState(UInteger8 state, RunTimeOpts *rtOpts, PtpClock *ptpClock);

#else
    /*
      handle actions and events for 'wrPortState'
    */
    void doWRState(RunTimeOpts *rtOpts, PtpClock *ptpClock);

    /*
      perform actions required when leaving 'wrPortState' and entering 'state'
    */
    void toWRState(UInteger8 state, RunTimeOpts *rtOpts, PtpClock *ptpClock);

#endif

/*
returns TRUE if the port is UP (cable connected)
@return
    TRUE  	- link up,
    FALSE	- cable disconnected
*/
Boolean isPortUp(NetPath *netPath);

/*
polls HW for TX timestamp
@return
    TRUE  	- if timestamp read,
    FALSE	- otherwise
*/
Boolean getWRtxTimestamp(RunTimeOpts *rtOpts, PtpClock *ptpClock);

/*
  this function manages WR timeouts, each main state (except IDLE) has timeout
  timeouts are "automatically" started/stopped on the transition of main states
  by this function
  here we also count the number of attempt on the same state, if the retry number
  exccedded, we exit WR FSM, no WRPTP, sorry
*/
void wrTimetoutManage(UInteger8,UInteger8, RunTimeOpts *rtOpts,
		      PtpClock *ptpClock);

/*
  this function checks if wr timer has expired for a current WR state
*/
void wrTimerExpired(UInteger8 currentState, RunTimeOpts *rtOpts,
		    PtpClock *ptpClock, Enumeration8 wrMode);

/*
  this function comes as a consequence of implementing substates.
  it returns the main state currently being executed
*/
UInteger8 returnCurrentWRMainState(PtpClock*);

/*
Function tries to read fixed delays (if PTPWRd restarted, they are remembered by HW
if delays not known, Tx fixed delays are measured

we wait here as long as it takes to calibrate Tx !!!!!!

return:
  TRUE 	- calibration OK
  FALSE - sth wrong

*/
Boolean initWRcalibration(const char *ifaceName,PtpClock *ptpClock );

char *printf_bits(UInteger32 bits);

char *format_wr_timestamp(wr_timestamp_t ts);


#endif /*WR_PROTOCOL_H_*/
