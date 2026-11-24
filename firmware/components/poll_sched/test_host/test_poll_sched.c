/* test_poll_sched.c - host Unity tests for poll_sched and poll_step (spec 010).
 * Fake clock = a plain int64_t; the fake slave is scripted per test.
 */

#include <string.h>
#include "unity.h"
#include "poll_sched.h"
#include "daikin_regs.h"

#define DUR 10   /* default fake transaction duration in ms */

static poll_sched_t S;
static int64_t g_now;

static const uint16_t FIRST_CYCLE[] = { 0, 8, 104, 201, 231, 233, 1, 9, 105, 202, 203, 245, 246, 247 };
#define POLLED_COUNT (sizeof(FIRST_CYCLE) / sizeof(FIRST_CYCLE[0]))

static ps_config_t base_cfg(void)
{
    ps_config_t c = { .addr = 1, .gap_ms = 100, .fast_ms = 5000, .slow_ms = 60000,
                      .offline_probe_ms = 30000, .retries = 1, .offline_after = 5 };
    return c;
}

static void setup(const ps_config_t *c)
{
    TEST_ASSERT_EQUAL(0, poll_sched_init(&S, c));
    g_now = 0;
}

static void setup_running(const ps_config_t *c)
{
    setup(c);
    poll_sched_start(&S, 0);
}

static int idx(uint16_t reg)
{
    return (int)(daikin_regs_find(reg) - daikin_regs_at(0));
}

/* Advance the fake time to the wait time until a request is available. */
static ps_req_t take(void)
{
    ps_req_t r;
    int64_t wait;
    while (!poll_sched_next(&S, g_now, &r, &wait)) {
        TEST_ASSERT_NOT_EQUAL(INT64_MAX, wait);
        TEST_ASSERT_GREATER_THAN_INT64(g_now, wait);
        g_now = wait;
    }
    return r;
}

static void finish(ps_req_t r, ps_res_t res, uint8_t exc, int64_t dur)
{
    ps_txn_t t;
    memset(&t, 0, sizeof(t));
    t.req = r;
    t.res = res;
    t.exc_code = exc;
    t.t_start_ms = g_now;
    t.t_end_ms = g_now + dur;
    g_now = t.t_end_ms;
    poll_sched_complete(&S, &t);
}

static ps_req_t take_finish(ps_res_t res, uint8_t exc)
{
    ps_req_t r = take();
    finish(r, res, exc, DUR);
    return r;
}

static void expect_req(ps_req_t r, ps_kind_t kind, uint16_t reg)
{
    TEST_ASSERT_EQUAL(kind, r.kind);
    TEST_ASSERT_EQUAL_UINT16(reg, r.reg);
}

/* Idle = nothing to do even after the gap has passed. */
static bool next_is_idle(void)
{
    ps_req_t r;
    int64_t wait = 0;
    g_now += 1000;
    return !poll_sched_next(&S, g_now, &r, &wait) && wait == INT64_MAX;
}

/* Drive failing polls (retries 0 config) until the given health is reached. */
static void fail_polls(int count)
{
    for (int i = 0; i < count; i++) {
        take_finish(PS_RES_TIMEOUT, 0);
    }
}

/* ------------------------------------------------------------ init / start */

TEST_CASE("poll_sched: init rejects values outside the Kconfig ranges", "[poll_sched]")
{
    ps_config_t c = base_cfg();
    TEST_ASSERT_EQUAL(0, poll_sched_init(&S, &c));

    ps_config_t lo[] = { base_cfg(), base_cfg(), base_cfg(), base_cfg(), base_cfg(), base_cfg(), base_cfg() };
    lo[0].addr = 0; lo[1].gap_ms = 49; lo[2].fast_ms = 1999; lo[3].slow_ms = 9999;
    lo[4].retries = 2; lo[5].offline_after = 1; lo[6].offline_probe_ms = 9999;
    ps_config_t hi[] = { base_cfg(), base_cfg(), base_cfg(), base_cfg(), base_cfg(), base_cfg() };
    hi[0].addr = 248; hi[1].gap_ms = 1001; hi[2].fast_ms = 60001; hi[3].slow_ms = 3600001;
    hi[4].offline_after = 21; hi[5].offline_probe_ms = 600001;
    for (size_t i = 0; i < 7; i++) {
        TEST_ASSERT_EQUAL_MESSAGE(-1, poll_sched_init(&S, &lo[i]), "below range");
    }
    for (size_t i = 0; i < 6; i++) {
        TEST_ASSERT_EQUAL_MESSAGE(-1, poll_sched_init(&S, &hi[i]), "above range");
    }
    c.fast_ms = 20000; c.slow_ms = 10000;   /* both in range, slow < fast */
    TEST_ASSERT_EQUAL(-1, poll_sched_init(&S, &c));
    c = base_cfg(); c.addr = 247; c.gap_ms = 1000; c.fast_ms = 2000; c.slow_ms = 3600000;
    c.retries = 0; c.offline_after = 20; c.offline_probe_ms = 600000;
    TEST_ASSERT_EQUAL(0, poll_sched_init(&S, &c));   /* boundaries accepted */
}

TEST_CASE("poll_sched: init state is STOPPED and idle", "[poll_sched]")
{
    ps_config_t c = base_cfg();
    setup(&c);
    TEST_ASSERT_EQUAL(PS_STOPPED, poll_sched_health(&S));
    TEST_ASSERT_TRUE(next_is_idle());
}

TEST_CASE("poll_sched: start right after init gives POLL 000 at now", "[poll_sched]")
{
    ps_config_t c = base_cfg();
    setup(&c);
    poll_sched_start(&S, 4321);
    ps_req_t r;
    int64_t wait;
    TEST_ASSERT_TRUE(poll_sched_next(&S, 4321, &r, &wait));
    expect_req(r, PS_KIND_POLL, 0);
    TEST_ASSERT_EQUAL(PS_ONLINE, poll_sched_health(&S));
}

TEST_CASE("poll_sched: first cycle order (FAST then SLOW, ascending)", "[poll_sched]")
{
    ps_config_t c = base_cfg();
    setup_running(&c);
    for (size_t i = 0; i < POLLED_COUNT; i++) {
        ps_req_t r = take_finish(PS_RES_OK, 0);
        expect_req(r, PS_KIND_POLL, FIRST_CYCLE[i]);
    }
}

/* ------------------------------------------------------------ rules 1-11 */

TEST_CASE("poll_sched rule 1: only FAST/SLOW regs, qty 1, configured addr", "[poll_sched]")
{
    ps_config_t c = base_cfg();
    c.addr = 17;
    setup_running(&c);
    bool seen[RM_REG_COUNT] = { false };
    while (g_now < 130000) {
        ps_req_t r = take_finish(PS_RES_OK, 0);
        TEST_ASSERT_EQUAL(PS_KIND_POLL, r.kind);
        TEST_ASSERT_EQUAL_UINT16(1, r.qty);
        TEST_ASSERT_EQUAL_UINT8(17, r.addr);
        const dr_reg_t *d = daikin_regs_find(r.reg);
        TEST_ASSERT_NOT_NULL(d);
        TEST_ASSERT_NOT_EQUAL(DR_GROUP_NONE, d->group);
        seen[idx(r.reg)] = true;
    }
    for (size_t i = 0; i < POLLED_COUNT; i++) {
        TEST_ASSERT_TRUE(seen[idx(FIRST_CYCLE[i])]);
    }
}

TEST_CASE("poll_sched rule 2: exact gap waits", "[poll_sched]")
{
    ps_config_t c = base_cfg();
    setup_running(&c);
    ps_req_t r;
    int64_t wait = 0;
    TEST_ASSERT_TRUE(poll_sched_next(&S, 0, &r, &wait));
    finish(r, PS_RES_OK, 0, 24);            /* ends at 24 */
    TEST_ASSERT_FALSE(poll_sched_next(&S, 24, &r, &wait));
    TEST_ASSERT_EQUAL_INT64(124, wait);
    TEST_ASSERT_FALSE(poll_sched_next(&S, 123, &r, &wait));
    TEST_ASSERT_EQUAL_INT64(124, wait);
    TEST_ASSERT_TRUE(poll_sched_next(&S, 124, &r, &wait));
    /* the gap also separates a failure from its retry */
    finish(r, PS_RES_TIMEOUT, 0, 500);      /* start 24 (g_now), ends 524 */
    TEST_ASSERT_FALSE(poll_sched_next(&S, 600, &r, &wait));
    TEST_ASSERT_EQUAL_INT64(624, wait);
    /* a gap applies to manual requests too */
    poll_sched_request_manual(&S, 15, 1);
    TEST_ASSERT_FALSE(poll_sched_next(&S, 600, &r, &wait));
    TEST_ASSERT_EQUAL_INT64(624, wait);
}

