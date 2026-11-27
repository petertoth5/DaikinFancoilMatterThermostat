/* write_guard.c - arm session, command sequence PRE -> WRITE -> VERIFY, budget, latch and
 * restore set of the write image (spec 019; pure C99). See write_guard.h for the contract. */

#include "write_guard.h"
#include <string.h>
#include "daikin_regs.h"

/* Registers named by the rules (wiki register-map). */
#define REG_PRG      DR_PRG_REG   /* 201: mode bits 0-2, standby bit 7 */
#define REG_SP       231u         /* absolute setpoint */
#define REG_SEASON   233u         /* "Man": 0 auto, 3 winter, 5 summer */
#define REG_SPL      202u
#define REG_SPH      203u
#define REG_SPL_W    245u
#define REG_SPH_W    246u
#define REG_WEB      247u

/* Register 247 flags: flag n = bit n of the register (inferred, wiki web-restriction-flags). Flags
 * that restrict the local panel or the setpoint range are refused outright (W9). */
#define WEB_FLAG(n)               (1u << (n))
#define WEB_FLAGS_BLOCKING        (WEB_FLAG(1) | WEB_FLAG(2) | WEB_FLAG(3) | WEB_FLAG(6) | WEB_FLAG(7) | WEB_FLAG(8))
#define WEB_FLAG_INHIBIT_EXTREMES 4u   /* 5 C / 40 C ends disabled (inferred reading) */
#define WEB_FLAG_SETPOINT_LIMIT   5u   /* setpoint limited to 245/246 */
#define WEB_FLAG_LAST             8u

#define FULL_WORD    0xFFFFu
#define MS_PER_S     1000

#define RESTORE_STEPS 3u   /* order 201 -> 233 -> 231 (W13) */

static const uint16_t k_arm_registers[] = { REG_PRG, REG_SP, REG_SEASON, REG_SPL, REG_SPH, REG_WEB };
static const uint16_t k_limit_registers[] = { REG_SPL_W, REG_SPH_W };

/* ------------------------------------------------------------ events */

/* A full container reuses its last slot; WG_EVENTS_MAX covers the longest sequence of one call. */
static wg_event_t *push_event(wg_events_t *ev, wg_ev_kind_t kind, int64_t now_ms)
{
    size_t slot = ev->n < WG_EVENTS_MAX ? ev->n++ : WG_EVENTS_MAX - 1u;
    memset(&ev->e[slot], 0, sizeof(ev->e[slot]));
    ev->e[slot].kind = kind;
    ev->e[slot].t_ms = now_ms;
    return &ev->e[slot];
}

static uint32_t seconds_left(const write_guard_t *w, int64_t now_ms)
{
    if (!w->armed || w->deadline_ms <= now_ms) {
        return 0;
    }
    return (uint32_t)((w->deadline_ms - now_ms + MS_PER_S - 1) / MS_PER_S);
}

/* ------------------------------------------------------------ model access */

/* "Fresh OK" of WR2: an OK read, fresh, inside the register's fixed range. */
static bool fresh_ok(const reg_model_t *m, uint16_t reg, int64_t now_ms, uint16_t *raw)
{
    rm_entry_t e;
    if (!reg_model_get(m, reg, &e) || e.avail != RM_OK || e.out_of_range || !reg_model_fresh(m, reg, now_ms)) {
        return false;
    }
    if (raw != NULL) {
        *raw = e.raw;
    }
    return true;
}

/* Last known flags of 247, whatever their age (a stale set flag must still stop writes). */
static uint16_t last_web_flags(const reg_model_t *m)
{
    rm_entry_t e;
    return (reg_model_get(m, REG_WEB, &e) && e.has_value) ? e.raw : 0u;
}

/* Lowest blocking flag number set in flags, 0 if none (flag 0 is harmless). */
static unsigned blocking_flag(uint16_t flags)
{
    for (unsigned n = 1; n <= WEB_FLAG_LAST; n++) {
        if ((WEB_FLAGS_BLOCKING & WEB_FLAG(n)) != 0u && (flags & WEB_FLAG(n)) != 0u) {
            return n;
        }
    }
    return 0;
}

