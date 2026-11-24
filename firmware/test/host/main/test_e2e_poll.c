/* test_e2e_poll.c - end-to-end host tests of phase-1 polling (task 011).
 * poll_step -> poll_sched -> reg_model against a scripted fake slave behind the link seam.
 * Simulated time only: the fake advances the clock, nothing sleeps.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "unity.h"
#include "daikin_regs.h"
#include "fake_slave.h"
#include "poll_sched.h"
#include "reg_model.h"
#include "txn_log.h"

/* ------------------------------------------------------------ constants */

/* 001 Kconfig defaults. */
#define SLAVE_ADDR        1
#define GAP_MS            100
#define FAST_MS           5000
#define SLOW_MS           60000
#define RETRIES           1
#define OFFLINE_AFTER     5
#define PROBE_MS          30000
#define RESP_TIMEOUT_MS   500
#define STALE_FAST_MS     (3 * FAST_MS)   /* stale = 3 x period (task 010) */
#define STALE_SLOW_MS     (3 * SLOW_MS)

#define SECOND_MS         1000
#define LATENCY_MS        FAKE_DEFAULT_LATENCY_MS
#define QUIET_PERIOD_MS   (120 * SECOND_MS)
#define SILENCE_WINDOW_MS 5000            /* 006 boot silence check */

/* Registers named by the scenarios (register-map). */
#define REG_T1            0               /* local temperature; HEALTH probe register */
#define REG_SP_READ       8
#define REG_STAT          104
#define REG_T2            1
#define REG_OUT           9

/* Synthetic values that differ from the daikin_regs defaults (task 011). */
#define RAW_T1_21_5C      0x00d7
#define RAW_SP_20_0C      0x00c8

#define HEALTHY_FIXTURE         FIXTURE_DIR "/healthy-synthetic.log"
#define STARRED_TIMEOUT_FIXTURE FIXTURE_DIR "/starred-timeout-synthetic.log"

static const ps_config_t k_cfg = {
    .addr = SLAVE_ADDR, .gap_ms = GAP_MS, .fast_ms = FAST_MS, .slow_ms = SLOW_MS,
    .offline_probe_ms = PROBE_MS, .retries = RETRIES, .offline_after = OFFLINE_AFTER,
};

/* First automatic cycle (010): FAST by register, then SLOW by register. */
static const uint16_t k_first_cycle[] = { 0, 8, 104, 201, 231, 233, 1, 9, 105, 202, 203, 245, 246, 247 };

/* ------------------------------------------------------------ rig */

typedef struct {
    fake_slave_t fake;
    int64_t clock_ms;
    poll_sched_t sched;
    reg_model_t model;
    ps_env_t env;
    bool ever_degraded, ever_offline;
} rig_t;

static rig_t g_a, g_b;   /* static: the fake carries large journal and replay tables */

static void rig_init(rig_t *r)
{
    r->clock_ms = 0;
    r->ever_degraded = false;
    r->ever_offline = false;
    fake_init(&r->fake, SLAVE_ADDR, &r->clock_ms, RESP_TIMEOUT_MS);
    TEST_ASSERT_EQUAL_INT(0, poll_sched_init(&r->sched, &k_cfg));
    reg_model_init(&r->model, STALE_FAST_MS, STALE_SLOW_MS);
    r->env = (ps_env_t){ .link = fake_link, .link_ctx = &r->fake, .clock = fake_clock, .clock_ctx = &r->fake };
}

static void rig_start(rig_t *r)
{
    poll_sched_start(&r->sched, r->clock_ms);
}

static bool rig_step(rig_t *r, ps_txn_t *txn, int64_t *wait_ms)
{
    if (!poll_step(&r->sched, &r->model, &r->env, txn, wait_ms)) {
        return false;
    }
    fake_tag_last(&r->fake, txn->req.kind);
    ps_health_t h = poll_sched_health(&r->sched);
    r->ever_degraded |= (h == PS_DEGRADED);
    r->ever_offline |= (h == PS_OFFLINE);
    return true;
}

/* Idle time passes: jump to the wake-up time (capped), never backwards. */
static void rig_advance(rig_t *r, int64_t wake_ms, int64_t cap_ms)
{
    int64_t target = wake_ms < cap_ms ? wake_ms : cap_ms;
    TEST_ASSERT_GREATER_THAN_INT64_MESSAGE(r->clock_ms, target, "scheduler made no progress");
    r->clock_ms = target;
}

static void rig_run_until(rig_t *r, int64_t end_ms)
{
    while (r->clock_ms < end_ms) {
        ps_txn_t txn;
        int64_t wait_ms;
        if (!rig_step(r, &txn, &wait_ms)) {
            rig_advance(r, wait_ms, end_ms);
        }
    }
}

