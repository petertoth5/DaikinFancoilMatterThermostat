/* test_write_guard.c - host Unity tests for the write_guard log lines (spec 019, WR11).
 * The behaviour of the guard itself is covered end to end in test/host/main/test_e2e_write.c. */

#include <stdio.h>
#include <string.h>
#include "unity.h"
#include "write_guard.h"

static void expect_line(const wg_event_t *e, const char *expected)
{
    char buf[128];
    size_t n = write_guard_format_event(buf, sizeof(buf), e);
    TEST_ASSERT_EQUAL_STRING(expected, buf);
    TEST_ASSERT_EQUAL_UINT32(strlen(expected), n);
}

static wg_event_t ev(wg_ev_kind_t kind)
{
    wg_event_t e;
    memset(&e, 0, sizeof(e));
    e.kind = kind;
    e.t_ms = 123456;
    return e;
}

TEST_CASE("write_guard format: arm session lines", "[write_guard]")
{
    wg_event_t e = ev(WG_EV_ARMED);
    e.seconds = 120;
    e.budget = 40;
    expect_line(&e, "A 123456 ARMED 120 s budget 40");
    e = ev(WG_EV_EXTENDED);
    e.seconds = 87;
    e.budget = 33;
    expect_line(&e, "A 123456 EXTENDED 87 s left budget 33");
    e = ev(WG_EV_RESTORE_SET);
    e.restore_201 = 0x0150;
    e.restore_233 = 3;
    e.restore_231 = 0x00d2;
    expect_line(&e, "A 123456 RESTORE_SET 201=0150 233=0003 231=00d2");
    e = ev(WG_EV_LATCHED);
    e.failures = 3;
    expect_line(&e, "A 123456 LATCHED failures 3");
}

TEST_CASE("write_guard format: arm refusals", "[write_guard]")
{
    wg_event_t e = ev(WG_EV_ARM_REFUSED);
    e.refuse = WG_REFUSE_LATCHED;
    expect_line(&e, "A 123456 REFUSED arm latched");
    e.refuse = WG_REFUSE_HEALTH;
    expect_line(&e, "A 123456 REFUSED arm health");
    e.refuse = WG_REFUSE_STALE;
    e.detail = 203;
    expect_line(&e, "A 123456 REFUSED arm stale 203");
    e.refuse = WG_REFUSE_WEB_FLAG;
    e.detail = 6;
    expect_line(&e, "A 123456 REFUSED arm web_flag 6");
}

TEST_CASE("write_guard format: every disarm reason", "[write_guard]")
{
    static const char *const names[] = { "user", "timeout", "failure", "budget", "health", "poll_off",
                                         "silence", "addr", "latched", "link", "web_flags" };
    for (int r = WG_DISARM_USER; r <= WG_DISARM_WEB_FLAGS; r++) {
        wg_event_t e = ev(WG_EV_DISARMED);
        e.reason = (wg_reason_t)r;
        char expected[48];
        snprintf(expected, sizeof(expected), "A 123456 DISARMED %s", names[r]);
        expect_line(&e, expected);
    }
}

TEST_CASE("write_guard format: command lines", "[write_guard]")
{
    static const struct { wg_cmd_type_t cmd; uint16_t arg; const char *line; } cases[] = {
        { WG_CMD_SP, 215, "W 123456 CMD sp 215" },
        { WG_CMD_STANDBY, 1, "W 123456 CMD standby on" },
        { WG_CMD_STANDBY, 0, "W 123456 CMD standby off" },
        { WG_CMD_FAN, WG_FAN_AUTO, "W 123456 CMD fan auto" },
        { WG_CMD_FAN, WG_FAN_SILENT, "W 123456 CMD fan silent" },
        { WG_CMD_FAN, WG_FAN_NIGHT, "W 123456 CMD fan night" },
        { WG_CMD_FAN, WG_FAN_MAX, "W 123456 CMD fan max" },
        { WG_CMD_SEASON, 5, "W 123456 CMD season 5" },
        { WG_CMD_RESTORE, 0, "W 123456 CMD restore" },
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        wg_event_t e = ev(WG_EV_CMD);
        e.cmd = cases[i].cmd;
        e.arg = cases[i].arg;
        expect_line(&e, cases[i].line);
    }
}

TEST_CASE("write_guard format: command refusals", "[write_guard]")
{
    wg_event_t e = ev(WG_EV_REFUSED);
    e.cmd = WG_CMD_SP;
    static const struct { wg_refuse_t why; const char *line; } plain[] = {
        { WG_REFUSE_DISARMED, "W 123456 REFUSED sp disarmed" },
        { WG_REFUSE_LATCHED, "W 123456 REFUSED sp latched" },
        { WG_REFUSE_BUSY, "W 123456 REFUSED sp busy" },
        { WG_REFUSE_UNDEFINED_MODE, "W 123456 REFUSED sp undefined_mode" },
    };
    for (size_t i = 0; i < sizeof(plain) / sizeof(plain[0]); i++) {
        e.refuse = plain[i].why;
        expect_line(&e, plain[i].line);
    }
    e.refuse = WG_REFUSE_PACING;
    e.detail = 4321;
    expect_line(&e, "W 123456 REFUSED sp pacing 4321");
    e.refuse = WG_REFUSE_STALE;
    e.detail = 202;
    expect_line(&e, "W 123456 REFUSED sp stale 202");
    e.refuse = WG_REFUSE_WEB_FLAG;
    e.detail = 8;
    expect_line(&e, "W 123456 REFUSED sp web_flag 8");
    e.refuse = WG_REFUSE_LIMIT;
    e.lo = 160;
    e.hi = 280;
    expect_line(&e, "W 123456 REFUSED sp limit 160-280");
    e.cmd = WG_CMD_RESTORE;
    e.refuse = WG_REFUSE_DISARMED;
    expect_line(&e, "W 123456 REFUSED restore disarmed");
}

