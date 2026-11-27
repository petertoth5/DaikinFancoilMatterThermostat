/* test_txn_log.c - host Unity tests for txn_log (format v1). */

#include <stdio.h>
#include <string.h>
#include "unity.h"
#include "txn_log.h"

static ps_txn_t make_txn(ps_kind_t kind, uint16_t reg, uint16_t qty, ps_res_t res)
{
    ps_txn_t t;
    memset(&t, 0, sizeof(t));
    t.req.addr = 1;
    t.req.reg = reg;
    t.req.qty = qty;
    t.req.kind = kind;
    t.res = res;
    t.t_start_ms = 120345;
    t.t_end_ms = 120345 + 24;
    return t;
}

static void assert_format(const ps_txn_t *t, const char *expected)
{
    char buf[160];
    size_t n = txn_log_format_txn(buf, sizeof(buf), t);
    TEST_ASSERT_EQUAL_STRING(expected, buf);
    TEST_ASSERT_EQUAL_UINT32(strlen(expected), n);
}

TEST_CASE("txn_log: exact strings per result", "[txn_log]")
{
    ps_txn_t t = make_txn(PS_KIND_POLL, 0, 1, PS_RES_OK);
    t.val[0] = 0x00d7;
    assert_format(&t, "T 120345 24 1 POLL 000 1 OK 00d7");

    t = make_txn(PS_KIND_SWEEP, 104, 1, PS_RES_EXCEPTION);
    t.t_start_ms = 121002;
    t.t_end_ms = 121002 + 31;
    t.exc_code = 2;
    assert_format(&t, "T 121002 31 1 SWEEP 104 1 EXC 02");

    t = make_txn(PS_KIND_RETRY, 8, 1, PS_RES_TIMEOUT);
    assert_format(&t, "T 120345 24 1 RETRY 008 1 TIMEOUT");
    t = make_txn(PS_KIND_HEALTH, 0, 1, PS_RES_BAD_FRAME);
    assert_format(&t, "T 120345 24 1 HEALTH 000 1 BADFRAME");
    t = make_txn(PS_KIND_POLL, 247, 1, PS_RES_ERROR);
    assert_format(&t, "T 120345 24 1 POLL 247 1 ERROR");
}

TEST_CASE("txn_log: manual qty 4 with several data words", "[txn_log]")
{
    ps_txn_t t = make_txn(PS_KIND_MANUAL, 242, 4, PS_RES_OK);
    t.req.addr = 247;
    t.val[0] = 0xABCD;
    t.val[1] = 0;
    t.val[2] = 1;
    t.val[3] = 0xffff;
    assert_format(&t, "T 120345 24 247 MANUAL 242 4 OK abcd 0000 0001 ffff");
}

TEST_CASE("txn_log: health lines", "[txn_log]")
{
    static const char *const expect[] = { "H 5 STOPPED", "H 5 ONLINE", "H 5 DEGRADED", "H 5 OFFLINE", "H 5 LATCHED" };
    char buf[64];
    for (int h = PS_STOPPED; h <= PS_LATCHED; h++) {
        size_t n = txn_log_format_health(buf, sizeof(buf), 5, (ps_health_t)h);
        TEST_ASSERT_EQUAL_STRING(expect[h], buf);
        TEST_ASSERT_EQUAL_UINT32(strlen(expect[h]), n);
    }
}

TEST_CASE("txn_log: round trip parse(format(x)) == x", "[txn_log]")
{
    ps_txn_t in[6];
    in[0] = make_txn(PS_KIND_POLL, 0, 1, PS_RES_OK);
    in[0].val[0] = 215;
    in[1] = make_txn(PS_KIND_MANUAL, 242, 3, PS_RES_OK);
    in[1].val[0] = 1; in[1].val[1] = 0x8000; in[1].val[2] = 0xfffe;
    in[2] = make_txn(PS_KIND_SWEEP, 104, 1, PS_RES_EXCEPTION);
    in[2].exc_code = 0x0b;
    in[3] = make_txn(PS_KIND_RETRY, 201, 1, PS_RES_TIMEOUT);
    in[4] = make_txn(PS_KIND_HEALTH, 0, 1, PS_RES_BAD_FRAME);
    in[5] = make_txn(PS_KIND_POLL, 231, 1, PS_RES_ERROR);
    for (size_t i = 0; i < 6; i++) {
        char buf[160];
        tl_rec_t rec;
        txn_log_format_txn(buf, sizeof(buf), &in[i]);
        TEST_ASSERT_EQUAL(1, txn_log_parse(buf, &rec));
        TEST_ASSERT_EQUAL(TL_TXN, rec.type);
        TEST_ASSERT_EQUAL_MEMORY(&in[i], &rec.txn, sizeof(ps_txn_t));
    }
    for (int h = PS_STOPPED; h <= PS_LATCHED; h++) {
        char buf[64];
        tl_rec_t rec;
        txn_log_format_health(buf, sizeof(buf), 987654321012LL, (ps_health_t)h);
        TEST_ASSERT_EQUAL(1, txn_log_parse(buf, &rec));
        TEST_ASSERT_EQUAL(TL_HEALTH, rec.type);
        TEST_ASSERT_EQUAL_INT64(987654321012LL, rec.t_ms);
        TEST_ASSERT_EQUAL(h, rec.health);
    }
}

