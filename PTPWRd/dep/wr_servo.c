// this is a simple, PTP-like synchronization test program
// usage: netif_test [-m/-s] interface. (-m = master, -s = slave)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <math.h>

//#include <inttypes.h>  -- now in ptpd-wrappers.h
#include <sys/time.h>

#include "ptpd_netif.h"
#include "hal_exports.h"
#include "ptpd_exports.h"

#include "ptpd.h"

#define WR_SYNC_NSEC 1
#define WR_SYNC_TAI 2
#define WR_SYNC_PHASE 3
#define WR_TRACK_PHASE 4
#define WR_WAIT_SYNC_IDLE 5

#define SERVO_ADJUST_TIMEOUT 2000

/* my own timestamp arithmetic functions */

int servo_state_valid = 0;
ptpdexp_sync_state_t cur_servo_state;

static int tracking_enabled = 1;

void wr_servo_enable_tracking(int enable)
{
	tracking_enabled = enable;
}


static void dump_timestamp(char *what, wr_timestamp_t ts)
{
	fprintf(stderr, "%s = %d:%d:%d\n", what, (int32_t)ts.utc, ts.nsec, ts.phase);
}

static int64_t ts_to_picos(wr_timestamp_t ts)
{
	return (int64_t) ts.utc * (int64_t)1000000000000LL
		+ (int64_t) ts.nsec * (int64_t)1000LL
		+ (int64_t) ts.phase;
}

static wr_timestamp_t picos_to_ts(int64_t picos)
{
	int64_t phase = picos % 1000LL;
	picos = (picos - phase) / 1000LL;

	int64_t nsec = picos % 1000000000LL;
	picos = (picos-nsec)/1000000000LL;

	wr_timestamp_t ts;
	ts.utc = (int64_t) picos;
	ts.nsec = (int32_t) nsec;
	ts.phase = (int32_t) phase;

	return ts;
}

static wr_timestamp_t ts_add(wr_timestamp_t a, wr_timestamp_t b)
{
	wr_timestamp_t c;

	c.utc = 0;
	c.nsec = 0;

	c.phase =a.phase + b.phase;
	if(c.phase >= 1000)
	{
		c.phase -= 1000;
		c.nsec++;
	}

	c.nsec += (a.nsec + b.nsec);

	if(c.nsec >= 1000000000L)
	{
		c.nsec -= 1000000000L;
		c.utc++;
	}

	c.utc += (a.utc + b.utc);

	return c;
}

static wr_timestamp_t ts_sub(wr_timestamp_t a, wr_timestamp_t b)
{
	wr_timestamp_t c;

	c.utc = 0;
	c.nsec = 0;

	c.phase = a.phase - b.phase;

	if(c.phase < 0)
	{
		c.phase+=1000;
		c.nsec--;
	}

	c.nsec += a.nsec - b.nsec;
	if(c.nsec < 0)
	{
		c.nsec += 1000000000L;
		c.utc--;
	}

	c.utc += a.utc - b.utc;

	return c;
}

// "Hardwarizes" the timestamp - e.g. makes the nanosecond field a multiple
// of 8ns cycles and puts the extra nanoseconds in the phase field
static wr_timestamp_t ts_hardwarize(wr_timestamp_t ts)
{
	if(ts.nsec > 0)
	{
		int32_t extra_nsec = ts.nsec % 8;

		if(extra_nsec)
		{
			ts.nsec -=extra_nsec;
			ts.phase += extra_nsec * 1000;
		}
	}
	
/*	if(ts.nsec < 0) {
	 	ts.nsec += 125000000;
	 	ts.utc--;
	}*/


	return ts;
}

static int got_sync = 0;