static void rig_run_for(rig_t *r, int64_t duration_ms)
{
    rig_run_until(r, r->clock_ms + duration_ms);
}

static const ps_txn_t *journal_of(const rig_t *r, size_t *n)
{
    const ps_txn_t *j;
    *n = fake_journal(&r->fake, &j);
    return j;
}

static size_t journal_size(const rig_t *r)
{
    size_t n;
    journal_of(r, &n);
    return n;
}

/* ------------------------------------------------------------ synthetic slave */

typedef enum { STARRED_EXCEPTION, STARRED_SILENT } starred_mode_t;

static uint16_t synthetic_raw(const dr_reg_t *r)
{
    if (r->reg == REG_T1) {
        return RAW_T1_21_5C;
    }
    if (r->reg == REG_SP_READ) {
        return RAW_SP_20_0C;
    }
    return r->has_default ? (uint16_t)r->default_raw : 0;
}

static void populate_synthetic(fake_slave_t *f, starred_mode_t mode)
{
    for (size_t i = 0; i < daikin_regs_count(); i++) {
        const dr_reg_t *r = daikin_regs_at(i);
        if (!r->starred) {
            fake_set_reg(f, r->reg, synthetic_raw(r), LATENCY_MS);
        } else if (mode == STARRED_EXCEPTION) {
            fake_set_exception(f, r->reg, 2);
        } else {
            fake_set_silent(f, r->reg);
        }
    }
}

static bool is_unstarred_polled(const dr_reg_t *r)
{
    return !r->starred && r->group != DR_GROUP_NONE;
}

/* ------------------------------------------------------------ assertions */

static size_t count_reg(const ps_txn_t *j, size_t n, uint16_t reg)
{
    size_t c = 0;
    for (size_t i = 0; i < n; i++) {
        c += (j[i].req.reg == reg);
    }
    return c;
}

static size_t count_reg_kind(const ps_txn_t *j, size_t n, uint16_t reg, ps_kind_t kind)
{
    size_t c = 0;
    for (size_t i = 0; i < n; i++) {
        c += (j[i].req.reg == reg && j[i].req.kind == kind);
    }
    return c;
}

static void assert_gaps_respected(const ps_txn_t *j, size_t n)
{
    for (size_t i = 0; i + 1 < n; i++) {
        TEST_ASSERT_GREATER_OR_EQUAL_INT64_MESSAGE(GAP_MS, j[i + 1].t_start_ms - j[i].t_end_ms, "bus gap violated");
    }
}

static void assert_txn_equal(const ps_txn_t *expected, const ps_txn_t *actual)
{
    TEST_ASSERT_EQUAL_UINT8(expected->req.addr, actual->req.addr);
    TEST_ASSERT_EQUAL_UINT16(expected->req.reg, actual->req.reg);
    TEST_ASSERT_EQUAL_UINT16(expected->req.qty, actual->req.qty);
    TEST_ASSERT_EQUAL_INT(expected->req.kind, actual->req.kind);
    TEST_ASSERT_EQUAL_INT(expected->res, actual->res);
    TEST_ASSERT_EQUAL_UINT8(expected->exc_code, actual->exc_code);
    TEST_ASSERT_EQUAL_UINT16_ARRAY(expected->val, actual->val, 4);
    TEST_ASSERT_EQUAL_INT64(expected->t_start_ms, actual->t_start_ms);
    TEST_ASSERT_EQUAL_INT64(expected->t_end_ms, actual->t_end_ms);
}

static void assert_same_end_state(const rig_t *expected, const rig_t *actual)
{
    TEST_ASSERT_EQUAL_INT(poll_sched_health(&expected->sched), poll_sched_health(&actual->sched));
    for (size_t i = 0; i < daikin_regs_count(); i++) {
        uint16_t reg = daikin_regs_at(i)->reg;
        rm_entry_t e, a;
        TEST_ASSERT_TRUE(reg_model_get(&expected->model, reg, &e));
        TEST_ASSERT_TRUE(reg_model_get(&actual->model, reg, &a));
        TEST_ASSERT_EQUAL_INT(e.avail, a.avail);
        TEST_ASSERT_EQUAL(e.has_value, a.has_value);
        TEST_ASSERT_EQUAL_UINT16(e.raw, a.raw);
        TEST_ASSERT_EQUAL_UINT8(e.exc_code, a.exc_code);
        TEST_ASSERT_EQUAL_UINT32(e.ok_count, a.ok_count);
        TEST_ASSERT_EQUAL_UINT32(e.fail_count, a.fail_count);
        TEST_ASSERT_EQUAL_INT64(e.t_ok_ms, a.t_ok_ms);
        TEST_ASSERT_EQUAL(poll_sched_is_unsupported(&expected->sched, reg), poll_sched_is_unsupported(&actual->sched, reg));
    }
}

