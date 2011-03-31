#include <ptpd.h>

void timeInternal_display(TimeInternal *timeInternal) {
	printf("seconds : %d \n",timeInternal->seconds);
	printf("nanoseconds %d \n",timeInternal->nanoseconds);
}
