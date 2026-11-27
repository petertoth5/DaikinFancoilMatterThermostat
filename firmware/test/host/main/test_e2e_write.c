/* test_e2e_write.c - end-to-end host tests of the console write path (task 019a).
 * write_guard_step -> poll_sched -> reg_model against the scripted fake slave (FC03 and FC06).
 * Simulated time only. The fake enforces the 100 ms bus gap, the FC06 register/address whitelist and
 * (at the end of every scenario, rig_finish) the 5 s spacing of FC06 frames. No FC06 may exist while
 * disarmed; there is no bench slave, so these scenarios are the only pre-trip evidence of the write path.
 */

#include <stdio.h>
#include <string.h>
#include "unity.h"
#include "daikin_regs.h"
#include "fake_slave.h"
#include "poll_sched.h"
#include "reg_model.h"
#include "txn_log.h"
#include "write_guard.h"

/* ------------------------------------------------------------ constants */

/* 001 Kconfig defaults, as in test_e2e_poll.c. */
#define SLAVE_ADDR        1
#define GAP_MS            100
#define FAST_MS           5000
#define SLOW_MS           60000
#define RETRIES           1
#define OFFLINE_AFTER     5
#define PROBE_MS          30000
#define RESP_TIMEOUT_MS   500
#define STALE_FAST_MS     (3 * FAST_MS)
#define STALE_SLOW_MS     (3 * SLOW_MS)

#define SECOND_MS         1000
#define PACING_MS         WG_PACING_MS
#define READY_MS          (10 * SECOND_MS)     /* time to poll every documented register once */
#define STEP_LIMIT        100000

/* Registers (register-map). */
#define REG_PRG    201
#define REG_SP     231
#define REG_SEASON 233
#define REG_SPL    202
#define REG_SPH    203
#define REG_SPL_W  245
#define REG_SPH_W  246
#define REG_WEB    247

/* Initial panel state of the fake slave. 201 carries bits the node must never touch:
 * 0x0010 LOCK, 0x0040 (undocumented bit 6), 0x0100 (reserved bit 8). */
#define INIT_PRG      0x0150
#define INIT_SP       210       /* 21.0 C */
#define INIT_SEASON   3
#define INIT_SPL      160
#define INIT_SPH      280
#define INIT_SPL_W    200
#define INIT_SPH_W    240
#define PRG_STANDBY   0x0080
#define PRG_MODE_MASK 0x0007
#define PRG_UNDEFINED_MODE 0x0154   /* mode bits 100 */
#define SP_TARGET     215
#define WEB_FLAG(n)   (1u << (n))

#define LINES_MAX 400
#define LINE_LEN  96

static const ps_config_t k_cfg = {
    .addr = SLAVE_ADDR, .gap_ms = GAP_MS, .fast_ms = FAST_MS, .slow_ms = SLOW_MS,
    .offline_probe_ms = PROBE_MS, .retries = RETRIES, .offline_after = OFFLINE_AFTER,
};

/* ------------------------------------------------------------ rig */

typedef struct {
    fake_slave_t fake;
    int64_t clock_ms;
    poll_sched_t sched;
    reg_model_t model;
    wg_env_t env;
    write_guard_t wg;
    wg_events_t ev;
    char lines[LINES_MAX][LINE_LEN];   /* every W/A line printed since rig_init */
    size_t n_lines;
} wrig_t;

static wrig_t g_rig;   /* static: the fake carries large journal and replay tables */

static void capture(wrig_t *r)
{
    for (size_t i = 0; i < r->ev.n; i++) {
        TEST_ASSERT_LESS_THAN_size_t(LINES_MAX, r->n_lines);
        char *line = r->lines[r->n_lines++];
        TEST_ASSERT_GREATER_THAN_size_t_MESSAGE(0, write_guard_format_event(line, LINE_LEN, &r->ev.e[i]), "event does not format");
    }
    r->ev.n = 0;
}

static void print_lines(const wrig_t *r)
{
    for (size_t i = 0; i < r->n_lines; i++) {
        printf("    %s\n", r->lines[i]);
    }
}

/* Lines are "<A|W> <t> <rest>"; compare kind and rest, ignoring the time. */
static bool line_matches(const char *line, char kind, const char *rest)
{
    if (line[0] != kind || line[1] != ' ') {
        return false;
    }
    const char *p = strchr(line + 2, ' ');
    return p != NULL && strcmp(p + 1, rest) == 0;
}

static size_t count_lines(const wrig_t *r, char kind, const char *rest)
{
    size_t c = 0;
    for (size_t i = 0; i < r->n_lines; i++) {
        c += line_matches(r->lines[i], kind, rest);
    }
    return c;
}

static void expect_line(const wrig_t *r, char kind, const char *rest)
{
    if (count_lines(r, kind, rest) == 0) {
        printf("missing line: %c <t> %s\n", kind, rest);
        print_lines(r);
        TEST_FAIL_MESSAGE("expected W/A line not printed");
    }
}

static void expect_no_line(const wrig_t *r, char kind, const char *rest)
{
    if (count_lines(r, kind, rest) != 0) {
        printf("unexpected line: %c <t> %s\n", kind, rest);
        print_lines(r);
        TEST_FAIL_MESSAGE("unexpected W/A line printed");
    }
}

static void set_reg(wrig_t *r, uint16_t reg, uint16_t raw)
{
    fake_set_reg(&r->fake, reg, raw, FAKE_DEFAULT_LATENCY_MS);
    reg_model_set_ok(&r->model, reg, raw, r->clock_ms);
}

static void populate(wrig_t *r)
{
    static const uint16_t polled[] = { 0, 1, 8, 9, 104, 105 };
    for (size_t i = 0; i < sizeof(polled) / sizeof(polled[0]); i++) {
        fake_set_reg(&r->fake, polled[i], 0, FAKE_DEFAULT_LATENCY_MS);
    }
    fake_set_reg(&r->fake, REG_PRG, INIT_PRG, FAKE_DEFAULT_LATENCY_MS);
    fake_set_reg(&r->fake, REG_SP, INIT_SP, FAKE_DEFAULT_LATENCY_MS);
    fake_set_reg(&r->fake, REG_SEASON, INIT_SEASON, FAKE_DEFAULT_LATENCY_MS);
    fake_set_reg(&r->fake, REG_SPL, INIT_SPL, FAKE_DEFAULT_LATENCY_MS);
    fake_set_reg(&r->fake, REG_SPH, INIT_SPH, FAKE_DEFAULT_LATENCY_MS);
    fake_set_reg(&r->fake, REG_SPL_W, INIT_SPL_W, FAKE_DEFAULT_LATENCY_MS);
    fake_set_reg(&r->fake, REG_SPH_W, INIT_SPH_W, FAKE_DEFAULT_LATENCY_MS);
    fake_set_reg(&r->fake, REG_WEB, 0, FAKE_DEFAULT_LATENCY_MS);
}

static void rig_init(wrig_t *r)
{
    memset(&r->ev, 0, sizeof(r->ev));
    r->n_lines = 0;
    r->clock_ms = 0;
    fake_init(&r->fake, SLAVE_ADDR, &r->clock_ms, RESP_TIMEOUT_MS);
    TEST_ASSERT_EQUAL_INT(0, poll_sched_init(&r->sched, &k_cfg));
    reg_model_init(&r->model, STALE_FAST_MS, STALE_SLOW_MS);
    write_guard_init(&r->wg);
    r->env = (wg_env_t){
        .ps = { .link = fake_link, .link_ctx = &r->fake, .clock = fake_clock, .clock_ctx = &r->fake },
        .write = fake_write, .write_ctx = &r->fake,
    };
    populate(r);
}

/* One write_guard_step; the fake journal learns the kind of the read it just served. */
static bool wstep(wrig_t *r, ps_txn_t *txn, int64_t *wait_ms)
{
    bool did = write_guard_step(&r->wg, &r->sched, &r->model, &r->env, txn, &r->ev, wait_ms);
    capture(r);
    if (did) {
        fake_tag_last(&r->fake, txn->req.kind);
    }
    return did;
}

/* Idle time passes: jump to the wake-up time (capped), never backwards. */
static void advance(wrig_t *r, int64_t wake_ms, int64_t cap_ms)
{
    int64_t target = wake_ms < cap_ms ? wake_ms : cap_ms;
    TEST_ASSERT_GREATER_THAN_INT64_MESSAGE(r->clock_ms, target, "scheduler made no progress");
    r->clock_ms = target;
}

