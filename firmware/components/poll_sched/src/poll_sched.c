/* poll_sched.c - polling schedule, health state machine, statistics (pure C99).
 * Implements the normative schedule rules 1-12 of task 010.
 */

#include "poll_sched.h"
#include "daikin_regs.h"
#include <string.h>

/* Kconfig ranges of task 001 (periods converted from s to ms). */
#define ADDR_MIN            1u
#define ADDR_MAX            247u
#define GAP_MS_MIN          50u
#define GAP_MS_MAX          1000u
#define FAST_MS_MIN         2000u
#define FAST_MS_MAX         60000u
#define SLOW_MS_MIN         10000u
#define SLOW_MS_MAX         3600000u
#define RETRIES_MAX         1u
#define OFFLINE_AFTER_MIN   2u
#define OFFLINE_AFTER_MAX   20u
#define PROBE_MS_MIN        10000u
#define PROBE_MS_MAX        600000u

#define HEALTH_REG          0u      /* T1, read by HEALTH probes while OFFLINE (rule 8) */
#define EXC_ILLEGAL_ADDR    2u      /* exception 02: register not supported (rule 6) */
#define NO_INDEX            (-1)

static int index_of(uint16_t reg)
{
    const dr_reg_t *r = daikin_regs_find(reg);
    if (r == NULL) {
        return NO_INDEX;
    }
    return (int)(r - daikin_regs_at(0));
}

static uint16_t reg_at(int i)
{
    return daikin_regs_at((size_t)i)->reg;
}

static dr_group_t group_at(int i)
{
    return daikin_regs_at((size_t)i)->group;
}

static bool is_running(const poll_sched_t *s)
{
    return s->health == PS_ONLINE || s->health == PS_DEGRADED;
}

/* Rule 1: polled set = FAST/SLOW registers not marked unsupported. */
static bool is_pollable(const poll_sched_t *s, int i)
{
    return group_at(i) != DR_GROUP_NONE && !s->unsupported[i];
}

static uint32_t period_ms(const poll_sched_t *s, int i)
{
    return group_at(i) == DR_GROUP_FAST ? s->cfg.fast_ms : s->cfg.slow_ms;
}

static void make_all_due(poll_sched_t *s, int64_t when_ms)
{
    for (int i = 0; i < RM_REG_COUNT; i++) {
        s->due_ms[i] = when_ms;
    }
}

static void drop_retry_and_sweep(poll_sched_t *s)
{
    s->retry_pending = false;
    s->sweep_active = false;
}

/* ------------------------------------------------------------ lifecycle */

int poll_sched_init(poll_sched_t *s, const ps_config_t *cfg)
{
    if (cfg->addr < ADDR_MIN || cfg->addr > ADDR_MAX ||
        cfg->gap_ms < GAP_MS_MIN || cfg->gap_ms > GAP_MS_MAX ||
        cfg->fast_ms < FAST_MS_MIN || cfg->fast_ms > FAST_MS_MAX ||
        cfg->slow_ms < SLOW_MS_MIN || cfg->slow_ms > SLOW_MS_MAX ||
        cfg->slow_ms < cfg->fast_ms ||
        cfg->retries > RETRIES_MAX ||
        cfg->offline_after < OFFLINE_AFTER_MIN || cfg->offline_after > OFFLINE_AFTER_MAX ||
        cfg->offline_probe_ms < PROBE_MS_MIN || cfg->offline_probe_ms > PROBE_MS_MAX) {
        return -1;
    }
    memset(s, 0, sizeof(*s));
    s->cfg = *cfg;
    s->health = PS_STOPPED;
    s->stats.lat_min_ms = UINT32_MAX;   /* normalised to 0 by get_stats while no sample exists */
    return 0;
}

void poll_sched_start(poll_sched_t *s, int64_t now_ms)
{
    if (s->health != PS_STOPPED) {   /* keeps counter and backoff: no bypass of the OFFLINE probe interval */
        return;
    }
    s->health = PS_ONLINE;
    s->failures = 0;
    s->retry_pending = false;
    make_all_due(s, now_ms);
}