/* ------------------------------------------------------------ scenario runs shared by 1, 2 and 8 */

#define SCENARIO_1_2_DURATION_MS (130 * SECOND_MS)

static void run_healthy_130s(rig_t *r)
{
    rig_init(r);
    populate_synthetic(&r->fake, STARRED_EXCEPTION);
    rig_start(r);
    rig_run_until(r, SCENARIO_1_2_DURATION_MS);
}

static void run_starred_silent_130s(rig_t *r)
{
    rig_init(r);
    populate_synthetic(&r->fake, STARRED_SILENT);
    rig_start(r);
    rig_run_until(r, SCENARIO_1_2_DURATION_MS);
}

/* ------------------------------------------------------------ fixtures */

TEST_CASE("e2e fixtures: synthetic values lie in the documented ranges", "[e2e]")
{
    for (size_t i = 0; i < daikin_regs_count(); i++) {
        const dr_reg_t *r = daikin_regs_at(i);
        TEST_ASSERT_TRUE_MESSAGE(daikin_regs_in_range(r, synthetic_raw(r)), r->name);
    }
}

static void assert_fixture_matches_synthetic(const char *path, ps_res_t starred_res)
{
    FILE *fp = fopen(path, "r");
    TEST_ASSERT_NOT_NULL_MESSAGE(fp, path);
    char line[256];
    size_t i = 0;
    while (fgets(line, sizeof(line), fp) != NULL) {
        tl_rec_t rec;
        int parsed = txn_log_parse(line, &rec);
        TEST_ASSERT_NOT_EQUAL_MESSAGE(-1, parsed, line);
        if (parsed != 1) {
            continue;   /* header and comments */
        }
        TEST_ASSERT_TRUE(i < daikin_regs_count());
        const dr_reg_t *r = daikin_regs_at(i++);
        TEST_ASSERT_EQUAL_UINT16(r->reg, rec.txn.req.reg);
        TEST_ASSERT_EQUAL_UINT8(SLAVE_ADDR, rec.txn.req.addr);
        TEST_ASSERT_EQUAL_UINT16(1, rec.txn.req.qty);
        if (r->starred) {
            TEST_ASSERT_EQUAL_INT(starred_res, rec.txn.res);
            TEST_ASSERT_EQUAL_UINT8(starred_res == PS_RES_EXCEPTION ? 2 : 0, rec.txn.exc_code);
        } else {
            TEST_ASSERT_EQUAL_INT(PS_RES_OK, rec.txn.res);
            TEST_ASSERT_EQUAL_UINT16(synthetic_raw(r), rec.txn.val[0]);
        }
    }
    fclose(fp);
    TEST_ASSERT_EQUAL_UINT32(daikin_regs_count(), i);
}

TEST_CASE("e2e fixtures: the synthetic logs hold one line per register with the synthetic values", "[e2e]")
{
    assert_fixture_matches_synthetic(HEALTHY_FIXTURE, PS_RES_EXCEPTION);
    assert_fixture_matches_synthetic(STARRED_TIMEOUT_FIXTURE, PS_RES_TIMEOUT);
}

/* ------------------------------------------------------------ 1 */

TEST_CASE("e2e 1: healthy slave, starred = exception 02, 130 s", "[e2e]")
{
    run_healthy_130s(&g_a);
    size_t n;
    const ps_txn_t *j = journal_of(&g_a, &n);

    TEST_ASSERT_FALSE(g_a.ever_degraded);
    TEST_ASSERT_FALSE(g_a.ever_offline);
    TEST_ASSERT_EQUAL_INT(PS_ONLINE, poll_sched_health(&g_a.sched));

    const uint16_t starred_polled[] = { REG_STAT, REG_OUT, REG_T2 };
    for (size_t i = 0; i < sizeof(starred_polled) / sizeof(starred_polled[0]); i++) {
        TEST_ASSERT_EQUAL_UINT32(1, count_reg(j, n, starred_polled[i]));
        TEST_ASSERT_TRUE(poll_sched_is_unsupported(&g_a.sched, starred_polled[i]));
    }

    assert_gaps_respected(j, n);

    for (size_t i = 0; i < daikin_regs_count(); i++) {
        const dr_reg_t *r = daikin_regs_at(i);
        size_t c = count_reg(j, n, r->reg);
        if (r->group == DR_GROUP_NONE) {
            TEST_ASSERT_EQUAL_UINT32(0, c);   /* group NONE is never polled automatically */
        } else if (is_unstarred_polled(r) && r->group == DR_GROUP_FAST) {
            TEST_ASSERT_UINT32_WITHIN(1, 26, c);
        } else if (is_unstarred_polled(r)) {
            TEST_ASSERT_UINT32_WITHIN(1, 3, c);
        }
    }

    /* average rate <= 2 requests/s */
    TEST_ASSERT_TRUE((int64_t)n * SECOND_MS <= 2 * g_a.clock_ms);
}