static void run_until(wrig_t *r, int64_t end_ms)
{
    for (int i = 0; r->clock_ms < end_ms; i++) {
        TEST_ASSERT_LESS_THAN_INT(STEP_LIMIT, i);
        ps_txn_t txn;
        int64_t wait_ms;
        if (!wstep(r, &txn, &wait_ms)) {
            advance(r, wait_ms, end_ms);
        }
    }
}

static void run_for(wrig_t *r, int64_t ms)
{
    run_until(r, r->clock_ms + ms);
}

static bool busy(const wrig_t *r)
{
    return r->wg.phase != WG_PHASE_IDLE || r->wg.restore_active;
}

static void run_until_idle(wrig_t *r)
{
    for (int i = 0; busy(r); i++) {
        TEST_ASSERT_LESS_THAN_INT(STEP_LIMIT, i);
        ps_txn_t txn;
        int64_t wait_ms;
        if (!wstep(r, &txn, &wait_ms)) {
            advance(r, wait_ms, INT64_MAX);
        }
    }
}

/* Step until a transaction of the given kind has just completed. */
static void run_until_txn(wrig_t *r, ps_kind_t kind)
{
    for (int i = 0; i < STEP_LIMIT; i++) {
        ps_txn_t txn;
        int64_t wait_ms;
        if (wstep(r, &txn, &wait_ms)) {
            if (txn.req.kind == kind) {
                return;
            }
        } else {
            advance(r, wait_ms, INT64_MAX);
        }
    }
    TEST_FAIL_MESSAGE("transaction kind never happened");
}

static void rig_ready(wrig_t *r)
{
    rig_init(r);
    poll_sched_start(&r->sched, r->clock_ms);
    run_for(r, READY_MS);
}

static bool arm(wrig_t *r, uint32_t seconds)
{
    bool ok = write_guard_arm(&r->wg, &r->sched, &r->model, seconds, r->clock_ms, &r->ev);
    capture(r);
    return ok;
}

static void arm_ok(wrig_t *r, uint32_t seconds)
{
    if (!arm(r, seconds)) {
        print_lines(r);
        TEST_FAIL_MESSAGE("arm refused");
    }
}

static bool command(wrig_t *r, wg_cmd_type_t type, uint16_t arg)
{
    wg_cmd_t c = { .type = type, .arg = arg };
    bool ok = write_guard_command(&r->wg, &r->sched, &r->model, &c, r->clock_ms, &r->ev);
    capture(r);
    return ok;
}

static void disarm(wrig_t *r, wg_reason_t reason)
{
    write_guard_disarm(&r->wg, reason, r->clock_ms, &r->ev);
    capture(r);
}

/* Accepted command, run to its end, then the 5 s pacing so the next command is accepted. */
static void run_command(wrig_t *r, wg_cmd_type_t type, uint16_t arg)
{
    if (!command(r, type, arg)) {
        print_lines(r);
        TEST_FAIL_MESSAGE("command refused");
    }
    run_until_idle(r);
}

static void pace(wrig_t *r)
{
    run_for(r, PACING_MS);
}

static void expect_refused(wrig_t *r, wg_cmd_type_t type, uint16_t arg, const char *rest)
{
    TEST_ASSERT_FALSE(command(r, type, arg));
    expect_line(r, 'W', rest);
    TEST_ASSERT_FALSE(busy(r));
}

static wg_status_t status(const wrig_t *r)
{
    wg_status_t st;
    write_guard_get_status(&r->wg, r->clock_ms, &st);
    return st;
}

/* ------------------------------------------------------------ journal views */

/* "P201 W201 V201 ..." of the write-sequence transactions in the fake journal, polls left out. */
static void sequence_of(const wrig_t *r, char *out, size_t n)
{
    const ps_txn_t *j;
    size_t count = fake_journal(&r->fake, &j);
    size_t len = 0;
    out[0] = '\0';
    for (size_t i = 0; i < count; i++) {
        char tag = 0;
        switch (j[i].req.kind) {
        case PS_KIND_PRE:    tag = 'P'; break;
        case PS_KIND_WRITE:  tag = 'W'; break;
        case PS_KIND_VERIFY: tag = 'V'; break;
        default: break;
        }
        if (tag == 0) {
            continue;
        }
        TEST_ASSERT_LESS_THAN_size_t(n, len + 6);
        len += (size_t)snprintf(out + len, n - len, "%s%c%u", len ? " " : "", tag, (unsigned)j[i].req.reg);
    }
}

static void expect_sequence(const wrig_t *r, const char *expected)
{
    char seq[512];
    sequence_of(r, seq, sizeof(seq));
    TEST_ASSERT_EQUAL_STRING(expected, seq);
}

static int64_t last_txn_end(const wrig_t *r)
{
    const ps_txn_t *j;
    size_t n = fake_journal(&r->fake, &j);
    TEST_ASSERT_GREATER_THAN_size_t(0, n);
    return j[n - 1].t_end_ms;
}

static uint16_t fake_raw(const wrig_t *r, uint16_t reg)
{
    return r->fake.regs[reg].raw;
}

static uint16_t model_raw(const wrig_t *r, uint16_t reg)
{
    rm_entry_t e;
    TEST_ASSERT_TRUE(reg_model_get(&r->model, reg, &e));
    return e.raw;
}

static void rig_finish(const wrig_t *r)
{
    fake_check_write_spacing(&r->fake);
}

/* ------------------------------------------------------------ (a) inertness */

TEST_CASE("e2e write a: disarmed, every command is refused and no FC06 or write transaction exists in 10 min",
          "[e2e_write]")
{
    wrig_t *r = &g_rig;
    rig_ready(r);
    static const struct { wg_cmd_type_t type; uint16_t arg; const char *rest; } cases[] = {
        { WG_CMD_SP, SP_TARGET, "REFUSED sp disarmed" },
        { WG_CMD_STANDBY, 1, "REFUSED standby disarmed" },
        { WG_CMD_FAN, WG_FAN_MAX, "REFUSED fan disarmed" },
        { WG_CMD_SEASON, 5, "REFUSED season disarmed" },
        { WG_CMD_RESTORE, 0, "REFUSED restore disarmed" },
    };
    int64_t end_ms = r->clock_ms + 10 * 60 * SECOND_MS;
    while (r->clock_ms < end_ms) {
        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
            r->n_lines = 0;
            expect_refused(r, cases[i].type, cases[i].arg, cases[i].rest);
            TEST_ASSERT_FALSE(status(r).armed);
        }
        run_for(r, 60 * SECOND_MS);
    }
    TEST_ASSERT_EQUAL_UINT32(0, fake_write_count(&r->fake));
    expect_sequence(r, "");
    ps_stats_t st;
    poll_sched_get_stats(&r->sched, &st);
    TEST_ASSERT_EQUAL_UINT32(0, st.pre + st.write + st.verify);
    TEST_ASSERT_TRUE(st.txns > 100);   /* polling went on the whole time */
    rig_finish(r);
}

/* ------------------------------------------------------------ (b) arm preconditions */

static const uint16_t k_arm_regs[] = { REG_PRG, REG_SP, REG_SEASON, REG_SPL, REG_SPH, REG_WEB };

static void expect_arm_refused(wrig_t *r, const char *rest)
{
    r->n_lines = 0;
    TEST_ASSERT_FALSE(arm(r, WG_ARM_DEFAULT_S));
    expect_line(r, 'A', rest);
    TEST_ASSERT_FALSE(status(r).armed);
}

static void run_until_health(wrig_t *r, ps_health_t wanted)
{
    for (int i = 0; poll_sched_health(&r->sched) != wanted; i++) {
        TEST_ASSERT_LESS_THAN_INT(STEP_LIMIT, i);
        ps_txn_t txn;
        int64_t wait_ms;
        if (!wstep(r, &txn, &wait_ms)) {
            advance(r, wait_ms, INT64_MAX);
        }
    }
}

TEST_CASE("e2e write b1: arm needs health ONLINE and a clear latch", "[e2e_write]")
{
    wrig_t *r = &g_rig;
    rig_ready(r);
    poll_sched_stop(&r->sched);
    expect_arm_refused(r, "REFUSED arm health");

    rig_ready(r);
    fake_set_dead(&r->fake, r->clock_ms, r->clock_ms + 1000 * SECOND_MS);
    run_until_health(r, PS_DEGRADED);
    expect_arm_refused(r, "REFUSED arm health");
    run_until_health(r, PS_OFFLINE);
    expect_arm_refused(r, "REFUSED arm health");

    rig_ready(r);
    poll_sched_latch(&r->sched);
    expect_arm_refused(r, "REFUSED arm health");

    rig_ready(r);
    r->wg.latched = true;
    expect_arm_refused(r, "REFUSED arm latched");
    poll_sched_stop(&r->sched);   /* the write latch is reported before the health */
    expect_arm_refused(r, "REFUSED arm latched");
    rig_finish(r);
}