TEST_CASE("txn_log: parse accepts trailing newline variants", "[txn_log]")
{
    tl_rec_t rec;
    TEST_ASSERT_EQUAL(1, txn_log_parse("T 1 2 1 POLL 000 1 OK 00d7\n", &rec));
    TEST_ASSERT_EQUAL(1, txn_log_parse("T 1 2 1 POLL 000 1 OK 00d7\r\n", &rec));
    TEST_ASSERT_EQUAL_INT64(3, rec.txn.t_end_ms);
    TEST_ASSERT_EQUAL(1, txn_log_parse("H 7 ONLINE\r\n", &rec));
}

TEST_CASE("txn_log: ignored lines", "[txn_log]")
{
    tl_rec_t rec;
    TEST_ASSERT_EQUAL(0, txn_log_parse("I (123) foo", &rec));
    TEST_ASSERT_EQUAL(0, txn_log_parse("", &rec));
    TEST_ASSERT_EQUAL(0, txn_log_parse("\n", &rec));
    TEST_ASSERT_EQUAL(0, txn_log_parse("# dmt-probe v1 addr=1 baud=9600 fw=x", &rec));
    TEST_ASSERT_EQUAL(0, txn_log_parse("T", &rec));
    TEST_ASSERT_EQUAL(0, txn_log_parse("Tx 1 2", &rec));
}

TEST_CASE("txn_log: malformed lines", "[txn_log]")
{
    static const char *const bad[] = {
        "T 1 2 1 POLL 000 1",                      /* too few fields */
        "T 1 2 1 POLL 000 1 OK",                   /* missing data */
        "T 1 2 1 POLL 000 1 OK 00d7 0001",         /* too much data */
        "T 1 2 1 POLL 000 2 OK 00d7",              /* data count != qty */
        "T 1 2 1 POLL 000 1 OK 00dz",              /* non-hex */
        "T 1 2 1 POLL 000 1 OK d7",                /* wrong data width */
        "T 1 2 1 NOPE 000 1 OK 00d7",              /* unknown kind */
        "T 1 2 1 POLL 000 1 FAIL",                 /* unknown result */
        "T 1 2 1 POLL 000 1 TIMEOUT 00d7",         /* data on a no-data result */
        "T 1 2 1 POLL 000 1 EXC",                  /* missing code */
        "T 1 2 1 POLL 000 1 EXC 2",                /* code width */
        "T 1 2 1 POLL 000 1 EXC 02 03",            /* extra */
        "T 1 2 1 POLL 000 0 TIMEOUT",              /* qty 0 */
        "T 1 2 1 POLL 000 5 TIMEOUT",              /* qty 5 */
        "T x 2 1 POLL 000 1 TIMEOUT",              /* non-numeric time */
        "T 1 2 256 POLL 000 1 TIMEOUT",            /* addr out of byte range */
        "T 1 2 1  POLL 000 1 TIMEOUT",             /* empty token */
        "T 1 2 1 POLL 000 1 TIMEOUT ",             /* trailing space */
        "H 1",                                     /* too few */
        "H 1 ONLINE X",                            /* too many */
        "H 1 SLEEPING",                            /* unknown state */
        "H x ONLINE",                              /* bad time */
    };
    tl_rec_t rec;
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        TEST_ASSERT_EQUAL_MESSAGE(-1, txn_log_parse(bad[i], &rec), bad[i]);
    }
}

TEST_CASE("txn_log: truncation returns 0 and stays NUL-terminated", "[txn_log]")
{
    ps_txn_t t = make_txn(PS_KIND_POLL, 0, 1, PS_RES_OK);
    const char *full = "T 120345 24 1 POLL 000 1 OK 0000";
    char buf[64];
    TEST_ASSERT_EQUAL_UINT32(strlen(full), txn_log_format_txn(buf, sizeof(buf), &t));
    /* exactly enough room (length + NUL) works, one byte less fails */
    TEST_ASSERT_EQUAL_UINT32(strlen(full), txn_log_format_txn(buf, strlen(full) + 1, &t));
    TEST_ASSERT_EQUAL_UINT32(0, txn_log_format_txn(buf, strlen(full), &t));
    TEST_ASSERT_EQUAL_CHAR('\0', buf[0]);
    TEST_ASSERT_EQUAL_UINT32(0, txn_log_format_txn(buf, 1, &t));
    TEST_ASSERT_EQUAL_CHAR('\0', buf[0]);
    TEST_ASSERT_EQUAL_UINT32(0, txn_log_format_txn(buf, 0, &t));

    TEST_ASSERT_EQUAL_UINT32(0, txn_log_format_health(buf, 5, 1, PS_ONLINE));
    TEST_ASSERT_EQUAL_CHAR('\0', buf[0]);
    TEST_ASSERT_EQUAL_UINT32(10, txn_log_format_health(buf, 11, 1, PS_ONLINE));
}

