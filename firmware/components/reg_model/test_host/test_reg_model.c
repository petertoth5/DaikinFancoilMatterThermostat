/* test_reg_model.c - host Unity tests for reg_model (spec 010 test strategy). */

#include "unity.h"
#include "reg_model.h"
#include "daikin_regs.h"

#define STALE_FAST 15000
#define STALE_SLOW 180000

static reg_model_t make(void)
{
    reg_model_t m;
    reg_model_init(&m, STALE_FAST, STALE_SLOW);
    return m;
}

TEST_CASE("reg_model: table size matches RM_REG_COUNT", "[reg_model]")
{
    TEST_ASSERT_EQUAL_UINT32(RM_REG_COUNT, daikin_regs_count());
}

TEST_CASE("reg_model: init leaves every entry UNKNOWN", "[reg_model]")
{
    reg_model_t m = make();
    for (size_t i = 0; i < daikin_regs_count(); i++) {
        TEST_ASSERT_EQUAL_UINT16(daikin_regs_at(i)->reg, m.e[i].reg);
        TEST_ASSERT_EQUAL(RM_UNKNOWN, m.e[i].avail);
        TEST_ASSERT_FALSE(m.e[i].has_value);
        TEST_ASSERT_EQUAL_UINT32(0, m.e[i].ok_count);
        TEST_ASSERT_FALSE(reg_model_fresh(&m, m.e[i].reg, 0));
    }
}

TEST_CASE("reg_model: set_ok then get", "[reg_model]")
{
    reg_model_t m = make();
    rm_entry_t e;
    TEST_ASSERT_EQUAL(0, reg_model_set_ok(&m, 0, 215, 1000));
    TEST_ASSERT_TRUE(reg_model_get(&m, 0, &e));
    TEST_ASSERT_EQUAL(RM_OK, e.avail);
    TEST_ASSERT_TRUE(e.has_value);
    TEST_ASSERT_EQUAL_UINT16(215, e.raw);
    TEST_ASSERT_FALSE(e.out_of_range);
    TEST_ASSERT_EQUAL_INT64(1000, e.t_ok_ms);
    TEST_ASSERT_EQUAL_INT64(1000, e.t_attempt_ms);
    TEST_ASSERT_EQUAL_UINT32(1, e.ok_count);
    TEST_ASSERT_EQUAL_UINT32(0, e.fail_count);
}

TEST_CASE("reg_model: fresh at threshold, stale one ms later (FAST and SLOW)", "[reg_model]")
{
    reg_model_t m = make();
    reg_model_set_ok(&m, 0, 200, 1000);   /* FAST */
    reg_model_set_ok(&m, 1, 200, 1000);   /* SLOW */
    TEST_ASSERT_TRUE(reg_model_fresh(&m, 0, 1000 + STALE_FAST));
    TEST_ASSERT_FALSE(reg_model_fresh(&m, 0, 1000 + STALE_FAST + 1));
    TEST_ASSERT_TRUE(reg_model_fresh(&m, 1, 1000 + STALE_SLOW));
    TEST_ASSERT_FALSE(reg_model_fresh(&m, 1, 1000 + STALE_SLOW + 1));
}

TEST_CASE("reg_model: group NONE uses the SLOW threshold", "[reg_model]")
{
    reg_model_t m = make();
    reg_model_set_ok(&m, 15, 500, 0);   /* MOT_SET, group NONE */
    TEST_ASSERT_TRUE(reg_model_fresh(&m, 15, STALE_SLOW));
    TEST_ASSERT_FALSE(reg_model_fresh(&m, 15, STALE_SLOW + 1));
}

TEST_CASE("reg_model: UNSUPPORTED is not fresh but keeps raw", "[reg_model]")
{
    reg_model_t m = make();
    rm_entry_t e;
    reg_model_set_ok(&m, 104, 0x0005, 100);
    TEST_ASSERT_EQUAL(0, reg_model_set_unsupported(&m, 104, 2, 200));
    reg_model_get(&m, 104, &e);
    TEST_ASSERT_EQUAL(RM_UNSUPPORTED, e.avail);
    TEST_ASSERT_EQUAL_UINT8(2, e.exc_code);
    TEST_ASSERT_TRUE(e.has_value);
    TEST_ASSERT_EQUAL_UINT16(0x0005, e.raw);
    TEST_ASSERT_EQUAL_INT64(100, e.t_ok_ms);
    TEST_ASSERT_EQUAL_INT64(200, e.t_attempt_ms);
    TEST_ASSERT_EQUAL_UINT32(1, e.fail_count);
    TEST_ASSERT_FALSE(reg_model_fresh(&m, 104, 200));
}