TEST_CASE("e2e write b2: every needed register must be a fresh OK value", "[e2e_write]")
{
    wrig_t *r = &g_rig;
    for (size_t i = 0; i < sizeof(k_arm_regs) / sizeof(k_arm_regs[0]); i++) {
        uint16_t reg = k_arm_regs[i];
        char rest[40];
        snprintf(rest, sizeof(rest), "REFUSED arm stale %03u", (unsigned)reg);
        for (int how = 0; how < 4; how++) {
            rig_ready(r);
            if (how == 0) {
                reg_model_set_timeout(&r->model, reg, r->clock_ms);
            } else if (how == 1) {
                reg_model_set_unsupported(&r->model, reg, 2, r->clock_ms);
            } else if (how == 2) {
                reg_model_set_ok(&r->model, reg, 0, r->clock_ms - 10 * STALE_SLOW_MS);   /* old */
            } else if (reg == REG_SEASON) {
                reg_model_set_ok(&r->model, reg, 4, r->clock_ms);   /* 4 is not in {0, 3, 5}: out of range */
            } else {
                continue;
            }
            expect_arm_refused(r, rest);
        }
    }
    rig_finish(r);
}

TEST_CASE("e2e write b3: 245 and 246 are needed only when flag 5 is set", "[e2e_write]")
{
    wrig_t *r = &g_rig;
    static const uint16_t limits[] = { REG_SPL_W, REG_SPH_W };
    for (size_t i = 0; i < 2; i++) {
        char rest[40];
        snprintf(rest, sizeof(rest), "REFUSED arm stale %03u", (unsigned)limits[i]);
        rig_ready(r);
        reg_model_set_timeout(&r->model, limits[i], r->clock_ms);
        arm_ok(r, WG_ARM_DEFAULT_S);   /* flag 5 clear: not needed */
        disarm(r, WG_DISARM_USER);
        set_reg(r, REG_WEB, WEB_FLAG(5));
        reg_model_set_timeout(&r->model, limits[i], r->clock_ms);
        expect_arm_refused(r, rest);
    }
    rig_finish(r);
}

TEST_CASE("e2e write b4: 247 flags 1 2 3 6 7 8 refuse, flags 0 4 5 allow", "[e2e_write]")
{
    wrig_t *r = &g_rig;
    for (unsigned n = 0; n <= 8; n++) {
        rig_ready(r);
        set_reg(r, REG_WEB, (uint16_t)WEB_FLAG(n));
        bool blocking = n == 1 || n == 2 || n == 3 || n == 6 || n == 7 || n == 8;
        if (blocking) {
            char rest[40];
            snprintf(rest, sizeof(rest), "REFUSED arm web_flag %u", n);
            expect_arm_refused(r, rest);
        } else {
            arm_ok(r, WG_ARM_DEFAULT_S);
        }
    }
    rig_ready(r);
    set_reg(r, REG_WEB, WEB_FLAG(6) | WEB_FLAG(3));   /* the lowest blocking flag is reported */
    expect_arm_refused(r, "REFUSED arm web_flag 3");
    rig_finish(r);
}

TEST_CASE("e2e write b5: arm seconds 30..600, ARMED line and restore set", "[e2e_write]")
{
    wrig_t *r = &g_rig;
    rig_ready(r);
    TEST_ASSERT_FALSE(arm(r, WG_ARM_MIN_S - 1));
    TEST_ASSERT_FALSE(arm(r, WG_ARM_MAX_S + 1));
    TEST_ASSERT_EQUAL_size_t(0, r->n_lines);
    arm_ok(r, WG_ARM_MIN_S);
    expect_line(r, 'A', "ARMED 30 s budget 40");
    expect_line(r, 'A', "RESTORE_SET 201=0150 233=0003 231=00d2");
    disarm(r, WG_DISARM_USER);
    arm_ok(r, WG_ARM_MAX_S);
    expect_line(r, 'A', "ARMED 600 s budget 40");
    rig_finish(r);
}

/* ------------------------------------------------------------ (c) setpoint happy path */

TEST_CASE("e2e write c: setpoint PRE WRITE VERIFY, value, model and lines", "[e2e_write]")
{
    wrig_t *r = &g_rig;
    rig_ready(r);
    arm_ok(r, WG_ARM_DEFAULT_S);
    run_command(r, WG_CMD_SP, SP_TARGET);
    expect_line(r, 'W', "CMD sp 215");
    expect_line(r, 'W', "DONE 231 00d2 -> 00d7 budget 39");
    expect_sequence(r, "P231 W231 V231");
    TEST_ASSERT_EQUAL_UINT16(SP_TARGET, fake_raw(r, REG_SP));
    TEST_ASSERT_EQUAL_UINT16(SP_TARGET, model_raw(r, REG_SP));
    TEST_ASSERT_TRUE(reg_model_fresh(&r->model, REG_SP, r->clock_ms));
    TEST_ASSERT_EQUAL_UINT8(39, status(r).budget_left);
    TEST_ASSERT_EQUAL_UINT8(0, status(r).failures);
    TEST_ASSERT_TRUE(status(r).armed);

    /* PRE, WRITE and VERIFY are three consecutive bus transactions (no poll in between, I6). */
    const ps_txn_t *j;
    size_t n = fake_journal(&r->fake, &j);
    size_t w = 0;
    while (w < n && j[w].req.kind != PS_KIND_WRITE) {
        w++;
    }
    TEST_ASSERT_TRUE(w > 0 && w + 1 < n);
    TEST_ASSERT_EQUAL_INT(PS_KIND_PRE, j[w - 1].req.kind);
    TEST_ASSERT_EQUAL_INT(PS_KIND_VERIFY, j[w + 1].req.kind);
    char line[LINE_LEN];
    TEST_ASSERT_GREATER_THAN_size_t(0, txn_log_format_txn(line, sizeof(line), &j[w]));
    TEST_ASSERT_NOT_NULL(strstr(line, " 1 WRITE 231 1 OK 00d7"));
    ps_stats_t st;
    poll_sched_get_stats(&r->sched, &st);
    TEST_ASSERT_EQUAL_UINT32(1, st.pre);
    TEST_ASSERT_EQUAL_UINT32(1, st.write);
    TEST_ASSERT_EQUAL_UINT32(1, st.verify);
    rig_finish(r);
}

/* ------------------------------------------------------------ (d) setpoint clamp */

static void set_limits(wrig_t *r, uint16_t spl, uint16_t sph)
{
    set_reg(r, REG_SPL, spl);
    set_reg(r, REG_SPH, sph);
}

TEST_CASE("e2e write d1: limits 202 and 203, boundaries and the fixed 50..400 bounds", "[e2e_write]")
{
    wrig_t *r = &g_rig;
    rig_ready(r);
    arm_ok(r, WG_ARM_MAX_S);
    expect_refused(r, WG_CMD_SP, 159, "REFUSED sp limit 160-280");
    expect_refused(r, WG_CMD_SP, 281, "REFUSED sp limit 160-280");
    expect_refused(r, WG_CMD_SP, 49, "REFUSED sp limit 160-280");
    expect_refused(r, WG_CMD_SP, 401, "REFUSED sp limit 160-280");
    expect_sequence(r, "");
    run_command(r, WG_CMD_SP, 160);
    TEST_ASSERT_EQUAL_UINT16(160, fake_raw(r, REG_SP));
    pace(r);
    run_command(r, WG_CMD_SP, 280);
    TEST_ASSERT_EQUAL_UINT16(280, fake_raw(r, REG_SP));
    pace(r);

    set_limits(r, 50, 350);   /* the widest the register map allows (203 max 350) */
    expect_refused(r, WG_CMD_SP, 49, "REFUSED sp limit 50-350");
    expect_refused(r, WG_CMD_SP, 351, "REFUSED sp limit 50-350");
    run_command(r, WG_CMD_SP, 50);
    TEST_ASSERT_EQUAL_UINT16(50, fake_raw(r, REG_SP));
    pace(r);
    run_command(r, WG_CMD_SP, 350);
    TEST_ASSERT_EQUAL_UINT16(350, fake_raw(r, REG_SP));
    rig_finish(r);
}

