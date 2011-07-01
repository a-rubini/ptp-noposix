#ifndef __PTPD_TRACE_H
#define __PTPD_TRACE_H

#ifndef PTPD_TRACE_MASK
	#define PTPD_TRACE_MASK 0xffff
#endif

#define TRACE_PROTO (1<<0)
#define TRACE_WR_PROTO (1<<1)
#define TRACE_NET (1<<2)
#define TRACE_MSG (1<<3)
#define TRACE_SYS (1<<4)
#define TRACE_SERVO (1<<5)
#define TRACE_BMC (1<<6)
#define TRACE_ERROR (1<<7)
#define TRACE_WR_IPC (1<<8)
#define TRACE_STARTUP (1<<9)
#define TRACE_ARITH (1<<10)
#define TRACE_PTPD_MAIN (1<<11)
#define TRACE_SPECIAL_DBG (1<<12)

#define TRACE_ALL 0xffff


#define PTPD_TRACE(subsys, p, x, ...) \
  {\
    PtpPortDS *port = (PtpPortDS *)p;\
    if(PTPD_TRACE_MASK & subsys)\
    {\
      if(p) \
      {\
	fprintf(stderr, "([p=%d] %s WR->%s%s%s) " x ,port->portIdentity.portNumber,\
	(port->portState==PTP_SLAVE ? "ptp[S]" : (port->portState==PTP_MASTER ? "ptp[M]" : "ptp[-]")),\
						    (port->wrModeON== TRUE ? "ON->" : "OFF "),\
						    (port->wrMode== WR_MASTER ? "MASTER" : ""),\
						    (port->wrMode== WR_SLAVE ? "SLAVE" : ""), \
						      ##__VA_ARGS__); \
      } else \
      {\
	fprintf(stderr, x, ## __VA_ARGS__); \
      }\
    }\
  }\



#define PTPD_TRACE_NOPTPDATADS(subsys, x, ...) \
  {\
      if(~PTPD_TRACE_PTPDATADS & subsys) \
      {\
	fprintf(stderr, x,## __VA_ARGS__); \
      }\
  }

#endif