void poll_sched_stop(poll_sched_t *s)
{
    if (s->health == PS_LATCHED) {
        return;
    }
    s->health = PS_STOPPED;
    drop_retry_and_sweep(s);
}

void poll_sched_latch(poll_sched_t *s)
{
    s->health = PS_LATCHED;
}

int poll_sched_set_addr(poll_sched_t *s, uint8_t addr, int64_t now_ms)
{
    if (addr < ADDR_MIN || addr > ADDR_MAX) {
        return -1;
    }
    s->cfg.addr = addr;
    memset(s->unsupported, 0, sizeof(s->unsupported));
    s->failures = 0;
    s->manual_pending = false;
    drop_retry_and_sweep(s);
    make_all_due(s, now_ms);
    /* A new slave starts with a clean health record. */
    if (s->health == PS_DEGRADED || s->health == PS_OFFLINE) {
        s->health = PS_ONLINE;
    }
    return 0;
}

void poll_sched_cancel_manual(poll_sched_t *s)
{
    s->manual_pending = false;
}

int poll_sched_request_manual(poll_sched_t *s, uint16_t reg, uint16_t qty)
{
    if (s->health == PS_LATCHED || !daikin_regs_read_allowed(reg, qty) || s->manual_pending) {
        return -1;
    }
    s->manual_pending = true;
    s->manual_reg = reg;
    s->manual_qty = qty;
    return 0;
}

int poll_sched_request_sweep(poll_sched_t *s)
{
    if (s->health == PS_LATCHED || s->sweep_active) {
        return -1;
    }
    s->sweep_active = true;
    s->sweep_idx = 0;
    return 0;
}

/* ------------------------------------------------------------ next request */

/* Rule 3: FAST before SLOW, then earliest due, then lowest reg (table is ascending,
 * so a strict comparison keeps the lowest index on ties). Returns NO_INDEX if none due. */
static int pick_due_poll(const poll_sched_t *s, int64_t now_ms)
{
    int best = NO_INDEX;
    for (int i = 0; i < RM_REG_COUNT; i++) {
        if (!is_pollable(s, i) || s->due_ms[i] > now_ms) {
            continue;
        }
        if (best == NO_INDEX || group_at(i) < group_at(best) ||
            (group_at(i) == group_at(best) && s->due_ms[i] < s->due_ms[best])) {
            best = i;
        }
    }
    return best;
}

static int64_t earliest_poll_due(const poll_sched_t *s)
{
    int64_t earliest = INT64_MAX;
    for (int i = 0; i < RM_REG_COUNT; i++) {
        if (is_pollable(s, i) && s->due_ms[i] < earliest) {
            earliest = s->due_ms[i];
        }
    }
    return earliest;
}

static void fill_req(const poll_sched_t *s, ps_req_t *out, ps_kind_t kind, uint16_t reg, uint16_t qty)
{
    out->addr = s->cfg.addr;
    out->kind = kind;
    out->reg = reg;
    out->qty = qty;
}

/* Rule 3 order. A request is consumed (pending flag cleared / sweep advanced) when issued. */
static bool pick_request(poll_sched_t *s, int64_t now_ms, ps_req_t *out)
{
    if (s->manual_pending) {
        s->manual_pending = false;
        fill_req(s, out, PS_KIND_MANUAL, s->manual_reg, s->manual_qty);
        return true;
    }
    if (s->retry_pending) {
        s->retry_pending = false;
        fill_req(s, out, PS_KIND_RETRY, s->retry_reg, 1);
        return true;
    }
    if (is_running(s)) {
        int i = pick_due_poll(s, now_ms);
        if (i != NO_INDEX) {
            fill_req(s, out, PS_KIND_POLL, reg_at(i), 1);
            return true;
        }
    }
    if (s->health == PS_OFFLINE && now_ms >= s->next_health_ms) {
        fill_req(s, out, PS_KIND_HEALTH, HEALTH_REG, 1);
        return true;
    }
    if (s->sweep_active && s->health != PS_OFFLINE) {   /* rule 8: suspended while OFFLINE */
        fill_req(s, out, PS_KIND_SWEEP, reg_at(s->sweep_idx), 1);
        s->sweep_idx++;
        if (s->sweep_idx >= daikin_regs_count()) {
            s->sweep_active = false;   /* a new sweep may be requested once the last read is issued */
        }
        return true;
    }
    return false;
}