TEST_CASE("e2e write d2: flag 5 intersects with 245 and 246", "[e2e_write]")
{
    wrig_t *r = &g_rig;
    rig_ready(r);
    set_reg(r, REG_WEB, WEB_FLAG(5));
    arm_ok(r, WG_ARM_MAX_S);
    expect_refused(r, WG_CMD_SP, 199, "REFUSED sp limit 200-240");
    expect_refused(r, WG_CMD_SP, 241, "REFUSED sp limit 200-240");
    run_command(r, WG_CMD_SP, 200);
    pace(r);
    run_command(r, WG_CMD_SP, 240);
    TEST_ASSERT_EQUAL_UINT16(240, fake_raw(r, REG_SP));
    pace(r);
    set_reg(r, REG_SPL_W, 100);   /* wider web limits do not widen 202/203 */
    set_reg(r, REG_SPH_W, 350);
    expect_refused(r, WG_CMD_SP, 159, "REFUSED sp limit 160-280");
    expect_refused(r, WG_CMD_SP, 281, "REFUSED sp limit 160-280");
    rig_finish(r);
}

TEST_CASE("e2e write d3: flag 4 refuses the 5.0 C end", "[e2e_write]")
{
    wrig_t *r = &g_rig;
    rig_ready(r);
    set_limits(r, 50, 350);
    set_reg(r, REG_WEB, WEB_FLAG(4));
    arm_ok(r, WG_ARM_MAX_S);
    expect_refused(r, WG_CMD_SP, 50, "REFUSED sp limit 51-350");
    run_command(r, WG_CMD_SP, 51);
    TEST_ASSERT_EQUAL_UINT16(51, fake_raw(r, REG_SP));
    pace(r);
    set_limits(r, INIT_SPL, INIT_SPH);   /* flag 4 changes nothing inside the normal range */
    run_command(r, WG_CMD_SP, 160);
    TEST_ASSERT_EQUAL_UINT16(160, fake_raw(r, REG_SP));
    rig_finish(r);
}

TEST_CASE("e2e write d4: a stale limit refuses the setpoint, only needed limits count", "[e2e_write]")
{
    wrig_t *r = &g_rig;
    rig_ready(r);
    arm_ok(r, WG_ARM_MAX_S);
    reg_model_set_timeout(&r->model, REG_SPL, r->clock_ms);
    expect_refused(r, WG_CMD_SP, SP_TARGET, "REFUSED sp stale 202");
    set_reg(r, REG_SPL, INIT_SPL);
    reg_model_set_timeout(&r->model, REG_SPH, r->clock_ms);
    expect_refused(r, WG_CMD_SP, SP_TARGET, "REFUSED sp stale 203");
    set_reg(r, REG_SPH, INIT_SPH);
    reg_model_set_timeout(&r->model, REG_SPL_W, r->clock_ms);   /* flag 5 clear: 245 does not matter */
    run_command(r, WG_CMD_SP, SP_TARGET);
    pace(r);
    set_reg(r, REG_WEB, WEB_FLAG(5));
    reg_model_set_timeout(&r->model, REG_SPL_W, r->clock_ms);
    expect_refused(r, WG_CMD_SP, 220, "REFUSED sp stale 245");
    set_reg(r, REG_SPL_W, INIT_SPL_W);
    reg_model_set_timeout(&r->model, REG_SPH_W, r->clock_ms);
    expect_refused(r, WG_CMD_SP, 220, "REFUSED sp stale 246");
    set_reg(r, REG_SPH_W, INIT_SPH_W);
    reg_model_set_timeout(&r->model, REG_WEB, r->clock_ms);   /* the flags themselves must be fresh */
    expect_refused(r, WG_CMD_SP, 220, "REFUSED sp stale 247");
    rig_finish(r);
}

/* ------------------------------------------------------------ (e) register 201 read-modify-write */

TEST_CASE("e2e write e1: fan codes keep every other bit of 201", "[e2e_write]")
{
    wrig_t *r = &g_rig;
    rig_ready(r);
    arm_ok(r, WG_ARM_MAX_S);
    static const struct { uint16_t code; uint16_t expect; const char *name; const char *done; } fans[] = {
        { WG_FAN_SILENT, 0x0151, "silent", "DONE 201 0150 -> 0151 budget 39" },
        { WG_FAN_NIGHT,  0x0152, "night",  "DONE 201 0151 -> 0152 budget 38" },
        { WG_FAN_MAX,    0x0153, "max",    "DONE 201 0152 -> 0153 budget 37" },
        { WG_FAN_AUTO,   0x0150, "auto",   "DONE 201 0153 -> 0150 budget 36" },
    };
    for (size_t i = 0; i < sizeof(fans) / sizeof(fans[0]); i++) {
        char cmd[24];
        snprintf(cmd, sizeof(cmd), "CMD fan %s", fans[i].name);
        run_command(r, WG_CMD_FAN, fans[i].code);
        expect_line(r, 'W', cmd);
        expect_line(r, 'W', fans[i].done);
        TEST_ASSERT_EQUAL_HEX16(fans[i].expect, fake_raw(r, REG_PRG));
        TEST_ASSERT_EQUAL_HEX16(fans[i].expect, model_raw(r, REG_PRG));
        pace(r);
    }
    rig_finish(r);
}

TEST_CASE("e2e write e2: standby on and off change bit 7 only", "[e2e_write]")
{
    wrig_t *r = &g_rig;
    rig_ready(r);
    arm_ok(r, WG_ARM_MAX_S);
    run_command(r, WG_CMD_STANDBY, 1);
    expect_line(r, 'W', "CMD standby on");
    expect_line(r, 'W', "DONE 201 0150 -> 01d0 budget 39");
    TEST_ASSERT_EQUAL_HEX16(INIT_PRG | PRG_STANDBY, fake_raw(r, REG_PRG));
    pace(r);
    run_command(r, WG_CMD_FAN, WG_FAN_MAX);   /* standby stays on */
    TEST_ASSERT_EQUAL_HEX16(INIT_PRG | PRG_STANDBY | WG_FAN_MAX, fake_raw(r, REG_PRG));
    pace(r);
    run_command(r, WG_CMD_STANDBY, 0);
    expect_line(r, 'W', "CMD standby off");
    TEST_ASSERT_EQUAL_HEX16(INIT_PRG | WG_FAN_MAX, fake_raw(r, REG_PRG));   /* fan mode stays */
    rig_finish(r);
}

TEST_CASE("e2e write e3: undefined mode bits refuse any 201 write, no FC06, no failure", "[e2e_write]")
{
    wrig_t *r = &g_rig;
    rig_ready(r);
    set_reg(r, REG_PRG, PRG_UNDEFINED_MODE);
    arm_ok(r, WG_ARM_MAX_S);
    run_command(r, WG_CMD_FAN, WG_FAN_MAX);
    expect_line(r, 'W', "REFUSED fan undefined_mode");
    expect_sequence(r, "P201");
    pace(r);
    run_command(r, WG_CMD_STANDBY, 1);
    expect_line(r, 'W', "REFUSED standby undefined_mode");
    expect_sequence(r, "P201 P201");
    TEST_ASSERT_EQUAL_UINT32(0, fake_write_count(&r->fake));
    TEST_ASSERT_EQUAL_UINT8(0, status(r).failures);
    TEST_ASSERT_TRUE(status(r).armed);
    TEST_ASSERT_EQUAL_UINT8(WG_BUDGET_FRAMES, status(r).budget_left);
    rig_finish(r);
}

/* ------------------------------------------------------------ (f) season */

TEST_CASE("e2e write f: season accepts 0, 3, 5 and nothing else", "[e2e_write]")
{
    wrig_t *r = &g_rig;
    rig_ready(r);
    arm_ok(r, WG_ARM_MAX_S);
    static const uint16_t bad[] = { 1, 2, 4, 6, 255 };
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        size_t lines = r->n_lines;
        TEST_ASSERT_FALSE(command(r, WG_CMD_SEASON, bad[i]));
        TEST_ASSERT_EQUAL_size_t(lines, r->n_lines);   /* the console prints the usage, no W line */
    }
    expect_sequence(r, "");
    run_command(r, WG_CMD_SEASON, 5);
    expect_line(r, 'W', "CMD season 5");
    expect_line(r, 'W', "DONE 233 0003 -> 0005 budget 39");
    pace(r);
    run_command(r, WG_CMD_SEASON, 0);
    expect_line(r, 'W', "DONE 233 0005 -> 0000 budget 38");
    pace(r);
    run_command(r, WG_CMD_SEASON, 3);
    expect_line(r, 'W', "DONE 233 0000 -> 0003 budget 37");
    TEST_ASSERT_EQUAL_UINT16(3, fake_raw(r, REG_SEASON));
    rig_finish(r);
}

