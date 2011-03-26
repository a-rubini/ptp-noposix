// this is a simple, PTP-like synchronization test program
// usage: netif_test [-m/-s] interface. (-m = master, -s = slave)

#include <stdio.h>
#include <stdlib.h>
//#include <string.h>

#include <math.h>

#include <inttypes.h>
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

// my own timestamp arithmetic function

int servo_state_valid = 0;
ptpdexp_sync_state_t cur_servo_state;

static int tracking_enabled = 1;

void wr_servo_enable_tracking(int enable)
{
	tracking_enabled = enable;
}

static uint64_t get_tics()
{
	struct timezone tz ={0,0};
	struct timeval tv;
	gettimeofday(&tv, &tz);
	return(uint64_t) tv.tv_sec *1000000ULL + (uint64_t) tv.tv_usec;
}

static void dump_timestamp(char *what, wr_timestamp_t ts)
{
	fprintf(stderr, "%s = %lld:%d:%d\n", what, ts.utc, ts.nsec, ts.phase);
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


#if 0 /* not used */
static wr_timestamp_t ts_div2(wr_timestamp_t a)
{
	if(a.utc % 1LL)
	{
		a.nsec += 500000000L;
	}
	a.utc /= 2;

	if(a.nsec % 1L)
	{
		a.phase += 500;
	}

	a.nsec /= 2;
	a.phase /= 2;

	return a;
}
#endif

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

	return ts;
}

#if 0 /* not used */
static wr_timestamp_t ts_zero()
{
	wr_timestamp_t a;
	a.utc = 0;
	a.nsec = 0;
	a.phase = 0;
	return a;
}
#endif

int wr_servo_init(PtpClock *clock)
{
	wr_servo_state_t *s = &clock->wr_servo;

	fprintf(stderr,"[slave] initializing clock servo\n");

	strncpy(s->if_name, clock->netPath.ifaceName, 16);

	s->state = WR_SYNC_TAI;
	s->cur_setpoint = 0;

	// fixme: full precision
	s->delta_tx_m = ((int32_t)clock->grandmasterDeltaTx
			 .scaledPicoseconds.lsb) >> 16;
	s->delta_rx_m = ((int32_t)clock->grandmasterDeltaRx
			 .scaledPicoseconds.lsb) >> 16;
	s->delta_tx_s = ((int32_t)clock->deltaTx.scaledPicoseconds.lsb) >> 16;
	s->delta_rx_s = ((int32_t)clock->deltaRx.scaledPicoseconds.lsb) >> 16;

	cur_servo_state.delta_tx_m = (double)s->delta_tx_m;
	cur_servo_state.delta_rx_m = (double)s->delta_rx_m;
	cur_servo_state.delta_tx_s = (double)s->delta_tx_s;
	cur_servo_state.delta_rx_s = (double)s->delta_rx_s;

	strncpy(cur_servo_state.sync_source,
			  clock->netPath.ifaceName, 16);//fixme
	strncpy(cur_servo_state.slave_servo_state,
			  "Uninitialized", 32);
	servo_state_valid = 1;
	cur_servo_state.valid = 1;
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
	//  s->t1.phase = 0;
	s->t2 = timeint_to_wr(t2);
	return 0;
}

int wr_servo_got_delay(PtpClock *clock, Integer32 cf)
{
	wr_servo_state_t *s = &clock->wr_servo;

	s->t3 = clock->delayReq_tx_ts;
	//  s->t3.phase = 0;
	s->t4 = timeint_to_wr(clock->delay_req_receive_time);
	s->t4.phase = (Integer32) ((double)cf / 65536.0 * 1000.0);
	if (0) { /* enable for debugging */
		dump_timestamp("T3", s->t3);
		dump_timestamp("T4", s->t4);
	}
	return 0;
}