/* ------------------------------------------------------------ refusals */

static bool refuse_with(wg_event_t *why, wg_refuse_t refuse, uint16_t detail)
{
    why->refuse = refuse;
    why->detail = detail;
    return false;
}

static bool registers_fresh(const reg_model_t *m, const uint16_t *regs, size_t count, int64_t now_ms,
                            wg_event_t *why)
{
    for (size_t i = 0; i < count; i++) {
        if (!fresh_ok(m, regs[i], now_ms, NULL)) {
            return refuse_with(why, WG_REFUSE_STALE, regs[i]);
        }
    }
    return true;
}

static bool no_blocking_flag(uint16_t flags, wg_event_t *why)
{
    unsigned n = blocking_flag(flags);
    return n == 0 ? true : refuse_with(why, WG_REFUSE_WEB_FLAG, (uint16_t)n);
}

/* 247 fresh OK and none of its blocking flags set. *flags is the register value on success. */
static bool web_flags_clear(const reg_model_t *m, int64_t now_ms, uint16_t *flags, wg_event_t *why)
{
    if (!fresh_ok(m, REG_WEB, now_ms, flags)) {
        return refuse_with(why, WG_REFUSE_STALE, REG_WEB);
    }
    return no_blocking_flag(*flags, why);
}

/* WR2 preconditions, in the order of the refusal reasons. */
static bool arm_allowed(const write_guard_t *w, const poll_sched_t *s, const reg_model_t *m, int64_t now_ms,
                        wg_event_t *why)
{
    if (w->latched) {
        return refuse_with(why, WG_REFUSE_LATCHED, 0);
    }
    if (poll_sched_health(s) != PS_ONLINE) {
        return refuse_with(why, WG_REFUSE_HEALTH, 0);
    }
    if (!registers_fresh(m, k_arm_registers, sizeof(k_arm_registers) / sizeof(k_arm_registers[0]), now_ms, why)) {
        return false;
    }
    uint16_t flags = last_web_flags(m);   /* 247 is fresh OK here */
    if ((flags & WEB_FLAG(WEB_FLAG_SETPOINT_LIMIT)) != 0u &&
        !registers_fresh(m, k_limit_registers, sizeof(k_limit_registers) / sizeof(k_limit_registers[0]), now_ms, why)) {
        return false;
    }
    return no_blocking_flag(flags, why);
}

static int32_t clamp_low(int32_t lo, int32_t bound) { return bound > lo ? bound : lo; }
static int32_t clamp_high(int32_t hi, int32_t bound) { return bound < hi ? bound : hi; }

/* WR7: [202,203] and, with flag 5, [245,246], inside the fixed 231 bounds; with flag 4 the ends are out. */
static bool setpoint_range(const reg_model_t *m, int64_t now_ms, uint16_t flags, uint16_t *lo, uint16_t *hi,
                           wg_event_t *why)
{
    uint16_t spl, sph, spl_w = 0, sph_w = 0;
    bool use_web_limits = (flags & WEB_FLAG(WEB_FLAG_SETPOINT_LIMIT)) != 0u;
    if (!fresh_ok(m, REG_SPL, now_ms, &spl)) {
        return refuse_with(why, WG_REFUSE_STALE, REG_SPL);
    }
    if (!fresh_ok(m, REG_SPH, now_ms, &sph)) {
        return refuse_with(why, WG_REFUSE_STALE, REG_SPH);
    }
    if (use_web_limits && !fresh_ok(m, REG_SPL_W, now_ms, &spl_w)) {
        return refuse_with(why, WG_REFUSE_STALE, REG_SPL_W);
    }
    if (use_web_limits && !fresh_ok(m, REG_SPH_W, now_ms, &sph_w)) {
        return refuse_with(why, WG_REFUSE_STALE, REG_SPH_W);
    }
    const dr_reg_t *sp = daikin_regs_find(REG_SP);
    int32_t low = clamp_low(spl, sp->min_raw);
    int32_t high = clamp_high(sph, sp->max_raw);
    if (use_web_limits) {
        low = clamp_low(low, spl_w);
        high = clamp_high(high, sph_w);
    }
    if ((flags & WEB_FLAG(WEB_FLAG_INHIBIT_EXTREMES)) != 0u) {
        low = clamp_low(low, sp->min_raw + 1);
        high = clamp_high(high, sp->max_raw - 1);
    }
    *lo = (uint16_t)low;
    *hi = (uint16_t)high;
    return true;
}