/* ------------------------------------------------------------ (g) unchanged value */

TEST_CASE("e2e write g: an unchanged value is a SKIP without FC06 and without budget", "[e2e_write]")
{
    wrig_t *r = &g_rig;
    rig_ready(r);
    arm_ok(r, WG_ARM_MAX_S);
    run_command(r, WG_CMD_SP, INIT_SP);
    expect_line(r, 'W', "SKIP 231 unchanged 00d2");
    pace(r);
    run_command(r, WG_CMD_STANDBY, 0);   /* already off */
    expect_line(r, 'W', "SKIP 201 unchanged 0150");
    pace(r);
    run_command(r, WG_CMD_FAN, WG_FAN_AUTO);
    pace(r);
    run_command(r, WG_CMD_SEASON, INIT_SEASON);
    expect_line(r, 'W', "SKIP 233 unchanged 0003");
    expect_sequence(r, "P231 P201 P201 P233");
    TEST_ASSERT_EQUAL_UINT32(0, fake_write_count(&r->fake));
    TEST_ASSERT_EQUAL_UINT8(WG_BUDGET_FRAMES, status(r).budget_left);
    TEST_ASSERT_EQUAL_UINT8(0, status(r).failures);
    rig_finish(r);
}

/* ------------------------------------------------------------ (h) failures */

typedef enum { H_SILENT_LANDED, H_SILENT_LOST, H_EXC, H_BAD_ECHO, H_CLAMP, H_VERIFY_TIMEOUT, H_PRE_TIMEOUT } h_case_t;

static void script_failure(wrig_t *r, h_case_t c)
{
    switch (c) {
    case H_SILENT_LANDED: fake_set_write(&r->fake, REG_SP, FAKE_WRITE_SILENT_LANDED, 0); break;
    case H_SILENT_LOST:   fake_set_write(&r->fake, REG_SP, FAKE_WRITE_SILENT_LOST, 0); break;
    case H_EXC:           fake_set_write(&r->fake, REG_SP, FAKE_WRITE_EXC, 3); break;
    case H_BAD_ECHO:      fake_set_write(&r->fake, REG_SP, FAKE_WRITE_BAD_ECHO, 0); break;
    case H_CLAMP:         fake_set_write(&r->fake, REG_SP, FAKE_WRITE_CLAMP, 200); break;
    case H_VERIFY_TIMEOUT: fake_fail_request(&r->fake, r->fake.request_count + 3); break;   /* PRE, WRITE, VERIFY */
    case H_PRE_TIMEOUT:   fake_fail_request(&r->fake, r->fake.request_count + 1); break;
    }
}

TEST_CASE("e2e write h: every failure counts once, disarms, still verifies and never resends", "[e2e_write]")
{
    wrig_t *r = &g_rig;
    static const struct {
        h_case_t c; const char *fail; const char *seq; unsigned frames; uint16_t fake_after; uint16_t model_after;
    } cases[] = {
        { H_SILENT_LANDED,  "FAIL 231 write_TIMEOUT failures 1/3",  "P231 W231 V231", 1, SP_TARGET, SP_TARGET },
        { H_SILENT_LOST,    "FAIL 231 write_TIMEOUT failures 1/3",  "P231 W231 V231", 1, INIT_SP, INIT_SP },
        { H_EXC,            "FAIL 231 write_EXC failures 1/3",      "P231 W231 V231", 1, INIT_SP, INIT_SP },
        { H_BAD_ECHO,       "FAIL 231 write_BADFRAME failures 1/3", "P231 W231 V231", 1, SP_TARGET, SP_TARGET },
        { H_CLAMP,          "FAIL 231 mismatch 00c8 failures 1/3",  "P231 W231 V231", 1, 200, 200 },
        { H_VERIFY_TIMEOUT, "FAIL 231 verify_TIMEOUT failures 1/3", "P231 W231 V231", 1, SP_TARGET, INIT_SP },
        { H_PRE_TIMEOUT,    "FAIL 231 pre_TIMEOUT failures 1/3",    "P231", 0, INIT_SP, INIT_SP },
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        rig_ready(r);
        arm_ok(r, WG_ARM_DEFAULT_S);
        script_failure(r, cases[i].c);
        run_command(r, WG_CMD_SP, SP_TARGET);
        expect_line(r, 'W', cases[i].fail);
        expect_line(r, 'A', "DISARMED failure");
        expect_sequence(r, cases[i].seq);
        TEST_ASSERT_EQUAL_UINT32(cases[i].frames, fake_write_count(&r->fake));
        TEST_ASSERT_FALSE(status(r).armed);
        TEST_ASSERT_EQUAL_UINT8(1, status(r).failures);
        TEST_ASSERT_FALSE(status(r).latched);
        TEST_ASSERT_EQUAL_UINT16(cases[i].fake_after, fake_raw(r, REG_SP));
        if (cases[i].c == H_VERIFY_TIMEOUT || cases[i].c == H_PRE_TIMEOUT) {
            /* a failed read leaves the model alone (the next poll may refresh it later) */
            rm_entry_t e;
            TEST_ASSERT_TRUE(reg_model_get(&r->model, REG_SP, &e));
            TEST_ASSERT_EQUAL_INT(RM_OK, e.avail);
            TEST_ASSERT_EQUAL_UINT16(INIT_SP, e.raw);
        } else {
            TEST_ASSERT_EQUAL_UINT16(cases[i].model_after, model_raw(r, REG_SP));   /* VERIFY: the bus is truth */
        }
        run_for(r, 10 * 60 * SECOND_MS);   /* no resend, ever */
        expect_sequence(r, cases[i].seq);
        TEST_ASSERT_EQUAL_UINT32(cases[i].frames, fake_write_count(&r->fake));
        rig_finish(r);
    }
}

/* ------------------------------------------------------------ (i) latch */

static void fail_one_write(wrig_t *r)
{
    run_for(r, PACING_MS);
    arm_ok(r, WG_ARM_DEFAULT_S);
    run_command(r, WG_CMD_SP, SP_TARGET);
}

TEST_CASE("e2e write i: the third failure latches writes until reboot", "[e2e_write]")
{
    wrig_t *r = &g_rig;
    rig_ready(r);
    fake_set_write(&r->fake, REG_SP, FAKE_WRITE_SILENT_LOST, 0);
    fail_one_write(r);
    expect_line(r, 'W', "FAIL 231 write_TIMEOUT failures 1/3");
    fail_one_write(r);
    expect_line(r, 'W', "FAIL 231 write_TIMEOUT failures 2/3");
    expect_no_line(r, 'A', "LATCHED failures 3");
    TEST_ASSERT_FALSE(status(r).latched);
    fail_one_write(r);
    expect_line(r, 'W', "FAIL 231 write_TIMEOUT failures 3/3");
    expect_line(r, 'A', "LATCHED failures 3");
    TEST_ASSERT_TRUE(status(r).latched);
    TEST_ASSERT_EQUAL_UINT8(3, status(r).failures);
    TEST_ASSERT_EQUAL_UINT32(3, fake_write_count(&r->fake));

    run_for(r, PACING_MS);
    r->n_lines = 0;
    TEST_ASSERT_FALSE(arm(r, WG_ARM_DEFAULT_S));
    expect_line(r, 'A', "REFUSED arm latched");
    expect_refused(r, WG_CMD_SP, SP_TARGET, "REFUSED sp latched");

    /* mb addr, mb silence and a new arm attempt do not clear it */
    disarm(r, WG_DISARM_ADDR);
    TEST_ASSERT_EQUAL_INT(0, poll_sched_set_addr(&r->sched, SLAVE_ADDR, r->clock_ms));
    disarm(r, WG_DISARM_SILENCE);
    run_for(r, READY_MS);
    r->n_lines = 0;
    TEST_ASSERT_FALSE(arm(r, WG_ARM_DEFAULT_S));
    expect_line(r, 'A', "REFUSED arm latched");
    TEST_ASSERT_TRUE(status(r).latched);
    TEST_ASSERT_EQUAL_UINT32(3, fake_write_count(&r->fake));
    rig_finish(r);
}