TEST_CASE("poll_sched rule 3: MANUAL > RETRY > POLL > SWEEP", "[poll_sched]")
{
    ps_config_t c = base_cfg();
    setup_running(&c);
    expect_req(take_finish(PS_RES_TIMEOUT, 0), PS_KIND_POLL, 0);   /* retry of 000 pending */
    poll_sched_request_sweep(&S);
    TEST_ASSERT_EQUAL(0, poll_sched_request_manual(&S, 15, 1));
    expect_req(take_finish(PS_RES_OK, 0), PS_KIND_MANUAL, 15);
    expect_req(take_finish(PS_RES_OK, 0), PS_KIND_RETRY, 0);
    expect_req(take_finish(PS_RES_OK, 0), PS_KIND_POLL, 8);        /* due POLL before SWEEP */
}

TEST_CASE("poll_sched rule 3: among due POLLs FAST first, then earliest due, then lowest reg", "[poll_sched]")
{
    ps_config_t c = base_cfg();
    setup_running(&c);
    g_now = 1000;
    for (int i = 0; i < RM_REG_COUNT; i++) {
        S.due_ms[i] = 900;
    }
    S.due_ms[idx(1)] = 10;      /* SLOW, much earlier: still after every FAST */
    S.due_ms[idx(8)] = 100;     /* FAST, earliest */
    S.due_ms[idx(104)] = 100;   /* tie with 008: lowest reg wins */
    expect_req(take_finish(PS_RES_OK, 0), PS_KIND_POLL, 8);
    expect_req(take_finish(PS_RES_OK, 0), PS_KIND_POLL, 104);
    expect_req(take_finish(PS_RES_OK, 0), PS_KIND_POLL, 0);        /* due 900, lowest FAST reg */
    for (int i = 0; i < 3; i++) {                                  /* 201, 231, 233 */
        take_finish(PS_RES_OK, 0);
    }
    expect_req(take_finish(PS_RES_OK, 0), PS_KIND_POLL, 1);        /* SLOW only now */
}

TEST_CASE("poll_sched rule 4: next due keeps the grid, no drift", "[poll_sched]")
{
    ps_config_t c = base_cfg();
    setup_running(&c);
    for (size_t i = 0; i < POLLED_COUNT; i++) {
        take_finish(PS_RES_OK, 0);
    }
    TEST_ASSERT_EQUAL_INT64(5000, S.due_ms[idx(0)]);                /* start + 5000, not t_end + 5000 */
    TEST_ASSERT_EQUAL_INT64(60000, S.due_ms[idx(1)]);
    ps_req_t r = take();                                            /* idle until 5000 */
    expect_req(r, PS_KIND_POLL, 0);
    TEST_ASSERT_EQUAL_INT64(5000, g_now);
    finish(r, PS_RES_OK, 0, 400);                                   /* late finish */
    TEST_ASSERT_EQUAL_INT64(10000, S.due_ms[idx(0)]);
}

TEST_CASE("poll_sched rule 4: due is at least t_end; failures back off by slow_ms", "[poll_sched]")
{
    ps_config_t c = base_cfg();
    c.fast_ms = 2000;
    c.retries = 0;
    setup_running(&c);
    finish(take(), PS_RES_OK, 0, 3000);                             /* longer than the period */
    TEST_ASSERT_EQUAL_INT64(3000, S.due_ms[idx(0)]);
    take_finish(PS_RES_TIMEOUT, 0);                                 /* 008 */
    TEST_ASSERT_EQUAL_INT64(60000, S.due_ms[idx(8)]);
    take_finish(PS_RES_ERROR, 0);                                   /* 104 */
    TEST_ASSERT_EQUAL_INT64(60000, S.due_ms[idx(104)]);
    take_finish(PS_RES_BAD_FRAME, 0);                               /* 201 */
    TEST_ASSERT_EQUAL_INT64(60000, S.due_ms[idx(201)]);
}

TEST_CASE("poll_sched rule 5: retry once on timeout, bad frame and error; never on exception", "[poll_sched]")
{
    ps_res_t fails[] = { PS_RES_TIMEOUT, PS_RES_BAD_FRAME, PS_RES_ERROR };
    for (size_t i = 0; i < 3; i++) {
        ps_config_t c = base_cfg();
        setup_running(&c);
        expect_req(take_finish(fails[i], 0), PS_KIND_POLL, 0);
        expect_req(take_finish(fails[i], 0), PS_KIND_RETRY, 0);
        expect_req(take_finish(PS_RES_OK, 0), PS_KIND_POLL, 8);      /* only one retry */
    }
    ps_config_t c = base_cfg();
    setup_running(&c);
    take_finish(PS_RES_EXCEPTION, 3);
    expect_req(take_finish(PS_RES_OK, 0), PS_KIND_POLL, 8);          /* no retry after exception */

    c.retries = 0;
    setup_running(&c);
    take_finish(PS_RES_TIMEOUT, 0);
    expect_req(take_finish(PS_RES_OK, 0), PS_KIND_POLL, 8);          /* retries = 0: none */
}

TEST_CASE("poll_sched rule 5: successful retry resets due like a poll", "[poll_sched]")
{
    ps_config_t c = base_cfg();
    setup_running(&c);
    take_finish(PS_RES_TIMEOUT, 0);
    take_finish(PS_RES_OK, 0);
    TEST_ASSERT_EQUAL_INT64(5000, S.due_ms[idx(0)]);
    TEST_ASSERT_EQUAL(PS_ONLINE, poll_sched_health(&S));
}

TEST_CASE("poll_sched rule 6: exception 02 marks unsupported on POLL, RETRY, SWEEP, MANUAL qty 1", "[poll_sched]")
{
    ps_config_t c = base_cfg();
    setup_running(&c);
    take_finish(PS_RES_OK, 0);                                       /* 000 */
    take_finish(PS_RES_OK, 0);                                       /* 008 */
    expect_req(take_finish(PS_RES_EXCEPTION, 2), PS_KIND_POLL, 104);
    TEST_ASSERT_TRUE(poll_sched_is_unsupported(&S, 104));
    TEST_ASSERT_EQUAL(PS_ONLINE, poll_sched_health(&S));
    while (g_now < 130000) {                                         /* never polled again */
        ps_req_t r = take_finish(PS_RES_OK, 0);
        TEST_ASSERT_NOT_EQUAL(104, r.reg);
    }

    setup(&c);
    poll_sched_request_sweep(&S);
    for (int i = 0; i < 32; i++) {
        ps_req_t r = take();
        finish(r, r.reg == 15 ? PS_RES_EXCEPTION : PS_RES_OK, 2, DUR);
    }
    TEST_ASSERT_TRUE(poll_sched_is_unsupported(&S, 15));
    TEST_ASSERT_FALSE(poll_sched_is_unsupported(&S, 0));

    poll_sched_request_manual(&S, 209, 1);
    take_finish(PS_RES_EXCEPTION, 2);
    TEST_ASSERT_TRUE(poll_sched_is_unsupported(&S, 209));
    poll_sched_request_manual(&S, 242, 2);                           /* qty 2: not marked */
    take_finish(PS_RES_EXCEPTION, 2);
    TEST_ASSERT_FALSE(poll_sched_is_unsupported(&S, 242));
    TEST_ASSERT_FALSE(poll_sched_is_unsupported(&S, 243));
    TEST_ASSERT_FALSE(poll_sched_is_unsupported(&S, 2));             /* undocumented */
}

TEST_CASE("poll_sched rule 6: other exception codes are not unsupported and not counted", "[poll_sched]")
{
    ps_config_t c = base_cfg();
    c.retries = 0;
    setup_running(&c);
    for (int i = 0; i < 10; i++) {
        take_finish(PS_RES_EXCEPTION, 3);
    }
    TEST_ASSERT_EQUAL(PS_ONLINE, poll_sched_health(&S));
    TEST_ASSERT_FALSE(poll_sched_is_unsupported(&S, 0));
    TEST_ASSERT_EQUAL_INT64(60000, S.due_ms[idx(0)]);                /* backoff as rule 4 */
}

TEST_CASE("poll_sched rule 7: ONLINE -> DEGRADED (1-4) -> OFFLINE at the 5th failure", "[poll_sched]")
{
    ps_config_t c = base_cfg();
    c.retries = 0;
    setup_running(&c);
    TEST_ASSERT_EQUAL(PS_ONLINE, poll_sched_health(&S));
    for (int i = 1; i <= 4; i++) {
        take_finish(PS_RES_TIMEOUT, 0);
        TEST_ASSERT_EQUAL(PS_DEGRADED, poll_sched_health(&S));
    }
    take_finish(PS_RES_BAD_FRAME, 0);
    TEST_ASSERT_EQUAL(PS_OFFLINE, poll_sched_health(&S));
    ps_stats_t st;
    poll_sched_get_stats(&S, &st);
    TEST_ASSERT_EQUAL_UINT32(1, st.offline_entries);
}