int wr_servo_init(PtpClock *clock)
{
	hexp_port_state_t pstate;
	wr_servo_state_t *s = &clock->wr_servo;

	PTPD_TRACE(TRACE_SERVO, "Initializing clock servo\n");

	/* Determine the alpha coefficient */
	halexp_get_port_state(&pstate, clock->netPath.ifaceName);

	strncpy(s->if_name, clock->netPath.ifaceName, 16);

	s->state = WR_SYNC_TAI;
	s->cur_setpoint = 0;
	s->fiber_fix_alpha = pstate.fiber_fix_alpha;

	// fixme: full precision
	s->delta_tx_m = ((int32_t)clock->grandmasterDeltaTx.scaledPicoseconds.lsb) >> 16;
	s->delta_rx_m = ((int32_t)clock->grandmasterDeltaRx.scaledPicoseconds.lsb) >> 16;
	s->delta_tx_s = ((((int32_t)clock->deltaTx.scaledPicoseconds.lsb) >> 16) & 0xffff) | (((int32_t)clock->deltaTx.scaledPicoseconds.msb) << 16);
	s->delta_rx_s = ((((int32_t)clock->deltaRx.scaledPicoseconds.lsb) >> 16) & 0xffff) | (((int32_t)clock->deltaRx.scaledPicoseconds.msb) << 16);


	cur_servo_state.delta_tx_m = (int64_t)s->delta_tx_m;
	cur_servo_state.delta_rx_m = (int64_t)s->delta_rx_m;
	cur_servo_state.delta_tx_s = (int64_t)s->delta_tx_s;
	cur_servo_state.delta_rx_s = (int64_t)s->delta_rx_s;
	
	PTPD_TRACE(TRACE_SERVO, "Deltas [ps]: TxM = %d RxM = %d TxS = %d RxS = %d\n", 
	       s->delta_tx_m,
	       s->delta_rx_m,
	       s->delta_tx_s,
	       s->delta_rx_s);


	strncpy(cur_servo_state.sync_source,
			  clock->netPath.ifaceName, 16);//fixme
	strncpy(cur_servo_state.slave_servo_state,
			  "Uninitialized", 32);

	servo_state_valid = 1;
	cur_servo_state.valid = 1;
	got_sync = 0;
	return 0;
}

wr_timestamp_t timeint_to_wr(TimeInternal t)
{
	wr_timestamp_t ts;
	ts.utc = t.seconds;
	ts.nsec = t.nanoseconds;
	ts.phase = t.phase;
	return ts;
}

static int ph_adjust = 0;

int wr_servo_man_adjust_phase(int phase)
{
	ph_adjust = phase;
	return phase;
}


int wr_servo_got_sync(PtpClock *clock, TimeInternal t1, TimeInternal t2)
{
	wr_servo_state_t *s = &clock->wr_servo;

	s->t1 = timeint_to_wr(t1);
	s->t2 = timeint_to_wr(t2);

/*	PTPD_TRACE(TRACE_SERVO, "got_sync\m");
		dump_timestamp("sync->t1", s->t1);
		dump_timestamp("sync->t2", s->t2);*/

	got_sync = 1;

	return 0;
}

int wr_servo_got_delay(PtpClock *clock, Integer32 cf)
{
	wr_servo_state_t *s = &clock->wr_servo;

	s->t3 = clock->delayReq_tx_ts;
	//  s->t3.phase = 0;
	s->t4 = timeint_to_wr(clock->delay_req_receive_time);
	s->t4.phase = (int64_t) cf * 1000LL / 65536LL;
	return 0;
}