/* ------------------------------------------------------------ 2 */

TEST_CASE("e2e 2: starred registers silent, 130 s", "[e2e]")
{
    run_starred_silent_130s(&g_a);
    size_t n;
    const ps_txn_t *j = journal_of(&g_a, &n);

    TEST_ASSERT_FALSE(g_a.ever_offline);

    const uint16_t starred_polled[] = { REG_STAT, REG_OUT, REG_T2 };
    for (size_t k = 0; k < sizeof(starred_polled) / sizeof(starred_polled[0]); k++) {
        uint16_t reg = starred_polled[k];
        rm_entry_t e;
        TEST_ASSERT_TRUE(reg_model_get(&g_a.model, reg, &e));
        TEST_ASSERT_EQUAL_INT(RM_TIMEOUT, e.avail);

        /* One RETRY directly after each POLL; the k-th POLL starts no earlier than k x 60 s (rule 4:
         * next due = prev due + 60 s after a final failure). */
        size_t polls = 0;
        for (size_t i = 0; i < n; i++) {
            if (j[i].req.reg != reg || j[i].req.kind != PS_KIND_POLL) {
                continue;
            }
            TEST_ASSERT_GREATER_OR_EQUAL_INT64((int64_t)polls * SLOW_MS, j[i].t_start_ms);
            polls++;
            if (i + 1 < n) {
                TEST_ASSERT_EQUAL_INT(PS_KIND_RETRY, j[i + 1].req.kind);
                TEST_ASSERT_EQUAL_UINT16(reg, j[i + 1].req.reg);
            }
        }
        TEST_ASSERT_UINT32_WITHIN(1, 2, polls);   /* 130 s: attempts near 0 s, 60 s, (120 s) */
        TEST_ASSERT_TRUE(polls <= 3);
        TEST_ASSERT_TRUE(count_reg_kind(j, n, reg, PS_KIND_RETRY) <= polls);
    }
    assert_gaps_respected(j, n);
}

/* ------------------------------------------------------------ 3 */

#define DEAD_FROM_MS      (20 * SECOND_MS)
#define DEAD_TO_MS        (200 * SECOND_MS)
#define RECOVERY_BOUND_MS (DEAD_TO_MS + PROBE_MS + RESP_TIMEOUT_MS + LATENCY_MS)   /* last failed probe ends < DEAD_TO + timeout */
#define FRESH_AFTER_RECOVERY_MS (10 * SECOND_MS)

static bool is_final_failure(const ps_txn_t *t)
{
    bool failed = t->res == PS_RES_TIMEOUT || t->res == PS_RES_BAD_FRAME || t->res == PS_RES_ERROR;
    return failed && (t->req.kind == PS_KIND_RETRY || t->req.kind == PS_KIND_HEALTH);
}

TEST_CASE("e2e 3: dead link 20 s..200 s: DEGRADED, OFFLINE, HEALTH probes, recovery", "[e2e]")
{
    rig_init(&g_a);
    populate_synthetic(&g_a.fake, STARRED_EXCEPTION);
    fake_set_dead(&g_a.fake, DEAD_FROM_MS, DEAD_TO_MS);
    rig_start(&g_a);

    unsigned final_failures = 0;
    int64_t last_probe_basis_ms = -1;   /* end of the failure that entered OFFLINE, then of each failed probe */
    int64_t online_at_ms = -1;
    while (online_at_ms < 0) {
        ps_txn_t txn;
        int64_t wait_ms;
        if (!rig_step(&g_a, &txn, &wait_ms)) {
            rig_advance(&g_a, wait_ms, INT64_MAX);
            continue;
        }
        if (final_failures >= OFFLINE_AFTER) {   /* OFFLINE: only HEALTH reads of register 000 */
            TEST_ASSERT_EQUAL_INT(PS_KIND_HEALTH, txn.req.kind);
            TEST_ASSERT_EQUAL_UINT16(REG_T1, txn.req.reg);
            TEST_ASSERT_GREATER_OR_EQUAL_INT64(last_probe_basis_ms + PROBE_MS, txn.t_start_ms);
        }
        if (txn.req.kind == PS_KIND_HEALTH && txn.res == PS_RES_OK) {
            online_at_ms = txn.t_end_ms;
            TEST_ASSERT_EQUAL_INT(PS_ONLINE, poll_sched_health(&g_a.sched));
            break;
        }
        if (is_final_failure(&txn)) {
            final_failures++;
            last_probe_basis_ms = txn.t_end_ms;
        }
        ps_health_t expected = final_failures == 0 ? PS_ONLINE
                             : final_failures < OFFLINE_AFTER ? PS_DEGRADED : PS_OFFLINE;
        TEST_ASSERT_EQUAL_INT(expected, poll_sched_health(&g_a.sched));
        TEST_ASSERT_TRUE_MESSAGE(txn.t_start_ms < RECOVERY_BOUND_MS, "no recovery within one probe period of the link coming back");
    }
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(OFFLINE_AFTER, final_failures);
    TEST_ASSERT_GREATER_OR_EQUAL_INT64(DEAD_TO_MS, online_at_ms);
    TEST_ASSERT_LESS_OR_EQUAL_INT64(RECOVERY_BOUND_MS, online_at_ms);

    rig_run_until(&g_a, online_at_ms + FRESH_AFTER_RECOVERY_MS);
    for (size_t i = 0; i < daikin_regs_count(); i++) {
        const dr_reg_t *r = daikin_regs_at(i);
        if (!is_unstarred_polled(r) || r->group != DR_GROUP_FAST) {
            continue;
        }
        rm_entry_t e;
        TEST_ASSERT_TRUE(reg_model_get(&g_a.model, r->reg, &e));
        TEST_ASSERT_GREATER_OR_EQUAL_INT64(online_at_ms, e.t_ok_ms);
        TEST_ASSERT_TRUE(reg_model_fresh(&g_a.model, r->reg, g_a.clock_ms));
    }
    size_t n;
    const ps_txn_t *j = journal_of(&g_a, &n);
    assert_gaps_respected(j, n);
}