TEST_CASE("poll_sched rule 7: DEGRADED -> ONLINE on OK or exception; counter counts final failures only", "[poll_sched]")
{
    ps_config_t c = base_cfg();                                     /* retries 1 */
    setup_running(&c);
    take_finish(PS_RES_TIMEOUT, 0);
    TEST_ASSERT_EQUAL(PS_ONLINE, poll_sched_health(&S));            /* first try only: not final */
    take_finish(PS_RES_TIMEOUT, 0);                                  /* retry fails: counted once */
    TEST_ASSERT_EQUAL(PS_DEGRADED, poll_sched_health(&S));
    take_finish(PS_RES_OK, 0);
    TEST_ASSERT_EQUAL(PS_ONLINE, poll_sched_health(&S));

    take_finish(PS_RES_TIMEOUT, 0);
    take_finish(PS_RES_TIMEOUT, 0);
    TEST_ASSERT_EQUAL(PS_DEGRADED, poll_sched_health(&S));
    take_finish(PS_RES_EXCEPTION, 3);
    TEST_ASSERT_EQUAL(PS_ONLINE, poll_sched_health(&S));
}

TEST_CASE("poll_sched rule 8: OFFLINE probes reg 000 every 30 s, OK -> ONLINE with all due", "[poll_sched]")
{
    ps_config_t c = base_cfg();
    c.retries = 0;
    setup_running(&c);
    fail_polls(5);
    TEST_ASSERT_EQUAL(PS_OFFLINE, poll_sched_health(&S));
    int64_t t_end = g_now;
    ps_req_t r;
    int64_t wait;
    TEST_ASSERT_FALSE(poll_sched_next(&S, t_end + 100, &r, &wait));
    TEST_ASSERT_EQUAL_INT64(t_end + 30000, wait);                   /* first HEALTH at t_end + probe */

    r = take();
    expect_req(r, PS_KIND_HEALTH, 0);
    TEST_ASSERT_EQUAL_UINT16(1, r.qty);
    TEST_ASSERT_EQUAL_INT64(t_end + 30000, g_now);
    finish(r, PS_RES_TIMEOUT, 0, 500);
    TEST_ASSERT_EQUAL(PS_OFFLINE, poll_sched_health(&S));
    int64_t t_end2 = g_now;
    TEST_ASSERT_FALSE(poll_sched_next(&S, t_end2 + 100, &r, &wait));
    TEST_ASSERT_EQUAL_INT64(t_end2 + 30000, wait);                  /* after the previous one ended */

    r = take();
    expect_req(r, PS_KIND_HEALTH, 0);
    finish(r, PS_RES_OK, 0, DUR);
    TEST_ASSERT_EQUAL(PS_ONLINE, poll_sched_health(&S));
    int64_t t_ok = g_now;
    r = take();                                                      /* everything is due at t_end */
    expect_req(r, PS_KIND_POLL, 0);
    TEST_ASSERT_EQUAL_INT64(t_ok + 100, g_now);
    for (size_t i = 1; i < POLLED_COUNT; i++) {
        finish(r, PS_RES_OK, 0, DUR);
        r = take();
        expect_req(r, PS_KIND_POLL, FIRST_CYCLE[i]);
    }
}

TEST_CASE("poll_sched rule 8: HEALTH exception also leaves OFFLINE; no POLL while OFFLINE", "[poll_sched]")
{
    ps_config_t c = base_cfg();
    c.retries = 0;
    setup_running(&c);
    fail_polls(5);
    for (int i = 0; i < 2; i++) {
        expect_req(take_finish(PS_RES_TIMEOUT, 0), PS_KIND_HEALTH, 0);   /* never a POLL */
    }
    expect_req(take_finish(PS_RES_EXCEPTION, 3), PS_KIND_HEALTH, 0);
    TEST_ASSERT_EQUAL(PS_ONLINE, poll_sched_health(&S));
}

TEST_CASE("poll_sched rule 9: STOPPED allows MANUAL and SWEEP only", "[poll_sched]")
{
    ps_config_t c = base_cfg();
    setup(&c);
    TEST_ASSERT_EQUAL(0, poll_sched_request_manual(&S, 0, 1));
    expect_req(take_finish(PS_RES_OK, 0), PS_KIND_MANUAL, 0);
    TEST_ASSERT_TRUE(next_is_idle());                                /* no POLL */
    TEST_ASSERT_EQUAL(0, poll_sched_request_sweep(&S));
    expect_req(take_finish(PS_RES_OK, 0), PS_KIND_SWEEP, 0);
    TEST_ASSERT_EQUAL(PS_STOPPED, poll_sched_health(&S));
}

TEST_CASE("poll_sched rule 9: stop drops RETRY and SWEEP, keeps MANUAL", "[poll_sched]")
{
    ps_config_t c = base_cfg();
    setup_running(&c);
    take_finish(PS_RES_TIMEOUT, 0);                                  /* retry pending */
    poll_sched_request_sweep(&S);
    poll_sched_request_manual(&S, 15, 1);
    poll_sched_stop(&S);
    TEST_ASSERT_EQUAL(PS_STOPPED, poll_sched_health(&S));
    expect_req(take_finish(PS_RES_OK, 0), PS_KIND_MANUAL, 15);
    TEST_ASSERT_TRUE(next_is_idle());
    TEST_ASSERT_EQUAL(0, poll_sched_request_sweep(&S));              /* old sweep is gone */
}

TEST_CASE("poll_sched rule 9: LATCHED from every state", "[poll_sched]")
{
    for (int variant = 0; variant < 4; variant++) {
        ps_config_t c = base_cfg();
        c.retries = 0;
        setup(&c);
        if (variant >= 1) {
            poll_sched_start(&S, 0);
        }
        if (variant == 2) {
            fail_polls(1);
            TEST_ASSERT_EQUAL(PS_DEGRADED, poll_sched_health(&S));
        }
        if (variant == 3) {
            fail_polls(5);
            TEST_ASSERT_EQUAL(PS_OFFLINE, poll_sched_health(&S));
        }
        poll_sched_latch(&S);
        TEST_ASSERT_EQUAL(PS_LATCHED, poll_sched_health(&S));
        TEST_ASSERT_TRUE(next_is_idle());
        TEST_ASSERT_EQUAL(-1, poll_sched_request_manual(&S, 0, 1));
        TEST_ASSERT_EQUAL(-1, poll_sched_request_sweep(&S));
        poll_sched_start(&S, g_now);
        TEST_ASSERT_EQUAL(PS_LATCHED, poll_sched_health(&S));
        poll_sched_stop(&S);
        TEST_ASSERT_EQUAL(PS_LATCHED, poll_sched_health(&S));
        g_now += 1000000;
        TEST_ASSERT_TRUE(next_is_idle());
    }
}

TEST_CASE("poll_sched rule 10: sweep reads all 32 once, ascending, no retry, no health effect", "[poll_sched]")
{
    ps_config_t c = base_cfg();
    setup(&c);
    TEST_ASSERT_EQUAL(0, poll_sched_request_sweep(&S));
    TEST_ASSERT_EQUAL(-1, poll_sched_request_sweep(&S));            /* running */
    for (size_t i = 0; i < daikin_regs_count(); i++) {
        ps_req_t r = take();
        expect_req(r, PS_KIND_SWEEP, daikin_regs_at(i)->reg);
        TEST_ASSERT_EQUAL_UINT16(1, r.qty);
        TEST_ASSERT_EQUAL_UINT8(1, r.addr);
        finish(r, i % 2 ? PS_RES_TIMEOUT : PS_RES_OK, 0, DUR);     /* failures: no retry */
    }
    TEST_ASSERT_TRUE(next_is_idle());
    TEST_ASSERT_EQUAL(PS_STOPPED, poll_sched_health(&S));
    TEST_ASSERT_EQUAL(0, poll_sched_request_sweep(&S));             /* a new one is possible */
}

TEST_CASE("poll_sched rule 10: sweep runs behind due POLLs and never repeats a reg", "[poll_sched]")
{
    ps_config_t c = base_cfg();
    setup_running(&c);
    poll_sched_request_sweep(&S);
    for (size_t i = 0; i < POLLED_COUNT; i++) {
        expect_req(take_finish(PS_RES_OK, 0), PS_KIND_POLL, FIRST_CYCLE[i]);
    }
    int sweeps = 0;
    int64_t next_reg_floor = -1;
    while (sweeps < 32) {
        ps_req_t r = take_finish(PS_RES_OK, 0);
        if (r.kind == PS_KIND_SWEEP) {
            TEST_ASSERT_GREATER_THAN_INT64(next_reg_floor, r.reg);
            next_reg_floor = r.reg;
            sweeps++;
        }
    }
    TEST_ASSERT_EQUAL(247, next_reg_floor);
    TEST_ASSERT_TRUE(g_now < 20000);
    for (int i = 0; i < 20; i++) {
        TEST_ASSERT_NOT_EQUAL(PS_KIND_SWEEP, take_finish(PS_RES_OK, 0).kind);
    }
}