/* WR5 step 1 after armed/latched/busy: pacing, flags, and for the setpoint the limits. */
static bool start_allowed(const write_guard_t *w, const reg_model_t *m, int64_t now_ms, bool is_setpoint,
                          uint16_t target, wg_event_t *why)
{
    if (w->have_cmd_end && now_ms < w->cmd_end_ms + WG_PACING_MS) {
        return refuse_with(why, WG_REFUSE_PACING, (uint16_t)(w->cmd_end_ms + WG_PACING_MS - now_ms));
    }
    uint16_t flags;
    if (!web_flags_clear(m, now_ms, &flags, why)) {
        return false;
    }
    if (!is_setpoint) {
        return true;
    }
    uint16_t lo, hi;
    if (!setpoint_range(m, now_ms, flags, &lo, &hi, why)) {
        return false;
    }
    if (target < lo || target > hi) {
        why->lo = lo;
        why->hi = hi;
        return refuse_with(why, WG_REFUSE_LIMIT, 0);
    }
    return true;
}

static void push_refusal(wg_events_t *ev, int64_t now_ms, wg_cmd_type_t cmd, const wg_event_t *why)
{
    wg_event_t *e = push_event(ev, WG_EV_REFUSED, now_ms);
    e->cmd = cmd;
    e->refuse = why->refuse;
    e->detail = why->detail;
    e->lo = why->lo;
    e->hi = why->hi;
}

/* ------------------------------------------------------------ session end */

/* The bus (or the address it was written to) is gone: a VERIFY cannot be trusted any more. */
static bool bus_gone(wg_reason_t reason)
{
    return reason == WG_DISARM_LINK || reason == WG_DISARM_LATCHED || reason == WG_DISARM_ADDR ||
           reason == WG_DISARM_POLL_OFF || reason == WG_DISARM_SILENCE;
}

static void finish_command(write_guard_t *w, bool ok)
{
    w->phase = WG_PHASE_IDLE;
    if (!ok) {
        w->restore_active = false;
    }
}

static void disarm_internal(write_guard_t *w, wg_reason_t reason, int64_t now_ms, wg_events_t *ev);

/* WR8: every failure disarms and counts toward the per-boot latch. */
static void register_failure(write_guard_t *w, int64_t now_ms, wg_events_t *ev, wg_fail_t fail, ps_res_t res,
                             uint16_t mismatch_raw)
{
    uint16_t reg = w->op_reg;
    finish_command(w, false);
    if (w->failures < UINT8_MAX) {
        w->failures++;
    }
    wg_event_t *e = push_event(ev, WG_EV_FAIL, now_ms);
    e->reg = reg;
    e->fail = fail;
    e->res = res;
    e->old_raw = mismatch_raw;
    e->failures = w->failures;
    disarm_internal(w, WG_DISARM_FAILURE, now_ms, ev);
    if (w->failures == WG_MAX_FAILURES) {
        w->latched = true;
        push_event(ev, WG_EV_LATCHED, now_ms)->failures = w->failures;
    }
}