/* ------------------------------------------------------------ 4 */

#define INTERMITTENT_EVERY_N   3
#define INTERMITTENT_DURATION_MS (300 * SECOND_MS)

TEST_CASE("e2e 4: every 3rd request times out, 300 s", "[e2e]")
{
    rig_init(&g_a);
    populate_synthetic(&g_a.fake, STARRED_EXCEPTION);
    fake_fail_every(&g_a.fake, INTERMITTENT_EVERY_N);
    rig_start(&g_a);
    rig_run_until(&g_a, INTERMITTENT_DURATION_MS);

    TEST_ASSERT_FALSE(g_a.ever_offline);
    ps_stats_t st;
    poll_sched_get_stats(&g_a.sched, &st);
    TEST_ASSERT_GREATER_THAN_UINT32(0, st.retries);
    for (size_t i = 0; i < daikin_regs_count(); i++) {
        const dr_reg_t *r = daikin_regs_at(i);
        if (is_unstarred_polled(r)) {
            TEST_ASSERT_TRUE_MESSAGE(reg_model_fresh(&g_a.model, r->reg, g_a.clock_ms), r->name);
        }
    }
    size_t n;
    const ps_txn_t *j = journal_of(&g_a, &n);
    assert_gaps_respected(j, n);
}

/* ------------------------------------------------------------ 5 */

#define MANUAL_WINDOW_MS (10 * SECOND_MS)

/* In a state without automatic traffic (STOPPED): nothing for 120 s, then one MANUAL read runs once. */
static void assert_quiet_then_single_manual(rig_t *r)
{
    size_t n0 = journal_size(r);
    rig_run_for(r, QUIET_PERIOD_MS);
    TEST_ASSERT_EQUAL_UINT32(n0, journal_size(r));

    TEST_ASSERT_EQUAL_INT(0, poll_sched_request_manual(&r->sched, REG_T1, 1));
    rig_run_for(r, MANUAL_WINDOW_MS);
    size_t n;
    const ps_txn_t *j = journal_of(r, &n);
    TEST_ASSERT_EQUAL_UINT32(n0 + 1, n);
    TEST_ASSERT_EQUAL_INT(PS_KIND_MANUAL, j[n0].req.kind);
    TEST_ASSERT_EQUAL_UINT16(REG_T1, j[n0].req.reg);
    assert_gaps_respected(j, n);

    rig_run_for(r, QUIET_PERIOD_MS);
    TEST_ASSERT_EQUAL_UINT32(n0 + 1, journal_size(r));
    TEST_ASSERT_EQUAL_INT(PS_STOPPED, poll_sched_health(&r->sched));
}

TEST_CASE("e2e 5a: STOPPED after init: no automatic request, one MANUAL read once", "[e2e]")
{
    rig_init(&g_a);
    populate_synthetic(&g_a.fake, STARRED_EXCEPTION);
    assert_quiet_then_single_manual(&g_a);
}

TEST_CASE("e2e 5b: STOPPED after poll_sched_stop: no automatic request, one MANUAL read once", "[e2e]")
{
    rig_init(&g_a);
    populate_synthetic(&g_a.fake, STARRED_EXCEPTION);
    rig_start(&g_a);
    rig_run_for(&g_a, MANUAL_WINDOW_MS);
    poll_sched_stop(&g_a.sched);
    assert_quiet_then_single_manual(&g_a);
}

