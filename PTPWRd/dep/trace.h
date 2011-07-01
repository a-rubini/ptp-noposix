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

#define TRACE_ALL 0xffff

#define PTPD_TRACE_PTPDATADS (TRACE_WR_PROTO | TRACE_PROTO | TRACE_BMC | TRACE_NET)

#define PTPD_TRACE(subsys, x, ...) \
  {\
    if(PTPD_TRACE_MASK & subsys) \
    {\
      if(PTPD_TRACE_PTPDATADS & subsys) \
      {\
	fprintf(stderr, "([p=%d] wrMode: %s%s%s) " x ,ptpPortDS->portIdentity.portNumber,\
						    (ptpPortDS->wrModeON== TRUE ? "ON->" : "OFF "),\
						    (ptpPortDS->wrMode== WR_MASTER ? "MASTER" : ""),\
						    (ptpPortDS->wrMode== WR_SLAVE ? "SLAVE" : ""), \
						      ##__VA_ARGS__); \
      }\
    }\
  }


#define PTPD_TRACE_NOPTPDATADS(subsys, x, ...) \
  {\
    if(PTPD_TRACE_MASK & subsys) \
    {\
      if(~PTPD_TRACE_PTPDATADS & subsys) \
      {\
	fprintf(stderr, x,## __VA_ARGS__); \
      }\
    }\
  }

#endif