TEST_CASE("poll_sched rule 8/10: sweep suspended in OFFLINE, resumes after recovery", "[poll_sched]")
{
    ps_config_t c = base_cfg();
    c.retries = 0;
    setup_running(&c);
    fail_polls(5);
    TEST_ASSERT_EQUAL(0, poll_sched_request_sweep(&S));
    for (int i = 0; i < 3; i++) {
        expect_req(take_finish(PS_RES_TIMEOUT, 0), PS_KIND_HEALTH, 0);
    }
    expect_req(take_finish(PS_RES_OK, 0), PS_KIND_HEALTH, 0);
    TEST_ASSERT_EQUAL(PS_ONLINE, poll_sched_health(&S));
    for (size_t i = 0; i < POLLED_COUNT; i++) {
        expect_req(take_finish(PS_RES_OK, 0), PS_KIND_POLL, FIRST_CYCLE[i]);
    }
    expect_req(take_finish(PS_RES_OK, 0), PS_KIND_SWEEP, 0);
}

TEST_CASE("poll_sched rule 10: sweep and manual failures do not count", "[poll_sched]")
{
    ps_config_t c = base_cfg();
    c.retries = 0;
    setup_running(&c);
    poll_sched_request_sweep(&S);
    for (size_t i = 0; i < POLLED_COUNT; i++) {
        take_finish(PS_RES_OK, 0);
    }
    for (int i = 0; i < 10; i++) {
        expect_req(take_finish(PS_RES_TIMEOUT, 0), PS_KIND_SWEEP, daikin_regs_at((size_t)i)->reg);
    }
    TEST_ASSERT_EQUAL(PS_ONLINE, poll_sched_health(&S));
    poll_sched_request_manual(&S, 0, 1);
    expect_req(take_finish(PS_RES_TIMEOUT, 0), PS_KIND_MANUAL, 0);
    TEST_ASSERT_EQUAL(PS_ONLINE, poll_sched_health(&S));
    TEST_ASSERT_EQUAL_INT64(5000, S.due_ms[idx(0)]);                /* schedule untouched */
}

TEST_CASE("poll_sched: manual validation and single pending slot", "[poll_sched]")
{
    ps_config_t c = base_cfg();
    setup(&c);
    TEST_ASSERT_EQUAL(-1, poll_sched_request_manual(&S, 2, 1));     /* undocumented */
    TEST_ASSERT_EQUAL(-1, poll_sched_request_manual(&S, 0, 0));
    TEST_ASSERT_EQUAL(-1, poll_sched_request_manual(&S, 242, 5));
    TEST_ASSERT_EQUAL(-1, poll_sched_request_manual(&S, 247, 2));   /* 248 undocumented */
    TEST_ASSERT_EQUAL(-1, poll_sched_request_manual(&S, 0, 3));     /* 002 undocumented */
    TEST_ASSERT_EQUAL(0, poll_sched_request_manual(&S, 242, 4));    /* 242..245 */
    TEST_ASSERT_EQUAL(-1, poll_sched_request_manual(&S, 0, 1));     /* one pending */
    ps_req_t r = take();
    expect_req(r, PS_KIND_MANUAL, 242);
    TEST_ASSERT_EQUAL_UINT16(4, r.qty);
    finish(r, PS_RES_TIMEOUT, 0, DUR);
    TEST_ASSERT_TRUE(next_is_idle());                               /* once, no retry */
    TEST_ASSERT_EQUAL(0, poll_sched_request_manual(&S, 0, 1));      /* free again */
}

TEST_CASE("poll_sched: manual works while OFFLINE and keeps the health probe schedule", "[poll_sched]")
{
    ps_config_t c = base_cfg();
    c.retries = 0;
    setup_running(&c);
    fail_polls(5);
    int64_t t_end = g_now;
    poll_sched_request_manual(&S, 15, 1);
    expect_req(take_finish(PS_RES_OK, 0), PS_KIND_MANUAL, 15);
    TEST_ASSERT_EQUAL(PS_OFFLINE, poll_sched_health(&S));
    ps_req_t r = take();
    expect_req(r, PS_KIND_HEALTH, 0);
    TEST_ASSERT_EQUAL_INT64(t_end + 30000, g_now);
}

TEST_CASE("poll_sched: set_addr applies to the next request and clears state", "[poll_sched]")
{
    ps_config_t c = base_cfg();
    c.retries = 0;
    setup_running(&c);
    take_finish(PS_RES_OK, 0);
    take_finish(PS_RES_OK, 0);
    take_finish(PS_RES_EXCEPTION, 2);                               /* 104 unsupported */
    take_finish(PS_RES_TIMEOUT, 0);                                 /* 201: counter 1, DEGRADED */
    poll_sched_request_manual(&S, 0, 1);
    poll_sched_request_sweep(&S);
    TEST_ASSERT_EQUAL(PS_DEGRADED, poll_sched_health(&S));

    TEST_ASSERT_EQUAL(-1, poll_sched_set_addr(&S, 0, 5000));
    TEST_ASSERT_EQUAL(-1, poll_sched_set_addr(&S, 248, 5000));
    TEST_ASSERT_TRUE(poll_sched_is_unsupported(&S, 104));
    TEST_ASSERT_EQUAL(0, poll_sched_set_addr(&S, 247, 5000));
    TEST_ASSERT_FALSE(poll_sched_is_unsupported(&S, 104));
    TEST_ASSERT_EQUAL(PS_ONLINE, poll_sched_health(&S));
    g_now = 5000;
    ps_req_t r = take();
    expect_req(r, PS_KIND_POLL, 0);                                 /* pending manual/sweep gone, all due */
    TEST_ASSERT_EQUAL_UINT8(247, r.addr);
    finish(r, PS_RES_OK, 0, DUR);
    for (size_t i = 1; i < 3; i++) {
        take_finish(PS_RES_OK, 0);
    }
    expect_req(take_finish(PS_RES_OK, 0), PS_KIND_POLL, 201);       /* 104 polled again: next after 008 */
}

TEST_CASE("poll_sched: set_addr keeps STOPPED", "[poll_sched]")
{
    ps_config_t c = base_cfg();
    setup(&c);
    TEST_ASSERT_EQUAL(0, poll_sched_set_addr(&S, 9, 0));
    TEST_ASSERT_EQUAL(PS_STOPPED, poll_sched_health(&S));
    poll_sched_request_manual(&S, 0, 1);
    TEST_ASSERT_EQUAL_UINT8(9, take().addr);
}

/* ------------------------------------------------------------ rule 11 statistics */

static void feed(ps_kind_t kind, ps_res_t res, int64_t dur)
{
    ps_txn_t t;
    memset(&t, 0, sizeof(t));
    t.req.kind = kind;
    t.req.reg = 0;
    t.req.qty = 1;
    t.res = res;
    t.t_start_ms = g_now;
    t.t_end_ms = g_now + dur;
    g_now += dur + 1000;
    poll_sched_complete(&S, &t);
}

TEST_CASE("poll_sched rule 11: counters per result and kind", "[poll_sched]")
{
    ps_config_t c = base_cfg();
    setup(&c);
    ps_stats_t st;
    poll_sched_get_stats(&S, &st);
    TEST_ASSERT_EQUAL_UINT32(0, st.txns);
    TEST_ASSERT_EQUAL_UINT32(0, st.lat_min_ms);
    TEST_ASSERT_EQUAL_UINT32(0, st.lat_max_ms);
    TEST_ASSERT_EQUAL_UINT32(0, poll_sched_lat_median_ms(&st));

    feed(PS_KIND_SWEEP, PS_RES_OK, 5);
    feed(PS_KIND_SWEEP, PS_RES_TIMEOUT, 500);
    feed(PS_KIND_MANUAL, PS_RES_EXCEPTION, 5);
    feed(PS_KIND_MANUAL, PS_RES_BAD_FRAME, 5);
    feed(PS_KIND_MANUAL, PS_RES_ERROR, 5);
    feed(PS_KIND_RETRY, PS_RES_OK, 5);       /* ignored by the scheduler while STOPPED, counted here */
    feed(PS_KIND_HEALTH, PS_RES_OK, 5);
    poll_sched_get_stats(&S, &st);
    TEST_ASSERT_EQUAL_UINT32(7, st.txns);
    TEST_ASSERT_EQUAL_UINT32(3, st.ok);
    TEST_ASSERT_EQUAL_UINT32(1, st.timeout);
    TEST_ASSERT_EQUAL_UINT32(1, st.exception);
    TEST_ASSERT_EQUAL_UINT32(1, st.bad_frame);
    TEST_ASSERT_EQUAL_UINT32(1, st.error);
    TEST_ASSERT_EQUAL_UINT32(1, st.retries);
    TEST_ASSERT_EQUAL_UINT32(1, st.health_probes);
    TEST_ASSERT_EQUAL_UINT32(2, st.sweeps);
    TEST_ASSERT_EQUAL_UINT32(3, st.manual);
    TEST_ASSERT_EQUAL_UINT32(0, st.offline_entries);
}