static void assert_latched_silent_and_refusing(rig_t *r)
{
    poll_sched_latch(&r->sched);
    size_t n0 = journal_size(r);
    rig_run_for(r, QUIET_PERIOD_MS);
    TEST_ASSERT_EQUAL_UINT32(n0, journal_size(r));
    TEST_ASSERT_EQUAL_INT(-1, poll_sched_request_manual(&r->sched, REG_T1, 1));
    TEST_ASSERT_EQUAL_INT(-1, poll_sched_request_sweep(&r->sched));
    rig_run_for(r, MANUAL_WINDOW_MS);
    TEST_ASSERT_EQUAL_UINT32(n0, journal_size(r));
    TEST_ASSERT_EQUAL_INT(PS_LATCHED, poll_sched_health(&r->sched));
    size_t n;
    const ps_txn_t *j = journal_of(r, &n);
    assert_gaps_respected(j, n);
}

TEST_CASE("e2e 5c: LATCHED from STOPPED and from ONLINE: no request of any kind, MANUAL and SWEEP refused", "[e2e]")
{
    rig_init(&g_a);
    populate_synthetic(&g_a.fake, STARRED_EXCEPTION);
    assert_latched_silent_and_refusing(&g_a);

    rig_init(&g_b);
    populate_synthetic(&g_b.fake, STARRED_EXCEPTION);
    rig_start(&g_b);
    rig_run_for(&g_b, MANUAL_WINDOW_MS);
    TEST_ASSERT_GREATER_THAN_UINT32(0, journal_size(&g_b));
    assert_latched_silent_and_refusing(&g_b);
}

/* ------------------------------------------------------------ 6 */

#define SWEEP_WINDOW_MS (30 * SECOND_MS)

TEST_CASE("e2e 6: sweep from STOPPED reads all 32 registers once, ascending", "[e2e]")
{
    rig_init(&g_a);
    populate_synthetic(&g_a.fake, STARRED_EXCEPTION);
    TEST_ASSERT_EQUAL_INT(0, poll_sched_request_sweep(&g_a.sched));
    rig_run_for(&g_a, SWEEP_WINDOW_MS);

    size_t n;
    const ps_txn_t *j = journal_of(&g_a, &n);
    TEST_ASSERT_EQUAL_UINT32(daikin_regs_count(), n);
    assert_gaps_respected(j, n);
    TEST_ASSERT_EQUAL_INT(PS_STOPPED, poll_sched_health(&g_a.sched));

    for (size_t i = 0; i < n; i++) {
        const dr_reg_t *r = daikin_regs_at(i);
        TEST_ASSERT_EQUAL_INT(PS_KIND_SWEEP, j[i].req.kind);
        TEST_ASSERT_EQUAL_UINT16(r->reg, j[i].req.reg);
        TEST_ASSERT_EQUAL_UINT16(1, j[i].req.qty);
        TEST_ASSERT_EQUAL(r->starred, poll_sched_is_unsupported(&g_a.sched, r->reg));
        rm_entry_t e;
        TEST_ASSERT_TRUE(reg_model_get(&g_a.model, r->reg, &e));
        TEST_ASSERT_EQUAL_INT(r->starred ? RM_UNSUPPORTED : RM_OK, e.avail);
    }
}

/* ------------------------------------------------------------ 7 */

#define BEFORE_ADDR_CHANGE_MS (10 * SECOND_MS)
#define AFTER_ADDR_CHANGE_MS  (40 * SECOND_MS)
#define NEW_ADDR              2

TEST_CASE("e2e 7: address change to an absent slave: only addr 2, all TIMEOUT, OFFLINE", "[e2e]")
{
    rig_init(&g_a);
    populate_synthetic(&g_a.fake, STARRED_EXCEPTION);
    rig_start(&g_a);
    rig_run_until(&g_a, BEFORE_ADDR_CHANGE_MS);
    TEST_ASSERT_EQUAL_INT(PS_ONLINE, poll_sched_health(&g_a.sched));

    size_t before_n = journal_size(&g_a);
    TEST_ASSERT_EQUAL_INT(0, poll_sched_set_addr(&g_a.sched, NEW_ADDR, g_a.clock_ms));
    rig_run_until(&g_a, AFTER_ADDR_CHANGE_MS);

    size_t n;
    const ps_txn_t *j = journal_of(&g_a, &n);
    TEST_ASSERT_GREATER_THAN_UINT32(before_n, n);
    for (size_t i = before_n; i < n; i++) {
        TEST_ASSERT_EQUAL_UINT8(NEW_ADDR, j[i].req.addr);
        TEST_ASSERT_EQUAL_INT(PS_RES_TIMEOUT, j[i].res);
    }
    assert_gaps_respected(j, n);
    TEST_ASSERT_EQUAL_INT(PS_OFFLINE, poll_sched_health(&g_a.sched));
}