/* When nothing is ready: the next time a POLL or HEALTH becomes due. Manual, retry and
 * sweep are never pending-but-unready once the gap has passed (sweep is suspended only
 * while OFFLINE, where HEALTH defines the wait). */
static int64_t next_ready_ms(const poll_sched_t *s)
{
    if (is_running(s)) {
        return earliest_poll_due(s);
    }
    if (s->health == PS_OFFLINE) {
        return s->next_health_ms;
    }
    return INT64_MAX;
}

bool poll_sched_next(poll_sched_t *s, int64_t now_ms, ps_req_t *out, int64_t *wait_until_ms)
{
    if (s->health == PS_LATCHED) {
        *wait_until_ms = INT64_MAX;
        return false;
    }
    if (s->have_last && now_ms < s->last_end_ms + (int64_t)s->cfg.gap_ms) {   /* rule 2 */
        *wait_until_ms = s->last_end_ms + (int64_t)s->cfg.gap_ms;
        return false;
    }
    if (pick_request(s, now_ms, out)) {
        return true;
    }
    *wait_until_ms = next_ready_ms(s);
    return false;
}

/* ------------------------------------------------------------ completion */

static void record_stats(poll_sched_t *s, const ps_txn_t *t)
{
    ps_stats_t *st = &s->stats;
    st->txns++;
    switch (t->res) {
    case PS_RES_OK:        st->ok++; break;
    case PS_RES_TIMEOUT:   st->timeout++; break;
    case PS_RES_EXCEPTION: st->exception++; break;
    case PS_RES_BAD_FRAME: st->bad_frame++; break;
    case PS_RES_ERROR:     st->error++; break;
    }
    switch (t->req.kind) {
    case PS_KIND_RETRY:  st->retries++; break;
    case PS_KIND_HEALTH: st->health_probes++; break;
    case PS_KIND_SWEEP:  st->sweeps++; break;
    case PS_KIND_MANUAL: st->manual++; break;
    case PS_KIND_POLL:   break;
    }
    if (t->res != PS_RES_OK && t->res != PS_RES_EXCEPTION) {
        return;
    }
    int64_t dur = t->t_end_ms - t->t_start_ms;
    if (dur < 0) {
        dur = 0;
    }
    uint32_t dur_ms = dur > (int64_t)UINT32_MAX ? UINT32_MAX : (uint32_t)dur;
    uint32_t bucket = dur_ms / PS_LAT_BUCKET_MS;
    if (bucket >= PS_LAT_BUCKETS) {
        bucket = PS_LAT_BUCKETS - 1;
    }
    st->lat_hist[bucket]++;
    if (dur_ms < st->lat_min_ms) {
        st->lat_min_ms = dur_ms;
    }
    if (dur_ms > st->lat_max_ms) {
        st->lat_max_ms = dur_ms;
    }
}

/* Rule 6: exception 02 on a qty 1 POLL, RETRY, SWEEP or MANUAL read marks the register unsupported. */
static void mark_unsupported_if_needed(poll_sched_t *s, const ps_txn_t *t)
{
    bool kind_ok = t->req.kind != PS_KIND_HEALTH;
    if (!kind_ok || t->res != PS_RES_EXCEPTION || t->exc_code != EXC_ILLEGAL_ADDR || t->req.qty != 1) {
        return;
    }
    int i = index_of(t->req.reg);
    if (i != NO_INDEX) {
        s->unsupported[i] = true;
    }
}

static void enter_offline(poll_sched_t *s, int64_t t_end_ms)
{
    s->health = PS_OFFLINE;
    s->stats.offline_entries++;
    s->retry_pending = false;
    s->next_health_ms = t_end_ms + (int64_t)s->cfg.offline_probe_ms;
}

/* Rule 7 health mapping after the counter changed. */
static void update_health(poll_sched_t *s, int64_t t_end_ms)
{
    if (s->failures == 0) {
        s->health = PS_ONLINE;
    } else if (s->failures < s->cfg.offline_after) {
        s->health = PS_DEGRADED;
    } else {
        enter_offline(s, t_end_ms);
    }
}