int wr_servo_update(PtpClock *clock)
{
	wr_servo_state_t *s = &clock->wr_servo;

	double big_delta, alpha /*, mu, asymmetry */;
	double delay_ms;
	wr_timestamp_t ts_offset, ts_offset_hw /*, ts_phase_adjust */;
	hexp_pps_params_t adjust;

	if (0) { /* enable for debugging */
		dump_timestamp("t1", s->t1);
		dump_timestamp("t2", s->t2);
		dump_timestamp("t3", s->t3);
		dump_timestamp("t4", s->t4);
	}

	s->mu = ts_sub(ts_sub(s->t4, s->t1), ts_sub(s->t3, s->t2));

	if (0) { /* enable for debugging */
		dump_timestamp("mdelay", s->mu);
	}

	alpha = 1.4682e-04*1.76; // EXPERIMENTALLY DERIVED. VALID.

	printf("delta_TX_M = %d\n", s->delta_tx_m);
	printf("delta_TX_S = %d\n", s->delta_tx_s);
	printf("delta_RX_M = %d\n", s->delta_rx_m);
	printf("delta_RX_S = %d\n", s->delta_rx_s);

	big_delta = (double) s->delta_tx_m + (double) s->delta_tx_s
		+ (double) s->delta_rx_m + (double) s->delta_rx_s;

	cur_servo_state.mu = (double)ts_to_picos(s->mu);

	// fiber part (first line) + PHY/routing part (second line)
	delay_ms = ((double)ts_to_picos(s->mu) - big_delta) / (2.0 + alpha)
		+ (double)s->delta_tx_m + (double) s->delta_rx_s + ph_adjust;

	//  printf("delay_ms = %.0f\n", delay_ms);
	//  printf("mu = %lld\n", ts_to_picos(s->mu));

	ts_offset = ts_add(ts_sub(s->t1, s->t2),
			   picos_to_ts((int64_t)delay_ms));
	ts_offset_hw = ts_hardwarize(ts_offset);

	cur_servo_state.cur_offset = ts_to_picos(ts_offset);

	cur_servo_state.delay_ms = delay_ms;
	cur_servo_state.total_asymmetry =
		(cur_servo_state.mu - 2.0 * delay_ms);
	cur_servo_state.fiber_asymmetry =
		cur_servo_state.total_asymmetry
		- (s->delta_tx_m + s->delta_rx_s)
		+ (s->delta_rx_m + s->delta_tx_s);

	if (0) { /* enable for debugging */
		dump_timestamp("offset", ts_offset_hw);
	}

	//printf("state %d\n", s->state);

	s->delta_ms = delay_ms;

	cur_servo_state.tracking_enabled = tracking_enabled;

	switch(s->state)
	{

	case WR_WAIT_SYNC_IDLE:
		strcpy(adjust.port_name, s->if_name);

		if(!halexp_pps_cmd(HEXP_PPSG_CMD_POLL, &adjust)
		   && (get_tics() - s->last_tics) > 2000000ULL)
			s->state = s->next_state;
		break;

	case WR_SYNC_TAI:
		if(ts_offset_hw.utc != 0)
		{
			strcpy(cur_servo_state.slave_servo_state, "SYNC_UTC");

			strcpy(adjust.port_name, s->if_name);
			adjust.adjust_utc = ts_offset_hw.utc;

			//  fprintf(stderr,"[slave] Adjusting UTC counter\n");

			halexp_pps_cmd(HEXP_PPSG_CMD_ADJUST_UTC, &adjust);
			s->next_state = WR_SYNC_NSEC;

			s->state = WR_WAIT_SYNC_IDLE;
			s->last_tics = get_tics();

		} else s->state = WR_SYNC_NSEC;
		break;

	case WR_SYNC_NSEC:
		strcpy(cur_servo_state.slave_servo_state, "SYNC_NSEC");

		if(ts_offset_hw.nsec != 0)
		{

			strcpy(adjust.port_name, s->if_name);
			adjust.adjust_nsec = ts_offset_hw.nsec;

			fprintf(stderr,"[slave] Adjusting NSEC counter\n");

			halexp_pps_cmd(HEXP_PPSG_CMD_ADJUST_NSEC, &adjust);
			s->next_state = WR_SYNC_PHASE;
			s->state = WR_WAIT_SYNC_IDLE;
			s->last_tics = get_tics();

		} else s->state = WR_SYNC_PHASE;
		break;

	case WR_SYNC_PHASE:
		strcpy(cur_servo_state.slave_servo_state, "SYNC_PHASE");

		s->cur_setpoint = -ts_offset_hw.phase;

		strcpy(adjust.port_name, s->if_name);
		adjust.adjust_phase_shift = s->cur_setpoint;
		halexp_pps_cmd(HEXP_PPSG_CMD_ADJUST_PHASE, &adjust);

		s->next_state = WR_TRACK_PHASE;
		s->state = WR_WAIT_SYNC_IDLE;
		s->last_tics = get_tics();

		s->delta_ms_prev = s->delta_ms;

		break;


	case WR_TRACK_PHASE:
		strcpy(cur_servo_state.slave_servo_state, "TRACK_PHASE");

		cur_servo_state.cur_setpoint = s->cur_setpoint;
		cur_servo_state.cur_skew = s->delta_ms - s->delta_ms_prev;

		if(tracking_enabled)
		{

			// just follow the changes of deltaMS
			s->cur_setpoint -= (s->delta_ms - s->delta_ms_prev);

			strcpy(adjust.port_name, s->if_name);
			adjust.adjust_phase_shift = s->cur_setpoint;
			halexp_pps_cmd(HEXP_PPSG_CMD_ADJUST_PHASE, &adjust);

			s->delta_ms_prev = s->delta_ms;
			s->next_state = WR_TRACK_PHASE;
			s->state = WR_WAIT_SYNC_IDLE;
			s->last_tics = get_tics();

		}

//      sleep(1);
		break;

	}
	return 0;
}