static void disarm_internal(write_guard_t *w, wg_reason_t reason, int64_t now_ms, wg_events_t *ev)
{
    if (reason == WG_DISARM_ADDR) {
        w->restore_valid = false;   /* the restore set belongs to one slave address */
    }
    if (w->armed) {
        w->armed = false;
        w->restore_active = false;
        w->last_disarm = reason;
        push_event(ev, WG_EV_DISARMED, now_ms)->reason = reason;
    }
    if (w->phase == WG_PHASE_VERIFY && bus_gone(reason)) {
        register_failure(w, now_ms, ev, WG_FAIL_VERIFY_LOST, PS_RES_ERROR, 0);
    }
}

/* A command that never reached its WRITE because the session ended (no FC06 was sent). */
static void abort_command(write_guard_t *w, int64_t now_ms, wg_events_t *ev)
{
    wg_event_t *e = push_event(ev, WG_EV_ABORTED, now_ms);
    e->reg = w->op_reg;
    e->reason = w->last_disarm;
    finish_command(w, false);
}

/* Timeout, health and 247 flags are checked on every call (WR9). */
static void housekeeping(write_guard_t *w, const poll_sched_t *s, const reg_model_t *m, int64_t now_ms,
                         wg_events_t *ev)
{
    if (!w->armed) {
        return;
    }
    if (now_ms >= w->deadline_ms) {
        disarm_internal(w, WG_DISARM_TIMEOUT, now_ms, ev);
        return;
    }
    ps_health_t health = poll_sched_health(s);
    if (health == PS_STOPPED || health == PS_LATCHED) {
        disarm_internal(w, health == PS_LATCHED ? WG_DISARM_LATCHED : WG_DISARM_POLL_OFF, now_ms, ev);
        return;
    }
    if (health != PS_ONLINE) {
        disarm_internal(w, WG_DISARM_HEALTH, now_ms, ev);
        return;
    }
    if (blocking_flag(last_web_flags(m)) != 0u) {
        disarm_internal(w, WG_DISARM_WEB_FLAGS, now_ms, ev);
    }
}

/* ------------------------------------------------------------ public: lifecycle */

void write_guard_init(write_guard_t *w)
{
    memset(w, 0, sizeof(*w));
    w->budget_left = WG_BUDGET_FRAMES;
}

static void capture_restore_set(write_guard_t *w, const reg_model_t *m, int64_t now_ms, wg_events_t *ev)
{
    if (w->restore_valid) {
        return;   /* first successful arm per boot and address only */
    }
    (void)fresh_ok(m, REG_PRG, now_ms, &w->restore_201);
    (void)fresh_ok(m, REG_SEASON, now_ms, &w->restore_233);
    (void)fresh_ok(m, REG_SP, now_ms, &w->restore_231);
    w->restore_valid = true;
    wg_event_t *e = push_event(ev, WG_EV_RESTORE_SET, now_ms);
    e->restore_201 = w->restore_201;
    e->restore_233 = w->restore_233;
    e->restore_231 = w->restore_231;
}

bool write_guard_arm(write_guard_t *w, const poll_sched_t *s, const reg_model_t *m, uint32_t seconds,
                     int64_t now_ms, wg_events_t *ev)
{
    ev->n = 0;
    if (seconds < WG_ARM_MIN_S || seconds > WG_ARM_MAX_S) {
        return false;
    }
    housekeeping(w, s, m, now_ms, ev);
    if (!w->armed && (w->phase == WG_PHASE_PRE || w->phase == WG_PHASE_WRITE)) {
        abort_command(w, now_ms, ev);   /* a command of the ended session must not run in the new one */
    }
    wg_event_t why;
    memset(&why, 0, sizeof(why));
    if (!arm_allowed(w, s, m, now_ms, &why)) {
        wg_event_t *e = push_event(ev, WG_EV_ARM_REFUSED, now_ms);
        e->refuse = why.refuse;
        e->detail = why.detail;
        return false;
    }
    int64_t wanted_deadline = now_ms + (int64_t)seconds * MS_PER_S;
    bool extend = w->armed;
    if (!extend || wanted_deadline > w->deadline_ms) {   /* re-arm never shortens */
        w->deadline_ms = wanted_deadline;
    }
    if (!extend) {
        w->armed = true;
        w->budget_left = WG_BUDGET_FRAMES;
    }
    wg_event_t *e = push_event(ev, extend ? WG_EV_EXTENDED : WG_EV_ARMED, now_ms);
    e->seconds = (uint16_t)seconds_left(w, now_ms);
    e->budget = w->budget_left;
    if (!extend) {
        capture_restore_set(w, m, now_ms, ev);
    }
    return true;
}

