/* test_sim_source.c - host Unity tests for sim_source (task 016). */

#include "unity.h"
#include "sim_source.h"

#define STALE_FAST_MS 15000

static void assert_fresh(const sim_t *s, uint16_t reg, int64_t now, bool expected)
{
    TEST_ASSERT_EQUAL_MESSAGE(expected, reg_model_fresh(sim_model(s), reg, now), "fresh");
}

static uint16_t raw_of(const sim_t *s, uint16_t reg)
{
    rm_entry_t e;
    TEST_ASSERT_TRUE(reg_model_get(sim_model(s), reg, &e));
    return e.raw;
}

TEST_CASE("sim: init seeds the demo registers and leaves the starred ones unset", "[sim]")
{
    sim_t s;
    sim_init(&s, 1000);
    const uint16_t seeded[][2] = {{0, 215}, {8, 200}, {201, 0}, {202, 160}, {203, 280}, {231, 200}, {233, 3}};
    for (size_t i = 0; i < sizeof(seeded) / sizeof(seeded[0]); i++) {
        assert_fresh(&s, seeded[i][0], 1000, true);
        TEST_ASSERT_EQUAL_UINT16(seeded[i][1], raw_of(&s, seeded[i][0]));
    }
    const uint16_t unset[] = {104, 245, 246, 247};
    for (size_t i = 0; i < sizeof(unset) / sizeof(unset[0]); i++) {
        assert_fresh(&s, unset[i], 1000, false);
    }
}

TEST_CASE("sim: tick refreshes values while LIVE", "[sim]")
{
    sim_t s;
    sim_init(&s, 0);
    sim_tick(&s, 10000);
    sim_tick(&s, 20000);
    assert_fresh(&s, 0, 20000 + STALE_FAST_MS, true);
    assert_fresh(&s, 0, 20000 + STALE_FAST_MS + 1, false);
}

TEST_CASE("sim: STALE stops the refresh and values age out", "[sim]")
{
    sim_t s;
    sim_init(&s, 0);
    sim_set_state(&s, SIM_STALE, 5000);
    sim_tick(&s, 10000);
    sim_tick(&s, 30000);
    assert_fresh(&s, 0, 5000, true);
    assert_fresh(&s, 0, 30000, false);
    TEST_ASSERT_EQUAL_UINT16(215, raw_of(&s, 0));  /* raw kept */
}

TEST_CASE("sim: OFFLINE sets timeouts and keeps raw and t_ok", "[sim]")
{
    sim_t s;
    sim_init(&s, 0);
    sim_set_state(&s, SIM_OFFLINE, 3000);
    rm_entry_t e;
    TEST_ASSERT_TRUE(reg_model_get(sim_model(&s), 231, &e));
    TEST_ASSERT_EQUAL(RM_TIMEOUT, e.avail);
    TEST_ASSERT_EQUAL_UINT16(200, e.raw);
    TEST_ASSERT_EQUAL_INT64(0, e.t_ok_ms);
    assert_fresh(&s, 231, 20000, false);
}

TEST_CASE("sim: LIVE recovery after STALE and OFFLINE", "[sim]")
{
    sim_t s;
    sim_init(&s, 0);
    sim_set_state(&s, SIM_OFFLINE, 1000);
    sim_set_state(&s, SIM_LIVE, 60000);
    assert_fresh(&s, 0, 60000, true);
    sim_set_state(&s, SIM_STALE, 61000);
    sim_set_state(&s, SIM_LIVE, 90000);
    assert_fresh(&s, 233, 90000, true);
}

TEST_CASE("sim: sim_set applies at once when LIVE", "[sim]")
{
    sim_t s;
    sim_init(&s, 0);
    TEST_ASSERT_EQUAL(0, sim_set(&s, 0, 230, 500));
    TEST_ASSERT_EQUAL_UINT16(230, raw_of(&s, 0));
    assert_fresh(&s, 0, 500 + STALE_FAST_MS, true);
}

TEST_CASE("sim: sim_set is deferred until LIVE when STALE or OFFLINE", "[sim]")
{
    sim_t s;
    sim_init(&s, 0);
    sim_set_state(&s, SIM_STALE, 100);
    TEST_ASSERT_EQUAL(0, sim_set(&s, 0, 230, 200));
    TEST_ASSERT_EQUAL_UINT16(215, raw_of(&s, 0));
    sim_set_state(&s, SIM_OFFLINE, 300);
    TEST_ASSERT_EQUAL(0, sim_set(&s, 0, 240, 400));
    TEST_ASSERT_EQUAL_UINT16(215, raw_of(&s, 0));
    sim_set_state(&s, SIM_LIVE, 500);
    TEST_ASSERT_EQUAL_UINT16(240, raw_of(&s, 0));
}

TEST_CASE("sim: sim_set can give a value to an unset register", "[sim]")
{
    sim_t s;
    sim_init(&s, 0);
    TEST_ASSERT_EQUAL(0, sim_set(&s, 247, 0x20, 100));
    assert_fresh(&s, 247, 100, true);
    sim_tick(&s, 5000);
    assert_fresh(&s, 247, 5000, true);
}

TEST_CASE("sim: undocumented register returns -1 and changes nothing", "[sim]")
{
    sim_t s;
    sim_init(&s, 0);
    TEST_ASSERT_EQUAL(-1, sim_set(&s, 2, 1, 0));
    TEST_ASSERT_EQUAL(-1, sim_unsupported(&s, 2, 0));
    TEST_ASSERT_EQUAL(-1, sim_set(&s, 0xFFFF, 1, 0));
    assert_fresh(&s, 0, 0, true);
}

TEST_CASE("sim: unsupported register stays unsupported across ticks until set again", "[sim]")
{
    sim_t s;
    sim_init(&s, 0);
    TEST_ASSERT_EQUAL(0, sim_unsupported(&s, 233, 100));
    sim_tick(&s, 200);
    rm_entry_t e;
    TEST_ASSERT_TRUE(reg_model_get(sim_model(&s), 233, &e));
    TEST_ASSERT_EQUAL(RM_UNSUPPORTED, e.avail);
    TEST_ASSERT_EQUAL_UINT8(2, e.exc_code);
    assert_fresh(&s, 233, 200, false);
    TEST_ASSERT_EQUAL(0, sim_set(&s, 233, 5, 300));
    assert_fresh(&s, 233, 300, true);
}

TEST_CASE("sim: out-of-range values are accepted and flagged by the model", "[sim]")
{
    sim_t s;
    sim_init(&s, 0);
    TEST_ASSERT_EQUAL(0, sim_set(&s, 0, 1001, 0));
    rm_entry_t e;
    TEST_ASSERT_TRUE(reg_model_get(sim_model(&s), 0, &e));
    TEST_ASSERT_TRUE(e.out_of_range);
}