static bool is_failure(ps_res_t res)
{
    return res == PS_RES_TIMEOUT || res == PS_RES_BAD_FRAME || res == PS_RES_ERROR;
}

/* Rule 4: next due = max(prev_due + period, t_end); period depends on the outcome. */
static void reschedule_poll(poll_sched_t *s, int i, uint32_t period, int64_t t_end_ms)
{
    int64_t next_due = s->due_ms[i] + (int64_t)period;
    s->due_ms[i] = next_due > t_end_ms ? next_due : t_end_ms;
}

static void complete_poll(poll_sched_t *s, const ps_txn_t *t)
{
    int i = index_of(t->req.reg);
    if (!is_running(s) || i == NO_INDEX) {
        return;   /* e.g. stopped while the read was in flight: statistics only */
    }
    if (t->res == PS_RES_OK) {
        s->failures = 0;
        reschedule_poll(s, i, period_ms(s, i), t->t_end_ms);
    } else if (t->res == PS_RES_EXCEPTION) {
        s->failures = 0;
        reschedule_poll(s, i, s->cfg.slow_ms, t->t_end_ms);   /* backoff; never retried */
    } else if (t->req.kind == PS_KIND_POLL && s->cfg.retries > 0) {   /* rule 5 */
        s->retry_pending = true;
        s->retry_reg = t->req.reg;
        return;
    } else {
        s->failures++;
        reschedule_poll(s, i, s->cfg.slow_ms, t->t_end_ms);   /* backoff, D21 */
    }
    update_health(s, t->t_end_ms);
}

static void complete_health(poll_sched_t *s, const ps_txn_t *t)
{
    if (s->health != PS_OFFLINE) {
        return;
    }
    if (is_failure(t->res)) {
        s->failures++;
        s->next_health_ms = t->t_end_ms + (int64_t)s->cfg.offline_probe_ms;
        return;
    }
    s->failures = 0;
    s->health = PS_ONLINE;
    make_all_due(s, t->t_end_ms);   /* rule 8 */
}

void poll_sched_complete(poll_sched_t *s, const ps_txn_t *t)
{
    record_stats(s, t);
    if (t->req.addr != s->cfg.addr) {   /* rule 12: in flight across set_addr; the bus was used, nothing else */
        s->have_last = true;
        s->last_end_ms = t->t_end_ms;
        return;
    }
    mark_unsupported_if_needed(s, t);
    switch (t->req.kind) {
    case PS_KIND_POLL:
    case PS_KIND_RETRY:  complete_poll(s, t); break;
    case PS_KIND_HEALTH: complete_health(s, t); break;
    case PS_KIND_SWEEP:
    case PS_KIND_MANUAL: break;   /* rule 10: no effect on counter, health or schedule */
    }
    s->have_last = true;
    s->last_end_ms = t->t_end_ms;
}

/* ------------------------------------------------------------ queries */

ps_health_t poll_sched_health(const poll_sched_t *s)
{
    return s->health;
}

bool poll_sched_is_unsupported(const poll_sched_t *s, uint16_t reg)
{
    int i = index_of(reg);
    return i != NO_INDEX && s->unsupported[i];
}

void poll_sched_get_stats(const poll_sched_t *s, ps_stats_t *out)
{
    *out = s->stats;
    if (out->lat_min_ms == UINT32_MAX) {
        out->lat_min_ms = 0;
    }
}

uint32_t poll_sched_lat_median_ms(const ps_stats_t *st)
{
    uint32_t total = 0;
    for (int b = 0; b < PS_LAT_BUCKETS; b++) {
        total += st->lat_hist[b];
    }
    if (total == 0) {
        return 0;
    }
    uint32_t target = (total + 1) / 2;
    uint32_t cumulative = 0;
    for (int b = 0; b < PS_LAT_BUCKETS; b++) {
        cumulative += st->lat_hist[b];
        if (cumulative >= target) {
            return (uint32_t)(b + 1) * PS_LAT_BUCKET_MS;
        }
    }
    return 0;   /* unreachable: cumulative reaches total */
}