int wr_servo_update(PtpClock *clock)
{
	wr_servo_state_t *s = &clock->wr_servo;

	uint64_t tics;
	uint64_t big_delta_fix;
	uint64_t delay_ms_fix;

	wr_timestamp_t ts_offset, ts_offset_hw /*, ts_phase_adjust */;
	hexp_pps_params_t adjust;

	if(!got_sync)
		return 0;
		
	got_sync = 0;

	if (0) { /* enable for debugging */
		dump_timestamp("->t1", s->t1);
		dump_timestamp("->t2", s->t2);
		dump_timestamp("->t3", s->t3);
		dump_timestamp("->t4", s->t4);
		dump_timestamp("->mdelay", s->mu);
	}

	s->mu = ts_sub(ts_sub(s->t4, s->t1), ts_sub(s->t3, s->t2));

	big_delta_fix =  s->delta_tx_m + s->delta_tx_s
		       + s->delta_rx_m + s->delta_rx_s;

	delay_ms_fix = (((uint64_t)(ts_to_picos(s->mu) - big_delta_fix) * (uint64_t) s->fiber_fix_alpha) >> FIX_ALPHA_FRACBITS) 
		+ ((ts_to_picos(s->mu) - big_delta_fix) >> 1) 
		+ s->delta_tx_m + s->delta_rx_s + ph_adjust;

	PTPD_TRACE(TRACE_SERVO, "delay_ms [ps] = %d\n", (int32_t) delay_ms_fix);
	PTPD_TRACE(TRACE_SERVO, "mu = %d mu - deltas  = %d\n", (int32_t) ts_to_picos(s->mu), (int32_t)ts_to_picos(s->mu) - (int32_t)big_delta_fix);

	ts_offset = ts_add(ts_sub(s->t1, s->t2), picos_to_ts(delay_ms_fix));
	ts_offset_hw = ts_hardwarize(ts_offset);

	cur_servo_state.mu = (uint64_t)ts_to_picos(s->mu);
	cur_servo_state.cur_offset = ts_to_picos(ts_offset);

	PTPD_TRACE(TRACE_SERVO, "offset [ps] = %lld\n", cur_servo_state.cur_offset);

	cur_servo_state.delay_ms = delay_ms_fix;
	cur_servo_state.total_asymmetry =
		(cur_servo_state.mu - 2LL * (int64_t)delay_ms_fix);
	cur_servo_state.fiber_asymmetry =
		cur_servo_state.total_asymmetry
		- (s->delta_tx_m + s->delta_rx_s)
		+ (s->delta_rx_m + s->delta_tx_s);

	cur_servo_state.tracking_enabled = tracking_enabled;

	s->delta_ms = delay_ms_fix;

	tics = ptpd_netif_get_msec_tics();

	switch(s->state)
	{
	case WR_WAIT_SYNC_IDLE:
		strcpy(adjust.port_name, s->if_name);

		if(!halexp_pps_cmd(HEXP_PPSG_CMD_POLL, &adjust)
		   && (tics - s->last_tics) > SERVO_ADJUST_TIMEOUT)
			s->state = s->next_state;
		break;

	case WR_SYNC_TAI:
		if(ts_offset_hw.utc != 0)
		{

			strcpy(cur_servo_state.slave_servo_state, "SYNC_UTC");
			strcpy(adjust.port_name, s->if_name);
			adjust.adjust_utc = ts_offset_hw.utc;

			PTPD_TRACE(TRACE_SERVO, "Adjusting UTC counter: %d seconds", (int32_t) ts_offset_hw.utc);

			halexp_pps_cmd(HEXP_PPSG_CMD_ADJUST_UTC, &adjust);
			s->next_state = WR_SYNC_NSEC;

			s->state = WR_WAIT_SYNC_IDLE;
			s->last_tics = tics;

		} else s->state = WR_SYNC_NSEC;
		break;

	case WR_SYNC_NSEC:
		strcpy(cur_servo_state.slave_servo_state, "SYNC_NSEC");

		if(ts_offset_hw.nsec != 0)
		{

			strcpy(adjust.port_name, s->if_name);
			adjust.adjust_nsec = ts_offset_hw.nsec;

			PTPD_TRACE(TRACE_SERVO, "Adjusting NSEC counter: %d nanoseconds", (int32_t) ts_offset_hw.nsec);
			
			halexp_pps_cmd(HEXP_PPSG_CMD_ADJUST_NSEC, &adjust);
			s->next_state = WR_SYNC_PHASE;
			s->state = WR_WAIT_SYNC_IDLE;
			s->last_tics = tics;

		} else s->state = WR_SYNC_PHASE;
		break;

	case WR_SYNC_PHASE:
		strcpy(cur_servo_state.slave_servo_state, "SYNC_PHASE");
		s->cur_setpoint = ts_offset_hw.phase;

		PTPD_TRACE(TRACE_SERVO, "Adjusting clock phase: %d picoseconds", (int32_t) ts_offset_hw.phase);

		strcpy(adjust.port_name, s->if_name);
		adjust.adjust_phase_shift = s->cur_setpoint;
		halexp_pps_cmd(HEXP_PPSG_CMD_ADJUST_PHASE, &adjust);

		s->next_state = WR_TRACK_PHASE;
		s->state = WR_WAIT_SYNC_IDLE;
		s->last_tics = tics;
		s->delta_ms_prev = s->delta_ms;

		break;


	case WR_TRACK_PHASE:
		strcpy(cur_servo_state.slave_servo_state, "TRACK_PHASE");
		cur_servo_state.cur_setpoint = s->cur_setpoint;
		cur_servo_state.cur_skew = s->delta_ms - s->delta_ms_prev;

		if(tracking_enabled)
		{

			// just follow the changes of deltaMS
			s->cur_setpoint += (s->delta_ms - s->delta_ms_prev);

			strcpy(adjust.port_name, s->if_name);
			adjust.adjust_phase_shift = s->cur_setpoint;
			halexp_pps_cmd(HEXP_PPSG_CMD_ADJUST_PHASE, &adjust);

			s->delta_ms_prev = s->delta_ms;
			s->next_state = WR_TRACK_PHASE;
			s->state = WR_WAIT_SYNC_IDLE;
			s->last_tics = tics;

		}

		break;

	}
	return 0;
}