/* ------------------------------------------------------------ 8 */

static void load_replay(rig_t *r, const char *path)
{
    rig_init(r);
    TEST_ASSERT_EQUAL_INT(0, fake_load_log(&r->fake, path));
    rig_start(r);
    rig_run_until(r, SCENARIO_1_2_DURATION_MS);
}

TEST_CASE("e2e 8a: replay of healthy-synthetic.log reproduces the end state of scenario 1", "[e2e]")
{
    run_healthy_130s(&g_a);
    load_replay(&g_b, HEALTHY_FIXTURE);
    assert_same_end_state(&g_a, &g_b);
}

TEST_CASE("e2e 8b: replay of starred-timeout-synthetic.log reproduces the end state of scenario 2", "[e2e]")
{
    run_starred_silent_130s(&g_a);
    load_replay(&g_b, STARRED_TIMEOUT_FIXTURE);
    assert_same_end_state(&g_a, &g_b);
}

TEST_CASE("e2e 8c: fake_load_log rejects a malformed line and a missing file", "[e2e]")
{
    char path[] = "/tmp/dmt_e2e_bad_XXXXXX";
    int fd = mkstemp(path);
    TEST_ASSERT_TRUE(fd >= 0);
    const char bad[] = "# header\nT 0 25 1 POLL 000 1 OK\n";   /* OK without data */
    TEST_ASSERT_EQUAL_INT((int)sizeof(bad) - 1, (int)write(fd, bad, sizeof(bad) - 1));
    close(fd);

    rig_init(&g_a);
    TEST_ASSERT_EQUAL_INT(-1, fake_load_log(&g_a.fake, path));
    unlink(path);
    TEST_ASSERT_EQUAL_INT(-1, fake_load_log(&g_a.fake, path));
}

/* ------------------------------------------------------------ 9 */

#define ROUND_TRIP_DURATION_MS (70 * SECOND_MS)
#define REPLAY_COMPARE_MS      (60 * SECOND_MS)
#define ROUND_TRIP_FAIL_EVERY  7

TEST_CASE("e2e 9: log round trip and replay of a run's own log", "[e2e]")
{
    rig_init(&g_a);
    populate_synthetic(&g_a.fake, STARRED_EXCEPTION);
    fake_fail_every(&g_a.fake, ROUND_TRIP_FAIL_EVERY);
    rig_start(&g_a);
    rig_run_until(&g_a, ROUND_TRIP_DURATION_MS);

    size_t n;
    const ps_txn_t *j = journal_of(&g_a, &n);
    char path[] = "/tmp/dmt_e2e_log_XXXXXX";
    int fd = mkstemp(path);
    TEST_ASSERT_TRUE(fd >= 0);
    FILE *fp = fdopen(fd, "w");
    TEST_ASSERT_NOT_NULL(fp);
    for (size_t i = 0; i < n; i++) {
        char line[160];
        TEST_ASSERT_GREATER_THAN_UINT32(0, txn_log_format_txn(line, sizeof(line), &j[i]));
        tl_rec_t rec;
        TEST_ASSERT_EQUAL_INT(1, txn_log_parse(line, &rec));
        TEST_ASSERT_EQUAL_INT(TL_TXN, rec.type);
        assert_txn_equal(&j[i], &rec.txn);
        fprintf(fp, "%s\n", line);
    }
    fclose(fp);

    rig_init(&g_b);
    int loaded = fake_load_log(&g_b.fake, path);
    unlink(path);
    TEST_ASSERT_EQUAL_INT(0, loaded);
    rig_start(&g_b);
    rig_run_until(&g_b, ROUND_TRIP_DURATION_MS);

    size_t nb;
    const ps_txn_t *jb = journal_of(&g_b, &nb);
    size_t compared = 0;
    for (size_t i = 0; i < n && j[i].t_start_ms < REPLAY_COMPARE_MS; i++) {
        TEST_ASSERT_TRUE(i < nb);
        assert_txn_equal(&j[i], &jb[i]);
        compared++;
    }
    TEST_ASSERT_GREATER_THAN_UINT32(0, compared);
    assert_gaps_respected(j, n);
    assert_gaps_respected(jb, nb);
}

/* ------------------------------------------------------------ 10 */

static void assert_no_requests_for_quiet_period(rig_t *r)
{
    rig_run_for(r, QUIET_PERIOD_MS);
    TEST_ASSERT_EQUAL_UINT32(0, journal_size(r));
}