TEST_CASE("write_guard format: result lines", "[write_guard]")
{
    wg_event_t e = ev(WG_EV_SKIP);
    e.reg = 201;
    e.old_raw = 0x0150;
    expect_line(&e, "W 123456 SKIP 201 unchanged 0150");
    e = ev(WG_EV_ABORTED);
    e.reg = 231;
    e.reason = WG_DISARM_TIMEOUT;
    expect_line(&e, "W 123456 ABORTED 231 timeout");
    e = ev(WG_EV_DONE);
    e.reg = 233;
    e.old_raw = 3;
    e.new_raw = 5;
    e.budget = 12;
    expect_line(&e, "W 123456 DONE 233 0003 -> 0005 budget 12");
}

TEST_CASE("write_guard format: failure lines", "[write_guard]")
{
    wg_event_t e = ev(WG_EV_FAIL);
    e.reg = 231;
    e.failures = 2;
    e.fail = WG_FAIL_PRE;
    e.res = PS_RES_TIMEOUT;
    expect_line(&e, "W 123456 FAIL 231 pre_TIMEOUT failures 2/3");
    e.fail = WG_FAIL_WRITE;
    e.res = PS_RES_EXCEPTION;
    expect_line(&e, "W 123456 FAIL 231 write_EXC failures 2/3");
    e.fail = WG_FAIL_WRITE;
    e.res = PS_RES_BAD_FRAME;
    expect_line(&e, "W 123456 FAIL 231 write_BADFRAME failures 2/3");
    e.fail = WG_FAIL_VERIFY;
    e.res = PS_RES_ERROR;
    expect_line(&e, "W 123456 FAIL 231 verify_ERROR failures 2/3");
    e.fail = WG_FAIL_MISMATCH;
    e.old_raw = 0x00c8;
    expect_line(&e, "W 123456 FAIL 231 mismatch 00c8 failures 2/3");
    e.fail = WG_FAIL_VERIFY_LOST;
    expect_line(&e, "W 123456 FAIL 231 verify_lost failures 2/3");
}

TEST_CASE("write_guard format: small buffers and malformed events give 0", "[write_guard]")
{
    wg_event_t e = ev(WG_EV_ARMED);
    e.seconds = 120;
    e.budget = 40;
    char buf[8];
    memset(buf, 'x', sizeof(buf));
    TEST_ASSERT_EQUAL_UINT32(0, write_guard_format_event(buf, sizeof(buf), &e));
    TEST_ASSERT_EQUAL_CHAR('\0', buf[0]);
    TEST_ASSERT_EQUAL_UINT32(0, write_guard_format_event(buf, 0, &e));
    char big[64];
    TEST_ASSERT_EQUAL_UINT32(strlen("A 123456 ARMED 120 s budget 40"), write_guard_format_event(big, 31, &e));
    TEST_ASSERT_EQUAL_UINT32(0, write_guard_format_event(big, 30, &e));
    e = ev(WG_EV_DISARMED);
    e.reason = (wg_reason_t)99;
    TEST_ASSERT_EQUAL_UINT32(0, write_guard_format_event(big, sizeof(big), &e));
    e = ev(WG_EV_REFUSED);
    e.refuse = (wg_refuse_t)99;
    TEST_ASSERT_EQUAL_UINT32(0, write_guard_format_event(big, sizeof(big), &e));
    e = ev((wg_ev_kind_t)99);
    TEST_ASSERT_EQUAL_UINT32(0, write_guard_format_event(big, sizeof(big), &e));
}

TEST_CASE("write_guard: init, status and may_transmit", "[write_guard]")
{
    write_guard_t w;
    write_guard_init(&w);
    wg_status_t st;
    write_guard_get_status(&w, 1000, &st);
    TEST_ASSERT_FALSE(st.armed);
    TEST_ASSERT_EQUAL_UINT32(0, st.left_s);
    TEST_ASSERT_EQUAL_UINT8(WG_BUDGET_FRAMES, st.budget_left);
    TEST_ASSERT_EQUAL_UINT8(0, st.failures);
    TEST_ASSERT_FALSE(st.latched);
    TEST_ASSERT_FALSE(st.restore_valid);

    poll_sched_t s;
    ps_config_t c = { .addr = 1, .gap_ms = 100, .fast_ms = 5000, .slow_ms = 60000,
                      .offline_probe_ms = 30000, .retries = 1, .offline_after = 5 };
    TEST_ASSERT_EQUAL_INT(0, poll_sched_init(&s, &c));
    poll_sched_start(&s, 0);
    TEST_ASSERT_FALSE(write_guard_may_transmit(&w, &s));   /* disarmed */
    w.armed = true;
    w.deadline_ms = 120000;
    TEST_ASSERT_TRUE(write_guard_may_transmit(&w, &s));
    write_guard_get_status(&w, 119001, &st);
    TEST_ASSERT_EQUAL_UINT32(1, st.left_s);   /* rounded up */
    w.budget_left = 0;
    TEST_ASSERT_FALSE(write_guard_may_transmit(&w, &s));
    w.budget_left = 1;
    w.latched = true;
    TEST_ASSERT_FALSE(write_guard_may_transmit(&w, &s));
    w.latched = false;
    poll_sched_stop(&s);
    TEST_ASSERT_FALSE(write_guard_may_transmit(&w, &s));   /* not ONLINE */
}