TEST_CASE("poll_sched rule 11: histogram only for answered transactions, clamp, min/max, median", "[poll_sched]")
{
    ps_config_t c = base_cfg();
    setup(&c);
    ps_stats_t st;
    feed(PS_KIND_MANUAL, PS_RES_OK, 9);                  /* bucket 0 */
    feed(PS_KIND_MANUAL, PS_RES_OK, 10);                 /* bucket 1 */
    feed(PS_KIND_MANUAL, PS_RES_EXCEPTION, 25);          /* bucket 2 */
    feed(PS_KIND_MANUAL, PS_RES_TIMEOUT, 500);           /* not answered: no sample */
    feed(PS_KIND_MANUAL, PS_RES_OK, 2500);               /* clamped to the last bucket */
    poll_sched_get_stats(&S, &st);
    TEST_ASSERT_EQUAL_UINT32(1, st.lat_hist[0]);
    TEST_ASSERT_EQUAL_UINT32(1, st.lat_hist[1]);
    TEST_ASSERT_EQUAL_UINT32(1, st.lat_hist[2]);
    TEST_ASSERT_EQUAL_UINT32(1, st.lat_hist[PS_LAT_BUCKETS - 1]);
    TEST_ASSERT_EQUAL_UINT32(0, st.lat_hist[50]);
    TEST_ASSERT_EQUAL_UINT32(9, st.lat_min_ms);
    TEST_ASSERT_EQUAL_UINT32(2500, st.lat_max_ms);
    TEST_ASSERT_EQUAL_UINT32(20, poll_sched_lat_median_ms(&st));   /* 4 samples: 2nd is in bucket 1 */
    feed(PS_KIND_MANUAL, PS_RES_OK, 2000);               /* last bucket boundary */
    feed(PS_KIND_MANUAL, PS_RES_OK, 1999);
    poll_sched_get_stats(&S, &st);
    TEST_ASSERT_EQUAL_UINT32(1, st.lat_hist[199]);
    TEST_ASSERT_EQUAL_UINT32(2, st.lat_hist[200]);
    TEST_ASSERT_EQUAL_UINT32(30, poll_sched_lat_median_ms(&st));   /* 6 samples: 3rd is bucket 2 */
}

TEST_CASE("poll_sched rule 11: offline entries and retries counted from the schedule", "[poll_sched]")
{
    ps_config_t c = base_cfg();
    setup_running(&c);
    take_finish(PS_RES_TIMEOUT, 0);
    take_finish(PS_RES_OK, 0);
    ps_stats_t st;
    poll_sched_get_stats(&S, &st);
    TEST_ASSERT_EQUAL_UINT32(1, st.retries);
    TEST_ASSERT_EQUAL_UINT32(2, st.txns);
}

/* ------------------------------------------------------------ poll_step */

typedef struct {
    int64_t clock_ms;
    int64_t dur_ms;
    ps_res_t res;
    uint8_t exc;
    uint16_t val[4];
    uint8_t last_addr;
    uint16_t last_reg, last_qty;
    int calls;
} fake_t;

static ps_res_t fake_link(void *ctx, uint8_t addr, uint16_t reg, uint16_t qty, uint16_t *val, uint8_t *exc)
{
    fake_t *f = ctx;
    f->calls++;
    f->last_addr = addr;
    f->last_reg = reg;
    f->last_qty = qty;
    f->clock_ms += f->dur_ms;
    if (f->res == PS_RES_OK) {
        memcpy(val, f->val, sizeof(f->val));
    } else if (f->res == PS_RES_EXCEPTION) {
        *exc = f->exc;
    }
    return f->res;
}

static int64_t fake_clock(void *ctx)
{
    return ((fake_t *)ctx)->clock_ms;
}

static ps_env_t env_for(fake_t *f)
{
    ps_env_t e = { fake_link, f, fake_clock, f };
    return e;
}

static reg_model_t make_model(void)
{
    reg_model_t m;
    reg_model_init(&m, 15000, 180000);
    return m;
}

TEST_CASE("poll_step: OK updates the model and carries durations from the clock", "[poll_step]")
{
    ps_config_t c = base_cfg();
    setup_running(&c);
    reg_model_t m = make_model();
    fake_t f = { .clock_ms = 1000, .dur_ms = 24, .res = PS_RES_OK, .val = { 215 } };
    ps_env_t env = env_for(&f);
    ps_txn_t txn;
    int64_t wait = 0;
    TEST_ASSERT_TRUE(poll_step(&S, &m, &env, &txn, &wait));
    TEST_ASSERT_EQUAL_UINT16(0, f.last_reg);
    TEST_ASSERT_EQUAL_UINT16(1, f.last_qty);
    TEST_ASSERT_EQUAL_UINT8(1, f.last_addr);
    TEST_ASSERT_EQUAL(PS_KIND_POLL, txn.req.kind);
    TEST_ASSERT_EQUAL_INT64(1000, txn.t_start_ms);
    TEST_ASSERT_EQUAL_INT64(1024, txn.t_end_ms);
    TEST_ASSERT_EQUAL_UINT16(215, txn.val[0]);
    rm_entry_t e;
    TEST_ASSERT_TRUE(reg_model_get(&m, 0, &e));
    TEST_ASSERT_EQUAL(RM_OK, e.avail);
    TEST_ASSERT_EQUAL_UINT16(215, e.raw);
    TEST_ASSERT_EQUAL_INT64(1024, e.t_ok_ms);
}

TEST_CASE("poll_step: wait_until is propagated, INT64_MAX when idle", "[poll_step]")
{
    ps_config_t c = base_cfg();
    setup_running(&c);
    reg_model_t m = make_model();
    fake_t f = { .clock_ms = 0, .dur_ms = 24, .res = PS_RES_OK };
    ps_env_t env = env_for(&f);
    ps_txn_t txn;
    int64_t wait = 0;
    TEST_ASSERT_TRUE(poll_step(&S, &m, &env, &txn, &wait));
    TEST_ASSERT_FALSE(poll_step(&S, &m, &env, &txn, &wait));
    TEST_ASSERT_EQUAL_INT64(124, wait);
    TEST_ASSERT_EQUAL(1, f.calls);                                   /* no link access while waiting */
    f.clock_ms = 124;
    TEST_ASSERT_TRUE(poll_step(&S, &m, &env, &txn, &wait));

    setup(&c);
    TEST_ASSERT_FALSE(poll_step(&S, &m, &env, &txn, &wait));
    TEST_ASSERT_EQUAL_INT64(INT64_MAX, wait);
}

TEST_CASE("poll_step: EXCEPTION qty 1 code 02 marks the model unsupported", "[poll_step]")
{
    ps_config_t c = base_cfg();
    setup_running(&c);
    reg_model_t m = make_model();
    fake_t f = { .clock_ms = 0, .dur_ms = 5, .res = PS_RES_EXCEPTION, .exc = 2 };
    ps_env_t env = env_for(&f);
    ps_txn_t txn;
    int64_t wait;
    TEST_ASSERT_TRUE(poll_step(&S, &m, &env, &txn, &wait));
    TEST_ASSERT_EQUAL_UINT8(2, txn.exc_code);
    rm_entry_t e;
    reg_model_get(&m, 0, &e);
    TEST_ASSERT_EQUAL(RM_UNSUPPORTED, e.avail);
    TEST_ASSERT_EQUAL_UINT8(2, e.exc_code);
}

TEST_CASE("poll_step: failed POLL touches the model only after the retry (retries 1)", "[poll_step]")
{
    ps_config_t c = base_cfg();
    setup_running(&c);
    reg_model_t m = make_model();
    fake_t f = { .clock_ms = 0, .dur_ms = 500, .res = PS_RES_TIMEOUT };
    ps_env_t env = env_for(&f);
    ps_txn_t txn;
    int64_t wait;
    rm_entry_t e;
    TEST_ASSERT_TRUE(poll_step(&S, &m, &env, &txn, &wait));
    TEST_ASSERT_EQUAL(PS_KIND_POLL, txn.req.kind);
    reg_model_get(&m, 0, &e);
    TEST_ASSERT_EQUAL(RM_UNKNOWN, e.avail);                          /* first try: not final */
    f.clock_ms += 100;
    TEST_ASSERT_TRUE(poll_step(&S, &m, &env, &txn, &wait));
    TEST_ASSERT_EQUAL(PS_KIND_RETRY, txn.req.kind);
    reg_model_get(&m, 0, &e);
    TEST_ASSERT_EQUAL(RM_TIMEOUT, e.avail);
    TEST_ASSERT_EQUAL_UINT32(1, e.fail_count);
}