void write_guard_disarm(write_guard_t *w, wg_reason_t reason, int64_t now_ms, wg_events_t *ev)
{
    ev->n = 0;
    disarm_internal(w, reason, now_ms, ev);
}

bool write_guard_may_transmit(const write_guard_t *w, const poll_sched_t *s)
{
    return w->armed && w->budget_left > 0 && !w->latched && poll_sched_health(s) == PS_ONLINE;
}

void write_guard_get_status(const write_guard_t *w, int64_t now_ms, wg_status_t *out)
{
    out->armed = w->armed;
    out->left_s = seconds_left(w, now_ms);
    out->budget_left = w->budget_left;
    out->failures = w->failures;
    out->latched = w->latched;
    out->restore_valid = w->restore_valid;
    out->restore_201 = w->restore_201;
    out->restore_233 = w->restore_233;
    out->restore_231 = w->restore_231;
}

/* ------------------------------------------------------------ public: commands */

static bool command_arg_valid(const wg_cmd_t *c)
{
    switch (c->type) {
    case WG_CMD_SP:      return true;   /* range-checked as a limit */
    case WG_CMD_STANDBY: return c->arg <= 1u;
    case WG_CMD_FAN:     return c->arg <= WG_FAN_MAX;
    case WG_CMD_SEASON:  return daikin_regs_in_range(daikin_regs_find(REG_SEASON), c->arg);
    case WG_CMD_RESTORE: return true;
    }
    return false;
}

/* Register, changeable bits and wanted bit values of a (non-restore) command. */
static void set_operation(write_guard_t *w, const wg_cmd_t *c)
{
    w->op_cmd = c->type;
    w->op_mask = FULL_WORD;
    w->op_target = c->arg;
    switch (c->type) {
    case WG_CMD_SP:      w->op_reg = REG_SP; break;
    case WG_CMD_SEASON:  w->op_reg = REG_SEASON; break;
    case WG_CMD_STANDBY: w->op_reg = REG_PRG; w->op_mask = DR_PRG_STBY_BIT; w->op_target = c->arg ? DR_PRG_STBY_BIT : 0u; break;
    case WG_CMD_FAN:     w->op_reg = REG_PRG; w->op_mask = DR_PRG_MODE_MASK; break;
    case WG_CMD_RESTORE: break;
    }
}

bool write_guard_command(write_guard_t *w, const poll_sched_t *s, const reg_model_t *m, const wg_cmd_t *c,
                         int64_t now_ms, wg_events_t *ev)
{
    ev->n = 0;
    housekeeping(w, s, m, now_ms, ev);
    if (!command_arg_valid(c)) {
        return false;
    }
    wg_event_t why;
    memset(&why, 0, sizeof(why));
    if (w->latched) {
        refuse_with(&why, WG_REFUSE_LATCHED, 0);
    } else if (!w->armed) {
        refuse_with(&why, WG_REFUSE_DISARMED, 0);
    } else if (w->phase != WG_PHASE_IDLE || w->restore_active) {
        refuse_with(&why, WG_REFUSE_BUSY, 0);
    } else {
        set_operation(w, c);
        if (start_allowed(w, m, now_ms, c->type == WG_CMD_SP, w->op_target, &why)) {
            wg_event_t *e = push_event(ev, WG_EV_CMD, now_ms);
            e->cmd = c->type;
            e->arg = c->arg;
            if (c->type == WG_CMD_RESTORE) {
                w->restore_active = true;
                w->restore_next = 0;
            } else {
                w->phase = WG_PHASE_PRE;
            }
            return true;
        }
    }
    push_refusal(ev, now_ms, c->type, &why);
    return false;
}

