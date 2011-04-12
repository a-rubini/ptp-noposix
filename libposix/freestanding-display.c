#include <ptpd.h>

void timeInternal_display(TimeInternal *timeInternal) {
	TRACE_DISP("seconds : %d \n",timeInternal->seconds);
	TRACE_DISP("nanoseconds %d \n",timeInternal->nanoseconds);
}