TEST_CASE("poll_step: retries 0 -> bad frame and error are final at once", "[poll_step]")
{
    ps_config_t c = base_cfg();
    c.retries = 0;
    setup_running(&c);
    reg_model_t m = make_model();
    fake_t f = { .clock_ms = 0, .dur_ms = 10, .res = PS_RES_BAD_FRAME };
    ps_env_t env = env_for(&f);
    ps_txn_t txn;
    int64_t wait;
    rm_entry_t e;
    TEST_ASSERT_TRUE(poll_step(&S, &m, &env, &txn, &wait));
    reg_model_get(&m, 0, &e);
    TEST_ASSERT_EQUAL(RM_TIMEOUT, e.avail);
    f.res = PS_RES_ERROR;
    f.clock_ms += 100;
    TEST_ASSERT_TRUE(poll_step(&S, &m, &env, &txn, &wait));
    reg_model_get(&m, 8, &e);
    TEST_ASSERT_EQUAL(RM_TIMEOUT, e.avail);
}

TEST_CASE("poll_step: manual qty 3 OK updates every register of the range", "[poll_step]")
{
    ps_config_t c = base_cfg();
    setup(&c);
    reg_model_t m = make_model();
    fake_t f = { .clock_ms = 50, .dur_ms = 10, .res = PS_RES_OK, .val = { 1, 2, 3, 4 } };
    ps_env_t env = env_for(&f);
    ps_txn_t txn;
    int64_t wait;
    TEST_ASSERT_EQUAL(0, poll_sched_request_manual(&S, 242, 3));
    TEST_ASSERT_TRUE(poll_step(&S, &m, &env, &txn, &wait));
    TEST_ASSERT_EQUAL_UINT16(3, f.last_qty);
    rm_entry_t e;
    for (uint16_t k = 0; k < 3; k++) {
        reg_model_get(&m, (uint16_t)(242 + k), &e);
        TEST_ASSERT_EQUAL(RM_OK, e.avail);
        TEST_ASSERT_EQUAL_UINT16(k + 1, e.raw);
        TEST_ASSERT_EQUAL_INT64(60, e.t_ok_ms);
    }
    reg_model_get(&m, 245, &e);
    TEST_ASSERT_EQUAL(RM_UNKNOWN, e.avail);                          /* 4th value not requested */
}

TEST_CASE("poll_step: failures and exceptions of qty > 1 do not touch the model", "[poll_step]")
{
    ps_config_t c = base_cfg();
    setup(&c);
    reg_model_t m = make_model();
    fake_t f = { .clock_ms = 0, .dur_ms = 10, .res = PS_RES_TIMEOUT };
    ps_env_t env = env_for(&f);
    ps_txn_t txn;
    int64_t wait;
    rm_entry_t e;
    poll_sched_request_manual(&S, 200, 2);
    TEST_ASSERT_TRUE(poll_step(&S, &m, &env, &txn, &wait));
    f.res = PS_RES_EXCEPTION;
    f.exc = 2;
    f.clock_ms += 100;
    poll_sched_request_manual(&S, 200, 2);
    TEST_ASSERT_TRUE(poll_step(&S, &m, &env, &txn, &wait));
    for (uint16_t r = 200; r <= 201; r++) {
        reg_model_get(&m, r, &e);
        TEST_ASSERT_EQUAL(RM_UNKNOWN, e.avail);
        TEST_ASSERT_EQUAL_UINT32(0, e.fail_count);
    }
}

TEST_CASE("poll_step: a failed MANUAL read is final even while a POLL retry is pending", "[poll_step]")
{
    ps_config_t c = base_cfg();
    setup_running(&c);
    reg_model_t m = make_model();
    fake_t f = { .clock_ms = 0, .dur_ms = 10, .res = PS_RES_TIMEOUT };
    ps_env_t env = env_for(&f);
    ps_txn_t txn;
    int64_t wait;
    TEST_ASSERT_TRUE(poll_step(&S, &m, &env, &txn, &wait));         /* POLL 000 fails, retry queued */
    poll_sched_request_manual(&S, 15, 1);
    f.clock_ms += 100;
    TEST_ASSERT_TRUE(poll_step(&S, &m, &env, &txn, &wait));
    TEST_ASSERT_EQUAL(PS_KIND_MANUAL, txn.req.kind);
    rm_entry_t e;
    reg_model_get(&m, 15, &e);
    TEST_ASSERT_EQUAL(RM_TIMEOUT, e.avail);
}

/* ------------------------------------------------------------ amendment 2026-11-24 */

/* U1: MANUAL while OFFLINE */
TEST_CASE("poll_sched U1: MANUAL while OFFLINE is single, gap-paced and leaves health state alone", "[poll_sched]")
{
    ps_config_t c = base_cfg();
    c.retries = 0;
    setup_running(&c);
    fail_polls(5);
    TEST_ASSERT_EQUAL(PS_OFFLINE, poll_sched_health(&S));
    uint32_t failures = S.failures;
    int64_t next_health = S.next_health_ms;
    int64_t t_end = g_now;

    poll_sched_request_manual(&S, 15, 1);
    ps_req_t r = take();
    expect_req(r, PS_KIND_MANUAL, 15);
    TEST_ASSERT_EQUAL_INT64(t_end + 100, g_now);                    /* gap respected */
    finish(r, PS_RES_TIMEOUT, 0, DUR);
    TEST_ASSERT_EQUAL(PS_OFFLINE, poll_sched_health(&S));
    TEST_ASSERT_EQUAL_UINT32(failures, S.failures);
    TEST_ASSERT_EQUAL_INT64(next_health, S.next_health_ms);
    int64_t wait = 0;
    TEST_ASSERT_FALSE(poll_sched_next(&S, g_now + 100, &r, &wait)); /* not retried */
    TEST_ASSERT_EQUAL_INT64(next_health, wait);

    poll_sched_request_manual(&S, 0, 1);                            /* OK read of 000 */
    expect_req(take_finish(PS_RES_OK, 0), PS_KIND_MANUAL, 0);
    TEST_ASSERT_EQUAL(PS_OFFLINE, poll_sched_health(&S));
    TEST_ASSERT_EQUAL_UINT32(failures, S.failures);
    TEST_ASSERT_EQUAL_INT64(next_health, S.next_health_ms);
    r = take();
    expect_req(r, PS_KIND_HEALTH, 0);
    TEST_ASSERT_EQUAL_INT64(next_health, g_now);
}

/* U3: RETRY with exception 02 */
TEST_CASE("poll_sched U3: POLL timeout then RETRY exception 02 marks unsupported for good", "[poll_sched]")
{
    ps_config_t c = base_cfg();
    setup_running(&c);
    expect_req(take_finish(PS_RES_TIMEOUT, 0), PS_KIND_POLL, 0);
    expect_req(take_finish(PS_RES_EXCEPTION, 2), PS_KIND_RETRY, 0);
    TEST_ASSERT_TRUE(poll_sched_is_unsupported(&S, 0));
    while (g_now < 130000) {
        ps_req_t r = take_finish(PS_RES_OK, 0);
        TEST_ASSERT_NOT_EQUAL(0, r.reg);
    }
}

/* U4: start acts only from STOPPED */
static void assert_sched_same(const poll_sched_t *a, const poll_sched_t *b)
{
    TEST_ASSERT_EQUAL(a->health, b->health);
    TEST_ASSERT_EQUAL_UINT32(a->failures, b->failures);
    TEST_ASSERT_EQUAL_INT64(a->next_health_ms, b->next_health_ms);
    TEST_ASSERT_EQUAL(a->retry_pending, b->retry_pending);
    TEST_ASSERT_EQUAL_MEMORY(a->due_ms, b->due_ms, sizeof(a->due_ms));
}