/* ------------------------------------------------------------ restore (WR3) */

/* Register, bits and value of restore step i: 201 (mode and standby only), 233, 231. */
static void restore_operation(write_guard_t *w, unsigned i)
{
    w->op_cmd = WG_CMD_RESTORE;
    w->op_mask = FULL_WORD;
    if (i == 0) {
        w->op_reg = REG_PRG;
        w->op_mask = DR_PRG_MODE_MASK | DR_PRG_STBY_BIT;
        w->op_target = w->restore_201 & w->op_mask;
    } else if (i == 1) {
        w->op_reg = REG_SEASON;
        w->op_target = w->restore_233;
    } else {
        w->op_reg = REG_SP;
        w->op_target = w->restore_231;
    }
}

/* A register needs no restore only when its fresh value already equals the stored one. */
static bool restore_needed(write_guard_t *w, const reg_model_t *m, int64_t now_ms, unsigned i)
{
    uint16_t current;
    restore_operation(w, i);
    if (!fresh_ok(m, w->op_reg, now_ms, &current)) {
        return true;
    }
    return ((current ^ w->op_target) & w->op_mask) != 0u;
}

static bool restore_pacing_elapsed(const write_guard_t *w, int64_t now_ms)
{
    return !w->have_cmd_end || now_ms >= w->cmd_end_ms + WG_PACING_MS;
}

/* Start the next differing register as a full write command once the pacing allows. */
static void start_restore_step(write_guard_t *w, const reg_model_t *m, int64_t now_ms, wg_events_t *ev)
{
    if (!w->armed || !w->restore_active || w->phase != WG_PHASE_IDLE) {
        return;
    }
    while (w->restore_next < RESTORE_STEPS && !restore_needed(w, m, now_ms, w->restore_next)) {
        w->restore_next++;
    }
    if (w->restore_next >= RESTORE_STEPS) {
        w->restore_active = false;
        return;
    }
    if (!restore_pacing_elapsed(w, now_ms)) {
        return;
    }
    restore_operation(w, w->restore_next);
    w->restore_next++;
    wg_event_t why;
    memset(&why, 0, sizeof(why));
    if (start_allowed(w, m, now_ms, w->op_reg == REG_SP, w->op_target, &why)) {
        w->phase = WG_PHASE_PRE;
        return;
    }
    push_refusal(ev, now_ms, WG_CMD_RESTORE, &why);
    w->restore_active = false;
}

/* ------------------------------------------------------------ transactions */

static void begin_txn(const write_guard_t *w, const wg_env_t *env, ps_kind_t kind, ps_txn_t *txn)
{
    memset(txn, 0, sizeof(*txn));
    txn->req.addr = w->op_addr;
    txn->req.reg = w->op_reg;
    txn->req.qty = 1;
    txn->req.kind = kind;
    txn->t_start_ms = env->ps.clock(env->ps.clock_ctx);
}

/* The bus was used: statistics, gap (poll_sched) and the pacing reference of the next command. */
static void end_txn(write_guard_t *w, poll_sched_t *s, const wg_env_t *env, ps_txn_t *txn)
{
    txn->t_end_ms = env->ps.clock(env->ps.clock_ctx);
    poll_sched_complete(s, txn);
    w->have_cmd_end = true;
    w->cmd_end_ms = txn->t_end_ms;
}

static void read_register(write_guard_t *w, poll_sched_t *s, reg_model_t *m, const wg_env_t *env, ps_kind_t kind,
                          ps_txn_t *txn)
{
    begin_txn(w, env, kind, txn);
    txn->res = env->ps.link(env->ps.link_ctx, txn->req.addr, txn->req.reg, 1, txn->val, &txn->exc_code);
    end_txn(w, s, env, txn);
    if (txn->res == PS_RES_OK) {   /* bus is truth; failures leave the model alone */
        reg_model_set_ok(m, txn->req.reg, txn->val[0], txn->t_end_ms);
    }
}