TEST_CASE("txn_log: out-of-range kind, result and health format to 0", "[txn_log]")
{
    char buf[64];
    memset(buf, 'x', sizeof(buf));
    ps_txn_t t = make_txn(PS_KIND_POLL, 0, 1, PS_RES_OK);
    t.req.kind = (ps_kind_t)99;
    TEST_ASSERT_EQUAL_UINT32(0, txn_log_format_txn(buf, sizeof(buf), &t));
    TEST_ASSERT_EQUAL_CHAR('\0', buf[0]);
    memset(buf, 'x', sizeof(buf));
    t = make_txn(PS_KIND_POLL, 0, 1, PS_RES_OK);
    t.res = (ps_res_t)99;
    TEST_ASSERT_EQUAL_UINT32(0, txn_log_format_txn(buf, sizeof(buf), &t));
    TEST_ASSERT_EQUAL_CHAR('\0', buf[0]);
    memset(buf, 'x', sizeof(buf));
    TEST_ASSERT_EQUAL_UINT32(0, txn_log_format_health(buf, sizeof(buf), 1, (ps_health_t)99));
    TEST_ASSERT_EQUAL_CHAR('\0', buf[0]);
}

/* ---- write-sequence kinds (spec 019) ---- */

TEST_CASE("txn_log: PRE, WRITE and VERIFY lines format and round-trip", "[txn_log]")
{
    static const struct { ps_kind_t kind; const char *name; } kinds[] = {
        { PS_KIND_PRE, "PRE" }, { PS_KIND_WRITE, "WRITE" }, { PS_KIND_VERIFY, "VERIFY" },
    };
    for (size_t i = 0; i < sizeof(kinds) / sizeof(kinds[0]); i++) {
        ps_txn_t t = make_txn(kinds[i].kind, 231, 1, PS_RES_OK);
        t.val[0] = 0x00d7;
        char expected[64], buf[160];
        snprintf(expected, sizeof(expected), "T 120345 24 1 %s 231 1 OK 00d7", kinds[i].name);
        assert_format(&t, expected);
        tl_rec_t rec;
        TEST_ASSERT_EQUAL_INT(1, txn_log_parse(expected, &rec));
        TEST_ASSERT_EQUAL_INT(kinds[i].kind, rec.txn.req.kind);
        TEST_ASSERT_EQUAL_HEX16(0x00d7, rec.txn.val[0]);
        TEST_ASSERT_EQUAL_UINT32(strlen(expected), txn_log_format_txn(buf, sizeof(buf), &rec.txn));
        TEST_ASSERT_EQUAL_STRING(expected, buf);
    }
    ps_txn_t w = make_txn(PS_KIND_WRITE, 201, 1, PS_RES_EXCEPTION);
    w.exc_code = 3;
    assert_format(&w, "T 120345 24 1 WRITE 201 1 EXC 03");
    w = make_txn(PS_KIND_WRITE, 233, 1, PS_RES_TIMEOUT);
    assert_format(&w, "T 120345 24 1 WRITE 233 1 TIMEOUT");
}

TEST_CASE("txn_log: W and A lines of the write guard are ignored", "[txn_log]")
{
    tl_rec_t rec;
    TEST_ASSERT_EQUAL_INT(0, txn_log_parse("W 1000 DONE 231 00d2 -> 00d7 budget 39", &rec));
    TEST_ASSERT_EQUAL_INT(0, txn_log_parse("A 1000 ARMED 120 s budget 40", &rec));
    TEST_ASSERT_EQUAL_INT(0, txn_log_parse("W 1000 REFUSED sp disarmed\r\n", &rec));
}

TEST_CASE("txn_log: result names", "[txn_log]")
{
    TEST_ASSERT_EQUAL_STRING("OK", txn_log_res_name(PS_RES_OK));
    TEST_ASSERT_EQUAL_STRING("TIMEOUT", txn_log_res_name(PS_RES_TIMEOUT));
    TEST_ASSERT_EQUAL_STRING("EXC", txn_log_res_name(PS_RES_EXCEPTION));
    TEST_ASSERT_EQUAL_STRING("BADFRAME", txn_log_res_name(PS_RES_BAD_FRAME));
    TEST_ASSERT_EQUAL_STRING("ERROR", txn_log_res_name(PS_RES_ERROR));
    TEST_ASSERT_EQUAL_STRING("?", txn_log_res_name((ps_res_t)99));
}