TEST_CASE("poll_sched rule 9: cancel_manual drops only the pending MANUAL", "[poll_sched]")
{
    ps_config_t c = base_cfg();
    setup(&c);
    TEST_ASSERT_EQUAL(0, poll_sched_request_manual(&S, 15, 1));
    TEST_ASSERT_EQUAL(0, poll_sched_request_sweep(&S));
    ps_stats_t before, after;
    poll_sched_get_stats(&S, &before);
    poll_sched_cancel_manual(&S);
    expect_req(take(), PS_KIND_SWEEP, daikin_regs_at(0)->reg);       /* SWEEP survives, no MANUAL */
    TEST_ASSERT_EQUAL(PS_STOPPED, poll_sched_health(&S));
    poll_sched_get_stats(&S, &after);
    TEST_ASSERT_EQUAL_MEMORY(&before, &after, sizeof(before));
    TEST_ASSERT_EQUAL(0, poll_sched_request_manual(&S, 15, 1));      /* slot is free again */

    setup_running(&c);                                               /* nothing pending: no-op */
    poll_sched_t ref = S;
    poll_sched_cancel_manual(&S);
    assert_sched_same(&ref, &S);
    expect_req(take(), PS_KIND_POLL, daikin_regs_at(0)->reg);        /* still the due POLL */
}

TEST_CASE("poll_sched U4: start is a no-op in ONLINE, DEGRADED and OFFLINE", "[poll_sched]")
{
    ps_config_t c = base_cfg();
    c.retries = 0;
    setup_running(&c);
    take_finish(PS_RES_OK, 0);
    fail_polls(2);
    TEST_ASSERT_EQUAL(PS_DEGRADED, poll_sched_health(&S));
    poll_sched_t before = S;
    poll_sched_start(&S, g_now + 7);
    assert_sched_same(&before, &S);

    fail_polls(3);
    TEST_ASSERT_EQUAL(PS_OFFLINE, poll_sched_health(&S));
    before = S;
    poll_sched_start(&S, g_now + 7);
    assert_sched_same(&before, &S);               /* counter, backoff, due times */

    setup_running(&c);
    take_finish(PS_RES_OK, 0);
    before = S;
    poll_sched_start(&S, g_now + 7);
    assert_sched_same(&before, &S);
}

TEST_CASE("poll_sched U4: start from STOPPED -> ONLINE, counter 0, all due now, pending kept", "[poll_sched]")
{
    ps_config_t c = base_cfg();
    c.retries = 0;
    setup_running(&c);
    fail_polls(2);
    poll_sched_stop(&S);
    poll_sched_request_manual(&S, 15, 1);
    poll_sched_request_sweep(&S);
    poll_sched_start(&S, 9000);
    TEST_ASSERT_EQUAL(PS_ONLINE, poll_sched_health(&S));
    TEST_ASSERT_EQUAL_UINT32(0, S.failures);
    for (size_t i = 0; i < POLLED_COUNT; i++) {
        TEST_ASSERT_EQUAL_INT64(9000, S.due_ms[idx(FIRST_CYCLE[i])]);
    }
    g_now = 9000;
    expect_req(take(), PS_KIND_MANUAL, 15);                         /* manual and sweep kept */
    TEST_ASSERT_TRUE(S.sweep_active);
}

TEST_CASE("poll_sched U4: set_addr from DEGRADED/OFFLINE -> ONLINE all due now; STOPPED/LATCHED kept", "[poll_sched]")
{
    ps_config_t c = base_cfg();
    c.retries = 0;
    setup_running(&c);
    fail_polls(5);
    TEST_ASSERT_EQUAL(PS_OFFLINE, poll_sched_health(&S));
    TEST_ASSERT_EQUAL(0, poll_sched_set_addr(&S, 2, 77000));
    TEST_ASSERT_EQUAL(PS_ONLINE, poll_sched_health(&S));
    TEST_ASSERT_EQUAL_UINT32(0, S.failures);
    for (size_t i = 0; i < POLLED_COUNT; i++) {
        TEST_ASSERT_EQUAL_INT64(77000, S.due_ms[idx(FIRST_CYCLE[i])]);
    }
    poll_sched_latch(&S);
    TEST_ASSERT_EQUAL(0, poll_sched_set_addr(&S, 3, 78000));
    TEST_ASSERT_EQUAL(PS_LATCHED, poll_sched_health(&S));
}

/* Rule 12: address mismatch */
TEST_CASE("poll_sched rule 12: completion for an old address only counts stats and the gap", "[poll_sched]")
{
    for (uint8_t retries = 0; retries <= 1; retries++) {
        ps_config_t c = base_cfg();
        c.retries = retries;
        setup_running(&c);
        ps_req_t old_req = take();
        TEST_ASSERT_EQUAL_UINT8(1, old_req.addr);
        TEST_ASSERT_EQUAL(0, poll_sched_set_addr(&S, 2, g_now));
        poll_sched_t before = S;
        ps_req_t reqs[3] = { old_req, old_req, old_req };
        reqs[1].reg = 104;
        reqs[2].reg = 8;
        static const ps_res_t results[3] = { PS_RES_TIMEOUT, PS_RES_EXCEPTION, PS_RES_OK };
        for (int k = 0; k < 3; k++) {
            finish(reqs[k], results[k], 2, DUR);
            TEST_ASSERT_FALSE(S.retry_pending);
            TEST_ASSERT_EQUAL_UINT32(0, S.failures);
            TEST_ASSERT_EQUAL(PS_ONLINE, poll_sched_health(&S));
            TEST_ASSERT_FALSE(poll_sched_is_unsupported(&S, 104));
            TEST_ASSERT_EQUAL_MEMORY(before.due_ms, S.due_ms, sizeof(S.due_ms));
            ps_stats_t st;
            poll_sched_get_stats(&S, &st);
            TEST_ASSERT_EQUAL_UINT32((uint32_t)k + 1, st.txns);
            int64_t wait = 0;
            ps_req_t r;
            TEST_ASSERT_FALSE(poll_sched_next(&S, g_now, &r, &wait));
            TEST_ASSERT_EQUAL_INT64(g_now + 100, wait);             /* gap after the stale transaction */
        }
        ps_req_t r = take();
        TEST_ASSERT_EQUAL_UINT8(2, r.addr);
        expect_req(r, PS_KIND_POLL, 0);
    }
}

typedef struct { fake_t f; poll_sched_t *s; } addr_change_t;

static ps_res_t addr_change_link(void *ctx, uint8_t addr, uint16_t reg, uint16_t qty, uint16_t *val, uint8_t *exc)
{
    addr_change_t *a = ctx;
    ps_res_t res = fake_link(&a->f, addr, reg, qty, val, exc);
    poll_sched_set_addr(a->s, 2, a->f.clock_ms);
    return res;
}

TEST_CASE("poll_step rule 12: set_addr during the link call leaves the model untouched", "[poll_step]")
{
    static const struct { ps_res_t res; uint8_t exc; } cases[] = {
        { PS_RES_OK, 0 }, { PS_RES_EXCEPTION, 2 }, { PS_RES_TIMEOUT, 0 },
    };
    for (size_t k = 0; k < sizeof(cases) / sizeof(cases[0]); k++) {
        ps_config_t c = base_cfg();
        setup_running(&c);
        reg_model_t m = make_model();
        addr_change_t a = { .f = { .clock_ms = 0, .dur_ms = 20, .res = cases[k].res,
                                   .exc = cases[k].exc, .val = { 215 } }, .s = &S };
        ps_env_t env = { addr_change_link, &a, fake_clock, &a.f };
        ps_txn_t txn;
        int64_t wait;
        TEST_ASSERT_TRUE(poll_step(&S, &m, &env, &txn, &wait));
        TEST_ASSERT_EQUAL_UINT8(1, txn.req.addr);
        rm_entry_t e;
        reg_model_get(&m, 0, &e);
        TEST_ASSERT_EQUAL(RM_UNKNOWN, e.avail);
        TEST_ASSERT_FALSE(e.has_value);
        TEST_ASSERT_EQUAL_UINT32(0, e.fail_count);
        TEST_ASSERT_FALSE(poll_sched_is_unsupported(&S, 0));
        TEST_ASSERT_EQUAL(PS_ONLINE, poll_sched_health(&S));
        TEST_ASSERT_FALSE(S.retry_pending);
    }
}

/* U2: model update per exception code */
static void step_after_gap(poll_sched_t *s, reg_model_t *m, const ps_env_t *env, fake_t *f, ps_txn_t *txn)
{
    int64_t wait;
    f->clock_ms += 100;
    TEST_ASSERT_TRUE(poll_step(s, m, env, txn, &wait));
}