TEST_CASE("reg_model: TIMEOUT keeps the value, freshness follows age", "[reg_model]")
{
    reg_model_t m = make();
    rm_entry_t e;
    reg_model_set_ok(&m, 0, 210, 1000);
    TEST_ASSERT_EQUAL(0, reg_model_set_timeout(&m, 0, 5000));
    reg_model_get(&m, 0, &e);
    TEST_ASSERT_EQUAL(RM_TIMEOUT, e.avail);
    TEST_ASSERT_EQUAL_UINT16(210, e.raw);
    TEST_ASSERT_EQUAL_INT64(1000, e.t_ok_ms);
    TEST_ASSERT_EQUAL_INT64(5000, e.t_attempt_ms);
    TEST_ASSERT_EQUAL_UINT32(1, e.fail_count);
    TEST_ASSERT_TRUE(reg_model_fresh(&m, 0, 1000 + STALE_FAST));
    TEST_ASSERT_FALSE(reg_model_fresh(&m, 0, 1000 + STALE_FAST + 1));
}

TEST_CASE("reg_model: TIMEOUT before any value is not fresh", "[reg_model]")
{
    reg_model_t m = make();
    reg_model_set_timeout(&m, 0, 10);
    TEST_ASSERT_FALSE(reg_model_fresh(&m, 0, 10));
}

TEST_CASE("reg_model: out_of_range follows the last OK value (231 raw 401)", "[reg_model]")
{
    reg_model_t m = make();
    rm_entry_t e;
    reg_model_set_ok(&m, 231, 401, 0);
    reg_model_get(&m, 231, &e);
    TEST_ASSERT_TRUE(e.out_of_range);
    reg_model_set_ok(&m, 231, 400, 1);
    reg_model_get(&m, 231, &e);
    TEST_ASSERT_FALSE(e.out_of_range);
    TEST_ASSERT_EQUAL_UINT32(2, e.ok_count);
}

TEST_CASE("reg_model: undocumented register", "[reg_model]")
{
    reg_model_t m = make();
    rm_entry_t e;
    TEST_ASSERT_EQUAL(-1, reg_model_set_ok(&m, 2, 1, 0));
    TEST_ASSERT_EQUAL(-1, reg_model_set_unsupported(&m, 2, 2, 0));
    TEST_ASSERT_EQUAL(-1, reg_model_set_timeout(&m, 2, 0));
    TEST_ASSERT_FALSE(reg_model_get(&m, 2, &e));
    TEST_ASSERT_FALSE(reg_model_fresh(&m, 2, 0));
}

TEST_CASE("reg_model: reset clears entries and keeps thresholds", "[reg_model]")
{
    reg_model_t m = make();
    rm_entry_t e;
    reg_model_set_ok(&m, 0, 200, 0);
    reg_model_set_timeout(&m, 1, 0);
    reg_model_reset(&m);
    reg_model_get(&m, 0, &e);
    TEST_ASSERT_EQUAL(RM_UNKNOWN, e.avail);
    TEST_ASSERT_FALSE(e.has_value);
    TEST_ASSERT_EQUAL_UINT32(0, e.ok_count);
    reg_model_get(&m, 1, &e);
    TEST_ASSERT_EQUAL_UINT32(0, e.fail_count);
    TEST_ASSERT_EQUAL_INT64(STALE_FAST, m.stale_fast_ms);
    TEST_ASSERT_EQUAL_INT64(STALE_SLOW, m.stale_slow_ms);
}

TEST_CASE("reg_model: set_exception is a failed read, value kept and ages out", "[reg_model]")
{
    reg_model_t m = make();
    rm_entry_t e;
    reg_model_set_ok(&m, 0, 215, 1000);
    TEST_ASSERT_EQUAL(0, reg_model_set_exception(&m, 0, 3, 2000));
    reg_model_get(&m, 0, &e);
    TEST_ASSERT_EQUAL(RM_TIMEOUT, e.avail);
    TEST_ASSERT_EQUAL_UINT8(3, e.exc_code);
    TEST_ASSERT_TRUE(e.has_value);
    TEST_ASSERT_EQUAL_UINT16(215, e.raw);
    TEST_ASSERT_EQUAL_INT64(1000, e.t_ok_ms);
    TEST_ASSERT_EQUAL_INT64(2000, e.t_attempt_ms);
    TEST_ASSERT_EQUAL_UINT32(1, e.fail_count);
    TEST_ASSERT_TRUE(reg_model_fresh(&m, 0, 1000 + STALE_FAST));
    TEST_ASSERT_FALSE(reg_model_fresh(&m, 0, 1000 + STALE_FAST + 1));
    TEST_ASSERT_EQUAL(-1, reg_model_set_exception(&m, 2, 3, 0));     /* undocumented */
}

TEST_CASE("reg_model: set_ok and set_timeout clear exc_code", "[reg_model]")
{
    reg_model_t m = make();
    rm_entry_t e;
    reg_model_set_exception(&m, 0, 3, 10);
    reg_model_set_timeout(&m, 0, 20);
    reg_model_get(&m, 0, &e);
    TEST_ASSERT_EQUAL_UINT8(0, e.exc_code);
    reg_model_set_exception(&m, 0, 4, 30);
    reg_model_set_ok(&m, 0, 1, 40);
    reg_model_get(&m, 0, &e);
    TEST_ASSERT_EQUAL_UINT8(0, e.exc_code);
}