/* ------------------------------------------------------------ (j) budget */

TEST_CASE("e2e write j1: 40 frames per session, then disarm; a new arm gives a fresh 40", "[e2e_write]")
{
    wrig_t *r = &g_rig;
    rig_ready(r);
    arm_ok(r, WG_ARM_MAX_S);
    for (unsigned i = 0; i < WG_BUDGET_FRAMES; i++) {
        TEST_ASSERT_TRUE_MESSAGE(status(r).armed, "disarmed before the 40th frame");
        run_command(r, WG_CMD_SP, (i % 2 == 0) ? SP_TARGET : INIT_SP);
        pace(r);
    }
    TEST_ASSERT_EQUAL_UINT32(WG_BUDGET_FRAMES, fake_write_count(&r->fake));
    expect_line(r, 'W', "DONE 231 00d7 -> 00d2 budget 0");
    expect_line(r, 'A', "DISARMED budget");
    TEST_ASSERT_FALSE(status(r).armed);
    expect_refused(r, WG_CMD_SP, SP_TARGET, "REFUSED sp disarmed");
    TEST_ASSERT_EQUAL_UINT32(WG_BUDGET_FRAMES, fake_write_count(&r->fake));

    r->n_lines = 0;
    arm_ok(r, WG_ARM_DEFAULT_S);
    expect_line(r, 'A', "ARMED 120 s budget 40");
    TEST_ASSERT_EQUAL_size_t(1, r->n_lines);   /* the restore set is captured once only */
    run_command(r, WG_CMD_SP, SP_TARGET);
    expect_line(r, 'W', "DONE 231 00d2 -> 00d7 budget 39");
    rig_finish(r);
}

TEST_CASE("e2e write j2: re-arm keeps the budget and never shortens the deadline", "[e2e_write]")
{
    wrig_t *r = &g_rig;
    rig_ready(r);
    arm_ok(r, 120);
    for (int i = 0; i < 3; i++) {
        run_command(r, WG_CMD_SP, (i % 2 == 0) ? SP_TARGET : INIT_SP);
        pace(r);
    }
    uint32_t left_before = status(r).left_s;
    TEST_ASSERT_TRUE(left_before > 30 && left_before < 120);
    r->n_lines = 0;
    arm_ok(r, WG_ARM_MIN_S);   /* a shorter request does not shorten */
    char expect[40];
    snprintf(expect, sizeof(expect), "EXTENDED %u s left budget 37", (unsigned)left_before);
    expect_line(r, 'A', expect);
    TEST_ASSERT_EQUAL_UINT32(left_before, status(r).left_s);
    TEST_ASSERT_EQUAL_UINT8(37, status(r).budget_left);
    arm_ok(r, WG_ARM_MAX_S);
    expect_line(r, 'A', "EXTENDED 600 s left budget 37");
    TEST_ASSERT_EQUAL_UINT32(600, status(r).left_s);
    TEST_ASSERT_EQUAL_UINT8(37, status(r).budget_left);
    expect_no_line(r, 'A', "RESTORE_SET 201=0150 233=0003 231=00d2");

    disarm(r, WG_DISARM_USER);
    arm_ok(r, 120);
    expect_line(r, 'A', "ARMED 120 s budget 40");
    TEST_ASSERT_EQUAL_UINT8(40, status(r).budget_left);
    rig_finish(r);
}

/* ------------------------------------------------------------ (k) pacing */

TEST_CASE("e2e write k: 4999 ms after the last transaction is refused, 5000 ms accepted", "[e2e_write]")
{
    wrig_t *r = &g_rig;
    rig_ready(r);
    arm_ok(r, WG_ARM_MAX_S);
    run_command(r, WG_CMD_SP, SP_TARGET);
    int64_t end_ms = last_txn_end(r);   /* the VERIFY */
    r->clock_ms = end_ms + PACING_MS - 1;
    expect_refused(r, WG_CMD_SP, INIT_SP, "REFUSED sp pacing 1");
    r->clock_ms = end_ms + 1000;
    expect_refused(r, WG_CMD_STANDBY, 1, "REFUSED standby pacing 4000");
    r->clock_ms = end_ms + PACING_MS;
    TEST_ASSERT_TRUE(command(r, WG_CMD_SP, INIT_SP));
    run_until_idle(r);
    TEST_ASSERT_EQUAL_UINT16(INIT_SP, fake_raw(r, REG_SP));

    /* a SKIP counts as a command: the 5 s run from the end of its PRE */
    pace(r);
    run_command(r, WG_CMD_SP, INIT_SP);
    expect_line(r, 'W', "SKIP 231 unchanged 00d2");
    end_ms = last_txn_end(r);
    r->clock_ms = end_ms + PACING_MS - 1;
    expect_refused(r, WG_CMD_SP, SP_TARGET, "REFUSED sp pacing 1");
    r->clock_ms = end_ms + PACING_MS;
    TEST_ASSERT_TRUE(command(r, WG_CMD_SP, SP_TARGET));
    run_until_idle(r);
    rig_finish(r);
}

/* ------------------------------------------------------------ (l) timeout */

TEST_CASE("e2e write l1: the session ends at the deadline", "[e2e_write]")
{
    wrig_t *r = &g_rig;
    rig_ready(r);
    int64_t t0 = r->clock_ms;
    arm_ok(r, WG_ARM_DEFAULT_S);
    run_until(r, t0 + 119 * SECOND_MS);
    TEST_ASSERT_TRUE(status(r).armed);
    run_until(r, t0 + 125 * SECOND_MS);
    TEST_ASSERT_FALSE(status(r).armed);
    char expect[48];
    snprintf(expect, sizeof(expect), "A %lld DISARMED timeout", (long long)(t0 + 120 * SECOND_MS));
    bool found = false;
    for (size_t i = 0; i < r->n_lines; i++) {
        found |= strcmp(r->lines[i], expect) == 0;
    }
    TEST_ASSERT_TRUE_MESSAGE(found, "DISARMED timeout not printed exactly at 120 s");
    rig_finish(r);
}

TEST_CASE("e2e write l2: a command with its FC06 sent at the deadline still verifies", "[e2e_write]")
{
    wrig_t *r = &g_rig;
    rig_ready(r);
    arm_ok(r, WG_ARM_MIN_S);
    int64_t deadline = r->clock_ms + WG_ARM_MIN_S * SECOND_MS;
    r->clock_ms = deadline - 130;   /* PRE ends at -105, WRITE -5..+20, VERIFY +120 */
    run_command(r, WG_CMD_SP, SP_TARGET);
    expect_sequence(r, "P231 W231 V231");
    expect_line(r, 'A', "DISARMED timeout");
    expect_line(r, 'W', "DONE 231 00d2 -> 00d7 budget 39");
    rig_finish(r);
}

TEST_CASE("e2e write l3: a deadline before the WRITE aborts the command without FC06", "[e2e_write]")
{
    wrig_t *r = &g_rig;
    rig_ready(r);
    arm_ok(r, WG_ARM_MIN_S);
    int64_t deadline = r->clock_ms + WG_ARM_MIN_S * SECOND_MS;
    r->clock_ms = deadline - 50;   /* PRE ends at -25, the WRITE would start at +75 */
    run_command(r, WG_CMD_SP, SP_TARGET);
    expect_sequence(r, "P231");
    expect_line(r, 'W', "ABORTED 231 timeout");
    TEST_ASSERT_EQUAL_UINT32(0, fake_write_count(&r->fake));
    TEST_ASSERT_EQUAL_UINT8(0, status(r).failures);
    rig_finish(r);
}

/* ------------------------------------------------------------ (m) disarm triggers */

TEST_CASE("e2e write m1: every hook trigger disarms and later commands are refused", "[e2e_write]")
{
    wrig_t *r = &g_rig;
    static const struct { wg_reason_t reason; const char *line; } cases[] = {
        { WG_DISARM_USER, "DISARMED user" }, { WG_DISARM_POLL_OFF, "DISARMED poll_off" },
        { WG_DISARM_SILENCE, "DISARMED silence" }, { WG_DISARM_ADDR, "DISARMED addr" },
        { WG_DISARM_LATCHED, "DISARMED latched" }, { WG_DISARM_LINK, "DISARMED link" },
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        rig_ready(r);
        arm_ok(r, WG_ARM_DEFAULT_S);
        disarm(r, cases[i].reason);
        expect_line(r, 'A', cases[i].line);
        TEST_ASSERT_FALSE(status(r).armed);
        expect_refused(r, WG_CMD_SP, SP_TARGET, "REFUSED sp disarmed");
        TEST_ASSERT_EQUAL_UINT32(0, fake_write_count(&r->fake));
        size_t lines = r->n_lines;
        disarm(r, cases[i].reason);   /* a second hook call is silent */
        TEST_ASSERT_EQUAL_size_t(lines, r->n_lines);
    }
    rig_finish(r);
}

