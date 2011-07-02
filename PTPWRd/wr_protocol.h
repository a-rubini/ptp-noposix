#ifndef WR_PROTOCOL_H_
#define WR_PROTOCOL_H_


    /*
      handle actions and events for 'wrPortState'
    */
    void doWRState(RunTimeOpts *rtOpts, PtpPortDS *ptpPortDS);

    /*
      perform actions required when leaving 'wrPortState' and entering 'state'
    */
    void toWRState(UInteger8 state, RunTimeOpts *rtOpts, PtpPortDS *ptpPortDS);


/*
returns TRUE if the port is UP (cable connected)
@return
    TRUE  	- link up,
    FALSE	- cable disconnected
*/
Boolean isPortUp(NetPath *netPath);


/*
  this function manages WR timeouts, each main state (except IDLE) has timeout
  timeouts are "automatically" started/stopped on the transition of main states
  by this function
  here we also count the number of attempt on the same state, if the retry number
  exccedded, we exit WR FSM, no WRPTP, sorry
*/
void wrTimetoutManage(UInteger8,UInteger8, RunTimeOpts *rtOpts,
		      PtpPortDS *ptpPortDS);

/*
  this function checks if wr timer has expired for a current WR state
*/
void wrTimerExpired(UInteger8 currentState, RunTimeOpts *rtOpts,
		    PtpPortDS *ptpPortDS, Enumeration8 wrMode);

/*
  this function comes as a consequence of implementing substates.
  it returns the main state currently being executed
*/
UInteger8 returnCurrentWRMainState(PtpPortDS*);

/*
A function a value in a bit form
 
return:
 value of "bits" in a bit form in a "string"
 */
char *printf_bits(UInteger32 bits);

/*
A function to display WR timestamps
 
return:
 value of wr timestamp as a "string"
 */
char *format_wr_timestamp(wr_timestamp_t ts);


/*
  It initializes White Rabbit dynamic data fields as 
  defined in the WRSPEC, talbe 1
*/
void initWrData(PtpPortDS *ptpPortDS, Enumeration8 mode);

#endif /*WR_PROTOCOL_H_*/
