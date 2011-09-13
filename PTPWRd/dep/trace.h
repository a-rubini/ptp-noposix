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
#define TRACE_WRPC (1<<13)

#define TRACE_ALL 0xffff

extern void wrc_debug_printf(int subsys, const char *fmt, ...);

#ifdef PTPD_FREESTANDING
	#define _PTPD_DPRINTF(subsys,...) wrc_debug_printf(subsys, __VA_ARGS__)
#else
	#define _PTPD_DPRINTF(subsys,...) fprintf(stderr,__VA_ARGS__)
#endif


#define PTPD_TRACE(subsys, p, x, ...) \
  {\
    PtpPortDS *port = (PtpPortDS *)p;\
    if(PTPD_TRACE_MASK & subsys)\
    {\
      if(p) \
      {\
	_PTPD_DPRINTF(subsys,"([p=%d] %s WR: %s%s%s) " x ,port->portIdentity.portNumber,\
	(port->portState==PTP_SLAVE ? "ptp[S]" : (port->portState==PTP_MASTER ? "ptp[M]" : "ptp[-]")),\
						    (port->wrModeON== TRUE ? "ON->" : "OFF "),\
						    (port->wrMode== WR_MASTER ? "MASTER" : ""),\
	(port->wrMode== WR_SLAVE ? (port->wrSlaveRole == PRIMARY_SLAVE ? "p-SLAVE" : "s-SLAVE") : ""), \
						      ##__VA_ARGS__); \
      } else \
      {\
	_PTPD_DPRINTF(subsys,x, ## __VA_ARGS__); \
      }\
    }\
  }\



#define PTPD_TRACE_NOPTPDATADS(subsys, x, ...) \
  {\
      if(~PTPD_TRACE_PTPDATADS & subsys) \
      {\
	_PTPD_DPRINTF(subsys, x,## __VA_ARGS__); \
      }\
  }
  
#endif