TEST_CASE("e2e write m2: a health change and a poll showing a web flag disarm", "[e2e_write]")
{
    wrig_t *r = &g_rig;
    rig_ready(r);
    arm_ok(r, WG_ARM_MAX_S);
    fake_set_dead(&r->fake, r->clock_ms, r->clock_ms + 1000 * SECOND_MS);
    run_until_health(r, PS_DEGRADED);
    ps_txn_t txn;
    int64_t wait_ms;
    wstep(r, &txn, &wait_ms);
    expect_line(r, 'A', "DISARMED health");
    TEST_ASSERT_FALSE(status(r).armed);

    rig_ready(r);
    arm_ok(r, WG_ARM_MAX_S);
    fake_set_reg(&r->fake, REG_WEB, WEB_FLAG(6), FAKE_DEFAULT_LATENCY_MS);
    run_for(r, 70 * SECOND_MS);   /* 247 is a slow register: next poll within 60 s */
    expect_line(r, 'A', "DISARMED web_flags");
    TEST_ASSERT_FALSE(status(r).armed);
    expect_refused(r, WG_CMD_SP, SP_TARGET, "REFUSED sp disarmed");
    rig_finish(r);
}

TEST_CASE("e2e write m3: a disarm between PRE and WRITE aborts without FC06", "[e2e_write]")
{
    wrig_t *r = &g_rig;
    static const wg_reason_t reasons[] = { WG_DISARM_USER, WG_DISARM_LINK, WG_DISARM_ADDR, WG_DISARM_HEALTH };
    static const char *const names[] = { "user", "link", "addr", "health" };
    for (size_t i = 0; i < sizeof(reasons) / sizeof(reasons[0]); i++) {
        rig_ready(r);
        arm_ok(r, WG_ARM_DEFAULT_S);
        TEST_ASSERT_TRUE(command(r, WG_CMD_SP, SP_TARGET));
        run_until_txn(r, PS_KIND_PRE);
        disarm(r, reasons[i]);
        run_for(r, 10 * SECOND_MS);
        char expect[32];
        snprintf(expect, sizeof(expect), "ABORTED 231 %s", names[i]);
        expect_line(r, 'W', expect);
        expect_sequence(r, "P231");
        TEST_ASSERT_EQUAL_UINT32(0, fake_write_count(&r->fake));
        TEST_ASSERT_EQUAL_UINT8(0, status(r).failures);
        TEST_ASSERT_FALSE(busy(r));
        rig_finish(r);
    }
}

TEST_CASE("e2e write m4: a disarm after the WRITE still verifies; a vanished bus loses the VERIFY", "[e2e_write]")
{
    wrig_t *r = &g_rig;
    static const struct { wg_reason_t reason; bool lost; } cases[] = {
        { WG_DISARM_USER, false }, { WG_DISARM_HEALTH, false }, { WG_DISARM_WEB_FLAGS, false },
        { WG_DISARM_POLL_OFF, true }, { WG_DISARM_SILENCE, true }, { WG_DISARM_ADDR, true },
        { WG_DISARM_LATCHED, true }, { WG_DISARM_LINK, true },
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        rig_ready(r);
        arm_ok(r, WG_ARM_DEFAULT_S);
        TEST_ASSERT_TRUE(command(r, WG_CMD_SP, SP_TARGET));
        run_until_txn(r, PS_KIND_WRITE);
        disarm(r, cases[i].reason);
        run_until_idle(r);
        TEST_ASSERT_EQUAL_UINT32(1, fake_write_count(&r->fake));
        if (cases[i].lost) {
            expect_line(r, 'W', "FAIL 231 verify_lost failures 1/3");
            expect_sequence(r, "P231 W231");
            TEST_ASSERT_EQUAL_UINT8(1, status(r).failures);
        } else {
            expect_line(r, 'W', "DONE 231 00d2 -> 00d7 budget 39");
            expect_sequence(r, "P231 W231 V231");
            TEST_ASSERT_EQUAL_UINT8(0, status(r).failures);
        }
        TEST_ASSERT_FALSE(status(r).armed);
        rig_finish(r);
    }
}

/* ------------------------------------------------------------ (m5) bus loss without any hook */

typedef enum { LOSS_STOP, LOSS_LATCH, LOSS_ADDR } bus_loss_t;

static const struct { bus_loss_t loss; const char *reason; } k_losses[] = {
    { LOSS_STOP, "poll_off" }, { LOSS_LATCH, "latched" }, { LOSS_ADDR, "addr" },
};

/* Only the scheduler changes; no write_guard hook is called. */
static void lose_bus(wrig_t *r, bus_loss_t loss)
{
    switch (loss) {
    case LOSS_STOP:  poll_sched_stop(&r->sched); break;
    case LOSS_LATCH: poll_sched_latch(&r->sched); break;
    case LOSS_ADDR:  TEST_ASSERT_EQUAL_INT(0, poll_sched_set_addr(&r->sched, SLAVE_ADDR + 1, r->clock_ms)); break;
    }
}

static void step_once(wrig_t *r)
{
    ps_txn_t txn;
    int64_t wait_ms;
    wstep(r, &txn, &wait_ms);
}

TEST_CASE("e2e write m5: a pending VERIFY is lost when the bus goes away, with no hook called", "[e2e_write]")
{
    wrig_t *r = &g_rig;
    for (size_t i = 0; i < sizeof(k_losses) / sizeof(k_losses[0]); i++) {
        for (int user_disarmed = 0; user_disarmed < 2; user_disarmed++) {
            rig_ready(r);
            arm_ok(r, WG_ARM_DEFAULT_S);
            TEST_ASSERT_TRUE(command(r, WG_CMD_SP, SP_TARGET));
            run_until_txn(r, PS_KIND_WRITE);
            if (user_disarmed) {
                disarm(r, WG_DISARM_USER);
            }
            lose_bus(r, k_losses[i].loss);
            step_once(r);
            TEST_ASSERT_FALSE(busy(r));
            expect_line(r, 'W', "FAIL 231 verify_lost failures 1/3");
            if (!user_disarmed) {
                char expect[32];
                snprintf(expect, sizeof(expect), "DISARMED %s", k_losses[i].reason);
                expect_line(r, 'A', expect);
            }
            run_for(r, 10 * SECOND_MS);
            expect_sequence(r, "P231 W231");   /* no VERIFY to either address */
            TEST_ASSERT_EQUAL_UINT32(1, fake_write_count(&r->fake));
            TEST_ASSERT_EQUAL_UINT8(1, status(r).failures);
            rig_finish(r);
        }
    }
}

TEST_CASE("e2e write m6: a pending PRE or WRITE is aborted when the bus goes away, with no hook called", "[e2e_write]")
{
    wrig_t *r = &g_rig;
    for (size_t i = 0; i < sizeof(k_losses) / sizeof(k_losses[0]); i++) {
        char expect[32];
        snprintf(expect, sizeof(expect), "ABORTED 231 %s", k_losses[i].reason);

        if (k_losses[i].loss != LOSS_ADDR) {   /* the PRE itself records the address, so it cannot differ */
            rig_ready(r);   /* PRE pending */
            arm_ok(r, WG_ARM_DEFAULT_S);
            TEST_ASSERT_TRUE(command(r, WG_CMD_SP, SP_TARGET));
            lose_bus(r, k_losses[i].loss);
            step_once(r);
            expect_line(r, 'W', expect);
            TEST_ASSERT_FALSE(busy(r));
            run_for(r, 10 * SECOND_MS);
            expect_sequence(r, "");
            TEST_ASSERT_EQUAL_UINT32(0, fake_write_count(&r->fake));
            TEST_ASSERT_EQUAL_UINT8(0, status(r).failures);
            rig_finish(r);
        }

        rig_ready(r);   /* WRITE pending */
        arm_ok(r, WG_ARM_DEFAULT_S);
        TEST_ASSERT_TRUE(command(r, WG_CMD_SP, SP_TARGET));
        run_until_txn(r, PS_KIND_PRE);
        lose_bus(r, k_losses[i].loss);
        step_once(r);
        expect_line(r, 'W', expect);
        TEST_ASSERT_FALSE(busy(r));
        run_for(r, 10 * SECOND_MS);
        expect_sequence(r, "P231");
        TEST_ASSERT_EQUAL_UINT32(0, fake_write_count(&r->fake));
        TEST_ASSERT_EQUAL_UINT8(0, status(r).failures);
        rig_finish(r);
    }
}

