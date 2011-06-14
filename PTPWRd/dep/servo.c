#include "../ptpd.h"

void initClock(RunTimeOpts *rtOpts, PtpPortDS *ptpPortDS)
{


}

void updateDelay (one_way_delay_filter *owd_filt, RunTimeOpts *rtOpts, PtpPortDS *ptpPortDS,TimeInternal *correctionField)
{
}


void updatePeerDelay (one_way_delay_filter *owd_filt, RunTimeOpts *rtOpts, PtpPortDS *ptpPortDS,TimeInternal *correctionField,Boolean twoStep)
{

}

void updateOffset(TimeInternal *send_time, TimeInternal *recv_time,
  offset_from_master_filter *ofm_filt, RunTimeOpts *rtOpts, PtpPortDS *ptpPortDS,TimeInternal *correctionField)
{



}

void updateClock(RunTimeOpts *rtOpts, PtpPortDS *ptpPortDS)
{


}