static bool mode_undefined(const write_guard_t *w, uint16_t pre)
{
    return w->op_reg == REG_PRG && (pre & DR_PRG_MODE_MASK) > WG_FAN_MAX;   /* bits 100-111 (A6) */
}

static void do_pre(write_guard_t *w, poll_sched_t *s, reg_model_t *m, const wg_env_t *env, ps_txn_t *txn,
                   wg_events_t *ev)
{
    w->op_addr = poll_sched_addr(s);
    read_register(w, s, m, env, PS_KIND_PRE, txn);
    int64_t t = txn->t_end_ms;
    if (txn->res != PS_RES_OK) {
        register_failure(w, t, ev, WG_FAIL_PRE, txn->res, 0);
        return;
    }
    uint16_t pre = txn->val[0];
    uint16_t mask = w->op_mask & daikin_regs_write_mask(w->op_reg);
    if (mode_undefined(w, pre)) {
        wg_event_t *e = push_event(ev, WG_EV_REFUSED, t);
        e->cmd = w->op_cmd;
        e->refuse = WG_REFUSE_UNDEFINED_MODE;
        finish_command(w, false);
        return;
    }
    w->op_pre = pre;
    w->op_new = (uint16_t)((pre & ~mask) | (w->op_target & mask));
    if (w->op_new == pre) {
        wg_event_t *e = push_event(ev, WG_EV_SKIP, t);
        e->reg = w->op_reg;
        e->old_raw = pre;
        finish_command(w, true);
        return;
    }
    w->phase = WG_PHASE_WRITE;
}

static void do_write(write_guard_t *w, poll_sched_t *s, const wg_env_t *env, ps_txn_t *txn)
{
    w->budget_left--;   /* a frame counts when it is sent, whatever the result */
    begin_txn(w, env, PS_KIND_WRITE, txn);
    txn->val[0] = w->op_new;
    txn->res = env->write(env->write_ctx, txn->req.addr, txn->req.reg, w->op_new, &txn->exc_code);
    end_txn(w, s, env, txn);
    w->op_write_failed = txn->res != PS_RES_OK;
    w->op_write_res = txn->res;
    w->phase = WG_PHASE_VERIFY;
}

static void report_done(write_guard_t *w, int64_t t, wg_events_t *ev, uint16_t verified)
{
    wg_event_t *e = push_event(ev, WG_EV_DONE, t);
    e->reg = w->op_reg;
    e->old_raw = w->op_pre;
    e->new_raw = verified;
    e->budget = w->budget_left;
    finish_command(w, true);
}

/* Runs after every sent FC06, also after a failed WRITE and after a disarm. */
static void do_verify(write_guard_t *w, poll_sched_t *s, reg_model_t *m, const wg_env_t *env, ps_txn_t *txn,
                      wg_events_t *ev)
{
    read_register(w, s, m, env, PS_KIND_VERIFY, txn);
    int64_t t = txn->t_end_ms;
    uint16_t mask = w->op_mask & daikin_regs_write_mask(w->op_reg);
    if (w->op_write_failed) {
        register_failure(w, t, ev, WG_FAIL_WRITE, w->op_write_res, 0);
    } else if (txn->res != PS_RES_OK) {
        register_failure(w, t, ev, WG_FAIL_VERIFY, txn->res, 0);
    } else if (((txn->val[0] ^ w->op_new) & mask) != 0u) {
        register_failure(w, t, ev, WG_FAIL_MISMATCH, PS_RES_OK, txn->val[0]);
    } else {
        report_done(w, t, ev, txn->val[0]);
    }
    if (w->budget_left == 0) {
        disarm_internal(w, WG_DISARM_BUDGET, t, ev);   /* no-op if the failure already disarmed */
    }
}

/* PRE needs a session; the WRITE needs the WR10 conditions again right before it goes out. */
static bool transmit_allowed(const write_guard_t *w, const poll_sched_t *s)
{
    if (w->phase == WG_PHASE_WRITE) {
        return write_guard_may_transmit(w, s);
    }
    return w->armed;
}