TEST_CASE("e2e write m6b: a user disarm then poll_off before the PRE reports poll_off, no FC06", "[e2e_write]")
{
    wrig_t *r = &g_rig;
    rig_ready(r);
    arm_ok(r, WG_ARM_DEFAULT_S);
    TEST_ASSERT_TRUE(command(r, WG_CMD_SP, SP_TARGET));   /* PRE pending */
    disarm(r, WG_DISARM_USER);
    disarm(r, WG_DISARM_POLL_OFF);   /* already disarmed: the hook still records the bus reason */
    step_once(r);
    expect_line(r, 'W', "ABORTED 231 poll_off");
    TEST_ASSERT_FALSE(busy(r));
    run_for(r, 10 * SECOND_MS);
    expect_sequence(r, "");
    TEST_ASSERT_EQUAL_UINT32(0, fake_write_count(&r->fake));
    TEST_ASSERT_EQUAL_UINT8(0, status(r).failures);
    rig_finish(r);
}

TEST_CASE("e2e write m7: DEGRADED does not stop a pending VERIFY", "[e2e_write]")
{
    wrig_t *r = &g_rig;
    for (int user_disarmed = 0; user_disarmed < 2; user_disarmed++) {
        rig_ready(r);
        arm_ok(r, WG_ARM_DEFAULT_S);
        TEST_ASSERT_TRUE(command(r, WG_CMD_SP, SP_TARGET));
        run_until_txn(r, PS_KIND_WRITE);
        if (user_disarmed) {
            disarm(r, WG_DISARM_USER);
        }
        r->sched.health = PS_DEGRADED;   /* health is only read by the guard; no bus fault needed */
        run_until_idle(r);
        expect_line(r, 'W', "DONE 231 00d2 -> 00d7 budget 39");
        expect_sequence(r, "P231 W231 V231");
        TEST_ASSERT_EQUAL_UINT8(0, status(r).failures);
        rig_finish(r);
    }
}

/* ------------------------------------------------------------ (n) restore */

static void expect_sequence_tail(const wrig_t *r, const char *tail)
{
    char seq[512];
    sequence_of(r, seq, sizeof(seq));
    size_t ls = strlen(seq), lt = strlen(tail);
    TEST_ASSERT_TRUE_MESSAGE(ls >= lt && strcmp(seq + ls - lt, tail) == 0, seq);
}

TEST_CASE("e2e write n1: restore writes 201, 233, 231 in that order, 5 s apart", "[e2e_write]")
{
    wrig_t *r = &g_rig;
    rig_ready(r);
    arm_ok(r, WG_ARM_MAX_S);
    run_command(r, WG_CMD_SP, SP_TARGET);
    pace(r);
    run_command(r, WG_CMD_SEASON, 5);
    pace(r);
    run_command(r, WG_CMD_STANDBY, 1);
    pace(r);
    run_command(r, WG_CMD_FAN, WG_FAN_MAX);
    pace(r);
    run_command(r, WG_CMD_RESTORE, 0);
    expect_line(r, 'W', "CMD restore");
    expect_sequence_tail(r, "P201 W201 V201 P233 W233 V233 P231 W231 V231");
    TEST_ASSERT_EQUAL_HEX16(INIT_PRG, fake_raw(r, REG_PRG));
    TEST_ASSERT_EQUAL_UINT16(INIT_SEASON, fake_raw(r, REG_SEASON));
    TEST_ASSERT_EQUAL_UINT16(INIT_SP, fake_raw(r, REG_SP));
    TEST_ASSERT_EQUAL_UINT32(7, fake_write_count(&r->fake));
    TEST_ASSERT_FALSE(busy(r));
    TEST_ASSERT_TRUE(status(r).armed);
    rig_finish(r);   /* includes the 5 s spacing of the three restore frames */
}

TEST_CASE("e2e write n2: restore writes only the registers that differ", "[e2e_write]")
{
    wrig_t *r = &g_rig;
    rig_ready(r);
    arm_ok(r, WG_ARM_MAX_S);
    run_command(r, WG_CMD_SP, SP_TARGET);
    pace(r);
    run_command(r, WG_CMD_RESTORE, 0);
    expect_sequence(r, "P231 W231 V231 P231 W231 V231");
    TEST_ASSERT_EQUAL_UINT16(INIT_SP, fake_raw(r, REG_SP));
    pace(r);
    run_command(r, WG_CMD_RESTORE, 0);   /* nothing differs: no transaction at all */
    expect_sequence(r, "P231 W231 V231 P231 W231 V231");
    TEST_ASSERT_FALSE(busy(r));
    rig_finish(r);
}

TEST_CASE("e2e write n3: restore of 201 touches bits 0-2 and 7 only", "[e2e_write]")
{
    wrig_t *r = &g_rig;
    rig_ready(r);
    arm_ok(r, WG_ARM_MAX_S);
    set_reg(r, REG_PRG, 0x01C3);   /* the panel user cleared LOCK, set mode 3 and standby */
    run_for(r, 6 * SECOND_MS);
    run_command(r, WG_CMD_RESTORE, 0);
    TEST_ASSERT_EQUAL_HEX16(0x0140, fake_raw(r, REG_PRG));   /* LOCK stays as the user left it */
    expect_line(r, 'W', "DONE 201 01c3 -> 0140 budget 39");
    rig_finish(r);
}

TEST_CASE("e2e write n4: restore set is taken at the first arm only and again after mb addr", "[e2e_write]")
{
    wrig_t *r = &g_rig;
    rig_ready(r);
    arm_ok(r, WG_ARM_DEFAULT_S);
    expect_line(r, 'A', "RESTORE_SET 201=0150 233=0003 231=00d2");
    disarm(r, WG_DISARM_USER);
    set_reg(r, REG_SEASON, 5);
    r->n_lines = 0;
    arm_ok(r, WG_ARM_DEFAULT_S);
    expect_no_line(r, 'A', "RESTORE_SET 201=0150 233=0005 231=00d2");
    TEST_ASSERT_EQUAL_size_t(1, r->n_lines);
    TEST_ASSERT_TRUE(status(r).restore_valid);
    TEST_ASSERT_EQUAL_UINT16(3, status(r).restore_233);

    disarm(r, WG_DISARM_ADDR);
    TEST_ASSERT_FALSE(status(r).restore_valid);
    TEST_ASSERT_EQUAL_INT(0, poll_sched_set_addr(&r->sched, SLAVE_ADDR, r->clock_ms));
    run_for(r, READY_MS);
    arm_ok(r, WG_ARM_DEFAULT_S);
    expect_line(r, 'A', "RESTORE_SET 201=0150 233=0005 231=00d2");
    rig_finish(r);
}

/* ------------------------------------------------------------ (o) lines */

TEST_CASE("e2e write o: T lines of PRE, WRITE, VERIFY round-trip through txn_log", "[e2e_write]")
{
    wrig_t *r = &g_rig;
    rig_ready(r);
    arm_ok(r, WG_ARM_DEFAULT_S);
    run_command(r, WG_CMD_SP, SP_TARGET);
    const ps_txn_t *j;
    size_t n = fake_journal(&r->fake, &j);
    size_t seen = 0;
    for (size_t i = 0; i < n; i++) {
        ps_kind_t k = j[i].req.kind;
        if (k != PS_KIND_PRE && k != PS_KIND_WRITE && k != PS_KIND_VERIFY) {
            continue;
        }
        char line[LINE_LEN];
        tl_rec_t rec;
        TEST_ASSERT_GREATER_THAN_size_t(0, txn_log_format_txn(line, sizeof(line), &j[i]));
        TEST_ASSERT_EQUAL_INT(1, txn_log_parse(line, &rec));
        TEST_ASSERT_EQUAL_INT(k, rec.txn.req.kind);
        TEST_ASSERT_EQUAL_UINT16(j[i].val[0], rec.txn.val[0]);
        seen++;
    }
    TEST_ASSERT_EQUAL_size_t(3, seen);
    rig_finish(r);
}