TEST_CASE("e2e 10a: clean boot: first request POLL 000 after the silence window, then the 010 first-cycle order", "[e2e]")
{
    rig_init(&g_a);
    populate_synthetic(&g_a.fake, STARRED_EXCEPTION);
    g_a.clock_ms += SILENCE_WINDOW_MS;
    TEST_ASSERT_EQUAL_UINT32(0, journal_size(&g_a));
    rig_start(&g_a);
    rig_run_for(&g_a, 5 * SECOND_MS);

    size_t n;
    const ps_txn_t *j = journal_of(&g_a, &n);
    TEST_ASSERT_TRUE(n >= sizeof(k_first_cycle) / sizeof(k_first_cycle[0]));
    TEST_ASSERT_EQUAL_INT64(SILENCE_WINDOW_MS, j[0].t_start_ms);
    for (size_t i = 0; i < sizeof(k_first_cycle) / sizeof(k_first_cycle[0]); i++) {
        TEST_ASSERT_EQUAL_INT(PS_KIND_POLL, j[i].req.kind);
        TEST_ASSERT_EQUAL_UINT16(k_first_cycle[i], j[i].req.reg);
    }
    assert_gaps_respected(j, n);
}

TEST_CASE("e2e 10b: failed silence check: latch instead of start: no request for 120 s", "[e2e]")
{
    rig_init(&g_a);
    populate_synthetic(&g_a.fake, STARRED_EXCEPTION);
    g_a.clock_ms += SILENCE_WINDOW_MS;
    poll_sched_latch(&g_a.sched);
    rig_start(&g_a);   /* no effect once LATCHED */
    assert_no_requests_for_quiet_period(&g_a);
    TEST_ASSERT_EQUAL_INT(PS_LATCHED, poll_sched_health(&g_a.sched));
}

TEST_CASE("e2e 10c: mb poll off during the window: stop, no start: silent; a later start polls 000 at once", "[e2e]")
{
    rig_init(&g_a);
    populate_synthetic(&g_a.fake, STARRED_EXCEPTION);
    g_a.clock_ms += SILENCE_WINDOW_MS;
    poll_sched_stop(&g_a.sched);
    assert_no_requests_for_quiet_period(&g_a);

    int64_t start_ms = g_a.clock_ms;
    rig_start(&g_a);
    rig_run_for(&g_a, SECOND_MS);
    size_t n;
    const ps_txn_t *j = journal_of(&g_a, &n);
    TEST_ASSERT_TRUE(n > 0);
    TEST_ASSERT_EQUAL_INT(PS_KIND_POLL, j[0].req.kind);
    TEST_ASSERT_EQUAL_UINT16(REG_T1, j[0].req.reg);
    TEST_ASSERT_EQUAL_INT64(start_ms, j[0].t_start_ms);
}

/* ------------------------------------------------------------ 11 */

/* Issues one request, then asks for a MANUAL read at once: it must wait out the gap (the fake
 * fails the test otherwise). */
static void assert_manual_waits_for_gap(rig_t *r)
{
    ps_txn_t txn;
    int64_t wait_ms;
    TEST_ASSERT_TRUE(rig_step(r, &txn, &wait_ms));
    TEST_ASSERT_EQUAL_INT(0, poll_sched_request_manual(&r->sched, REG_T1, 1));
    rig_run_for(r, SECOND_MS);
    size_t n;
    const ps_txn_t *j = journal_of(r, &n);
    TEST_ASSERT_GREATER_THAN_UINT32(1, n);
    assert_gaps_respected(j, n);
}

TEST_CASE("e2e 11a: MANUAL requested right after a request waits out the gap", "[e2e]")
{
    rig_init(&g_a);
    populate_synthetic(&g_a.fake, STARRED_EXCEPTION);
    rig_start(&g_a);
    assert_manual_waits_for_gap(&g_a);
}

TEST_CASE("e2e 11b: MANUAL requested right after the failure that enters OFFLINE waits out the gap", "[e2e]")
{
    rig_init(&g_a);
    populate_synthetic(&g_a.fake, STARRED_EXCEPTION);
    fake_set_dead(&g_a.fake, 0, INT64_MAX);
    rig_start(&g_a);
    while (poll_sched_health(&g_a.sched) != PS_OFFLINE) {
        ps_txn_t txn;
        int64_t wait_ms;
        if (!rig_step(&g_a, &txn, &wait_ms)) {
            rig_advance(&g_a, wait_ms, INT64_MAX);
        }
    }
    TEST_ASSERT_EQUAL_INT(0, poll_sched_request_manual(&g_a.sched, REG_T1, 1));
    rig_run_for(&g_a, SECOND_MS);
    size_t n;
    const ps_txn_t *j = journal_of(&g_a, &n);
    TEST_ASSERT_EQUAL_INT(PS_KIND_MANUAL, j[n - 1].req.kind);
    assert_gaps_respected(j, n);
}