TEST_CASE("poll_step U2: POLL exception 02 -> UNSUPPORTED, other codes -> TIMEOUT with exc_code and value kept", "[poll_step]")
{
    ps_config_t c = base_cfg();
    setup_running(&c);
    reg_model_t m = make_model();
    fake_t f = { .clock_ms = 0, .dur_ms = 10, .res = PS_RES_OK, .val = { 215 } };
    ps_env_t env = env_for(&f);
    ps_txn_t txn;
    for (size_t i = 0; i < POLLED_COUNT; i++) {
        step_after_gap(&S, &m, &env, &f, &txn);
    }
    f.clock_ms = 6000;                                               /* FAST regs due again */
    f.res = PS_RES_EXCEPTION;
    static const uint8_t codes[] = { 1, 3, 4 };
    static const uint16_t regs[] = { 0, 8, 104 };
    for (size_t k = 0; k < 3; k++) {
        f.exc = codes[k];
        step_after_gap(&S, &m, &env, &f, &txn);
        TEST_ASSERT_EQUAL_UINT16(regs[k], txn.req.reg);
        rm_entry_t e;
        reg_model_get(&m, regs[k], &e);
        TEST_ASSERT_EQUAL(RM_TIMEOUT, e.avail);
        TEST_ASSERT_EQUAL_UINT8(codes[k], e.exc_code);
        TEST_ASSERT_TRUE(e.has_value);
        TEST_ASSERT_EQUAL_UINT16(215, e.raw);
        TEST_ASSERT_FALSE(poll_sched_is_unsupported(&S, regs[k]));
    }
    f.exc = 2;
    step_after_gap(&S, &m, &env, &f, &txn);
    rm_entry_t e;
    reg_model_get(&m, txn.req.reg, &e);
    TEST_ASSERT_EQUAL(RM_UNSUPPORTED, e.avail);
    TEST_ASSERT_TRUE(poll_sched_is_unsupported(&S, txn.req.reg));
}

TEST_CASE("poll_step U2: HEALTH exception 02 is a failed read, not unsupported", "[poll_step]")
{
    ps_config_t c = base_cfg();
    c.retries = 0;
    setup_running(&c);
    reg_model_t m = make_model();
    fake_t f = { .clock_ms = 0, .dur_ms = 10, .res = PS_RES_TIMEOUT };
    ps_env_t env = env_for(&f);
    ps_txn_t txn;
    int64_t wait;
    for (int i = 0; i < 5; i++) {
        step_after_gap(&S, &m, &env, &f, &txn);
    }
    TEST_ASSERT_EQUAL(PS_OFFLINE, poll_sched_health(&S));
    f.clock_ms = S.next_health_ms;
    f.res = PS_RES_EXCEPTION;
    f.exc = 2;
    TEST_ASSERT_TRUE(poll_step(&S, &m, &env, &txn, &wait));
    TEST_ASSERT_EQUAL(PS_KIND_HEALTH, txn.req.kind);
    rm_entry_t e;
    reg_model_get(&m, 0, &e);
    TEST_ASSERT_EQUAL(RM_TIMEOUT, e.avail);
    TEST_ASSERT_EQUAL_UINT8(2, e.exc_code);
    TEST_ASSERT_FALSE(poll_sched_is_unsupported(&S, 0));
}

TEST_CASE("poll_step U3: RETRY exception 02 -> UNSUPPORTED in scheduler and model", "[poll_step]")
{
    ps_config_t c = base_cfg();
    setup_running(&c);
    reg_model_t m = make_model();
    fake_t f = { .clock_ms = 0, .dur_ms = 10, .res = PS_RES_TIMEOUT };
    ps_env_t env = env_for(&f);
    ps_txn_t txn;
    step_after_gap(&S, &m, &env, &f, &txn);
    f.res = PS_RES_EXCEPTION;
    f.exc = 2;
    step_after_gap(&S, &m, &env, &f, &txn);
    TEST_ASSERT_EQUAL(PS_KIND_RETRY, txn.req.kind);
    rm_entry_t e;
    reg_model_get(&m, 0, &e);
    TEST_ASSERT_EQUAL(RM_UNSUPPORTED, e.avail);
    TEST_ASSERT_TRUE(poll_sched_is_unsupported(&S, 0));
}

/* Sweep end */
TEST_CASE("poll_sched: request_sweep is refused until the 32nd read is issued", "[poll_sched]")
{
    ps_config_t c = base_cfg();
    setup(&c);
    TEST_ASSERT_EQUAL(0, poll_sched_request_sweep(&S));
    for (int i = 0; i < RM_REG_COUNT - 1; i++) {
        ps_req_t r = take();
        TEST_ASSERT_EQUAL(PS_KIND_SWEEP, r.kind);
        TEST_ASSERT_EQUAL(-1, poll_sched_request_sweep(&S));
        finish(r, PS_RES_OK, 0, DUR);
    }
    ps_req_t last = take();                                          /* 32nd issued, not completed */
    TEST_ASSERT_EQUAL(PS_KIND_SWEEP, last.kind);
    TEST_ASSERT_EQUAL(0, poll_sched_request_sweep(&S));
}

/* Mixed-kind soak */
#define SOAK_MS            (2 * 60 * 60 * 1000LL)
#define SOAK_OUTAGE_START  1000000LL
#define SOAK_OUTAGE_END    1300000LL
#define SOAK_MAX_STEPS     1000000

typedef struct { int64_t clock_ms; int calls; } soak_link_t;

static int64_t soak_clock(void *ctx)
{
    return ((soak_link_t *)ctx)->clock_ms;
}

/* Scripted slave: total outage window, reg 104 unsupported, periodic timeouts,
 * bad frames and other exceptions, latency 5..64 ms. */
static ps_res_t soak_link(void *ctx, uint8_t addr, uint16_t reg, uint16_t qty, uint16_t *val, uint8_t *exc)
{
    soak_link_t *l = ctx;
    int n = ++l->calls;
    (void)addr;
    (void)qty;
    int64_t start = l->clock_ms;
    l->clock_ms += 5 + (n * 7) % 60;
    if (start >= SOAK_OUTAGE_START && start < SOAK_OUTAGE_END) {
        l->clock_ms += 400;
        return PS_RES_TIMEOUT;
    }
    if (reg == 104) {
        *exc = 2;
        return PS_RES_EXCEPTION;
    }
    if (n % 97 == 0) {
        return PS_RES_TIMEOUT;
    }
    if (n % 41 == 0) {
        return PS_RES_BAD_FRAME;
    }
    if (n % 53 == 0) {
        *exc = 3;
        return PS_RES_EXCEPTION;
    }
    val[0] = 215;
    return PS_RES_OK;
}

TEST_CASE("poll_step soak: 2 h mixed kinds keep the gap on every request", "[poll_step]")
{
    ps_config_t c = base_cfg();
    setup_running(&c);
    reg_model_t m = make_model();
    soak_link_t l = { .clock_ms = 0 };
    ps_env_t env = { soak_link, &l, soak_clock, &l };
    bool seen[PS_KIND_MANUAL + 1] = { false };
    bool offline_manual_requested = false, manual_in_outage = false;
    int64_t prev_end = 0;
    bool have_prev = false;
    int64_t next_manual = 250000, next_sweep = 300000;
    static const int64_t extra_sweeps[] = { 1100000, 5000000 };   /* the first one overlaps the outage */
    size_t extra = 0;

    for (int step = 0; step < SOAK_MAX_STEPS && l.clock_ms < SOAK_MS; step++) {
        if (l.clock_ms >= next_manual) {
            poll_sched_request_manual(&S, (step % 2) ? 15 : 0, 1);
            next_manual += 250000;
        }
        if (poll_sched_health(&S) == PS_OFFLINE && !offline_manual_requested) {
            offline_manual_requested = poll_sched_request_manual(&S, 15, 1) == 0;
        }
        if (l.clock_ms >= next_sweep) {
            poll_sched_request_sweep(&S);
            next_sweep = extra < 2 ? extra_sweeps[extra++] : INT64_MAX;
        }
        ps_txn_t txn;
        int64_t wait;
        if (!poll_step(&S, &m, &env, &txn, &wait)) {
            TEST_ASSERT_NOT_EQUAL(INT64_MAX, wait);
            TEST_ASSERT_GREATER_THAN_INT64(l.clock_ms, wait);
            l.clock_ms = wait;
            continue;
        }
        if (have_prev) {
            TEST_ASSERT_GREATER_OR_EQUAL_INT64(prev_end + (int64_t)c.gap_ms, txn.t_start_ms);
        }
        prev_end = txn.t_end_ms;
        have_prev = true;
        seen[txn.req.kind] = true;
        if (txn.req.kind == PS_KIND_MANUAL && txn.t_start_ms >= SOAK_OUTAGE_START &&
            txn.t_start_ms < SOAK_OUTAGE_END) {
            manual_in_outage = true;
        }
    }
    TEST_ASSERT_GREATER_OR_EQUAL_INT64(SOAK_MS, l.clock_ms);
    for (int k = PS_KIND_POLL; k <= PS_KIND_MANUAL; k++) {
        TEST_ASSERT_TRUE_MESSAGE(seen[k], "kind missing in soak");
    }
    ps_stats_t st;
    poll_sched_get_stats(&S, &st);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(1, st.offline_entries);
    TEST_ASSERT_TRUE(manual_in_outage);
}