/* Why a pending command may not use the bus any more, WG_DISARM_USER if it still may. Independent
 * of the hooks: polling stopped, the latch, or a slave address other than the one of the PRE. */
static wg_reason_t bus_loss_reason(const write_guard_t *w, const poll_sched_t *s)
{
    ps_health_t health = poll_sched_health(s);
    if (health == PS_STOPPED) {
        return WG_DISARM_POLL_OFF;
    }
    if (health == PS_LATCHED) {
        return WG_DISARM_LATCHED;
    }
    bool after_pre = w->phase == WG_PHASE_WRITE || w->phase == WG_PHASE_VERIFY;
    if (after_pre && poll_sched_addr(s) != w->op_addr) {
        return WG_DISARM_ADDR;
    }
    return WG_DISARM_USER;
}

/* A pending VERIFY ends as verify_lost, a PRE/WRITE as ABORTED (A6). */
static void end_pending_on_bus_loss(write_guard_t *w, const poll_sched_t *s, int64_t now_ms, wg_events_t *ev)
{
    if (w->phase == WG_PHASE_IDLE) {
        return;
    }
    wg_reason_t reason = bus_loss_reason(w, s);
    if (reason == WG_DISARM_USER) {
        return;
    }
    disarm_internal(w, reason, now_ms, ev);   /* a bus-gone reason fails a pending VERIFY */
    if (w->phase == WG_PHASE_PRE || w->phase == WG_PHASE_WRITE) {
        abort_command(w, now_ms, ev);
    }
}

static int64_t earlier(int64_t a, int64_t b)
{
    return a < b ? a : b;
}

/* Next moment the guard itself needs the step to run: the session deadline. */
static int64_t deadline_wake(const write_guard_t *w)
{
    return w->armed ? w->deadline_ms : INT64_MAX;
}

static bool run_pending(write_guard_t *w, poll_sched_t *s, reg_model_t *m, const wg_env_t *env, int64_t now_ms,
                        ps_txn_t *txn, wg_events_t *ev, int64_t *wait_until_ms)
{
    int64_t ready_ms = poll_sched_ready_ms(s);   /* D16 gap applies to write transactions too */
    if (now_ms < ready_ms) {
        *wait_until_ms = earlier(ready_ms, deadline_wake(w));
        return false;
    }
    switch (w->phase) {
    case WG_PHASE_PRE:    do_pre(w, s, m, env, txn, ev); break;
    case WG_PHASE_WRITE:  do_write(w, s, env, txn); break;
    case WG_PHASE_VERIFY: do_verify(w, s, m, env, txn, ev); break;
    case WG_PHASE_IDLE:   break;
    }
    return true;
}

bool write_guard_step(write_guard_t *w, poll_sched_t *s, reg_model_t *m, const wg_env_t *env,
                      ps_txn_t *txn, wg_events_t *ev, int64_t *wait_until_ms)
{
    ev->n = 0;
    int64_t now_ms = env->ps.clock(env->ps.clock_ctx);
    housekeeping(w, s, m, now_ms, ev);
    start_restore_step(w, m, now_ms, ev);
    end_pending_on_bus_loss(w, s, now_ms, ev);
    if (w->phase == WG_PHASE_PRE || w->phase == WG_PHASE_WRITE) {
        if (!transmit_allowed(w, s)) {
            abort_command(w, now_ms, ev);
        }
    }
    if (w->phase != WG_PHASE_IDLE) {
        return run_pending(w, s, m, env, now_ms, txn, ev, wait_until_ms);
    }
    if (poll_step(s, m, &env->ps, txn, wait_until_ms)) {
        return true;
    }
    *wait_until_ms = earlier(*wait_until_ms, deadline_wake(w));
    if (w->armed && w->restore_active && w->have_cmd_end) {
        *wait_until_ms = earlier(*wait_until_ms, w->cmd_end_ms + WG_PACING_MS);
    }
    return false;
}
