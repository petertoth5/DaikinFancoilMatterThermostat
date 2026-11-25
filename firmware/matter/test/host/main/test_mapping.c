/* test_mapping.c - host Unity tests for mapping_thermostat (task 016, M1-M5; the mapping is PROVISIONAL). */

#include "unity.h"
#include "mapping.h"
#include "daikin_regs.h"

#define T0 100000          /* values are written at T0, mapped at T0 unless stated */
#define STALE_FAST_MS 15000
#define STALE_SLOW_MS 180000

#define PRG_STBY 0x0080
#define STAT_COOL 0x0001
#define STAT_HEAT 0x0002
#define WEB_SP_RESTRICT 0x0020

typedef struct {
    reg_model_t model;
    map_state_t state;
} fixture_t;

/* Seeds like sim_source: 000=215 008=200 201=0 202=160 203=280 231=200 233=3 (all OK at T0). */
static void fixture_init(fixture_t *f)
{
    reg_model_init(&f->model, STALE_FAST_MS, STALE_SLOW_MS);
    mapping_init(&f->state);
    reg_model_set_ok(&f->model, 0, 215, T0);
    reg_model_set_ok(&f->model, 8, 200, T0);
    reg_model_set_ok(&f->model, 201, 0, T0);
    reg_model_set_ok(&f->model, 202, 160, T0);
    reg_model_set_ok(&f->model, 203, 280, T0);
    reg_model_set_ok(&f->model, 231, 200, T0);
    reg_model_set_ok(&f->model, 233, 3, T0);
}

static void set(fixture_t *f, uint16_t reg, uint16_t raw)
{
    TEST_ASSERT_EQUAL(0, reg_model_set_ok(&f->model, reg, raw, T0));
}

static map_thermostat_t map_at(fixture_t *f, int64_t now)
{
    map_thermostat_t out;
    mapping_thermostat(&f->state, &f->model, now, &out);
    return out;
}

static map_thermostat_t map_now(fixture_t *f)
{
    return map_at(f, T0);
}

/* A register value that is stale at now_ms: written long before. */
static void make_stale(fixture_t *f, uint16_t reg, uint16_t raw)
{
    TEST_ASSERT_EQUAL(0, reg_model_set_ok(&f->model, reg, raw, T0 - STALE_SLOW_MS - 1));
}

/* ------------------------------------------------------------------ initial values */

TEST_CASE("mapping: init values", "[mapping]")
{
    map_state_t st;
    mapping_init(&st);
    TEST_ASSERT_FALSE(st.kept.local_temp_valid);
    TEST_ASSERT_EQUAL_INT16(2000, st.kept.heat_sp);
    TEST_ASSERT_EQUAL_INT16(2000, st.kept.cool_sp);
    TEST_ASSERT_FALSE(st.kept.sp_observed);
    TEST_ASSERT_EQUAL_INT16(500, st.kept.min_heat);
    TEST_ASSERT_EQUAL_INT16(4000, st.kept.max_cool);
    TEST_ASSERT_EQUAL_UINT8(MAP_MODE_HEAT, st.kept.system_mode);
}

TEST_CASE("mapping: seeded model gives the on-device expectation O1", "[mapping]")
{
    fixture_t f;
    fixture_init(&f);
    map_thermostat_t t = map_now(&f);
    TEST_ASSERT_TRUE(t.local_temp_valid);
    TEST_ASSERT_EQUAL_INT16(2150, t.local_temp);
    TEST_ASSERT_EQUAL_INT16(2000, t.heat_sp);
    TEST_ASSERT_EQUAL_INT16(2000, t.cool_sp);
    TEST_ASSERT_TRUE(t.sp_observed);
    TEST_ASSERT_EQUAL_INT16(1600, t.min_heat);
    TEST_ASSERT_EQUAL_INT16(2800, t.max_heat);
    TEST_ASSERT_EQUAL_INT16(1600, t.min_cool);
    TEST_ASSERT_EQUAL_INT16(2800, t.max_cool);
    TEST_ASSERT_EQUAL_UINT8(MAP_MODE_HEAT, t.system_mode);
    TEST_ASSERT_FALSE(t.season_auto_flag);
    TEST_ASSERT_FALSE(t.limits_conflict);
}

/* ------------------------------------------------------------------ M1 LocalTemperature */

TEST_CASE("M1: LocalTemperature is 000 x 10 when fresh, boundaries 0 and 1000", "[mapping]")
{
    fixture_t f;
    fixture_init(&f);
    set(&f, 0, 230);
    TEST_ASSERT_EQUAL_INT16(2300, map_now(&f).local_temp);
    set(&f, 0, 0);
    TEST_ASSERT_TRUE(map_now(&f).local_temp_valid);
    TEST_ASSERT_EQUAL_INT16(0, map_now(&f).local_temp);
    set(&f, 0, 1000);
    TEST_ASSERT_EQUAL_INT16(10000, map_now(&f).local_temp);
}

TEST_CASE("M1: LocalTemperature null when unknown", "[mapping]")
{
    fixture_t f;
    fixture_init(&f);
    reg_model_reset(&f.model);
    TEST_ASSERT_FALSE(map_now(&f).local_temp_valid);
}

TEST_CASE("M1: LocalTemperature null when stale, 15000 ms fresh and 15001 ms not", "[mapping]")
{
    fixture_t f;
    fixture_init(&f);
    TEST_ASSERT_TRUE(map_at(&f, T0 + STALE_FAST_MS).local_temp_valid);
    TEST_ASSERT_FALSE(map_at(&f, T0 + STALE_FAST_MS + 1).local_temp_valid);
}

TEST_CASE("M1: LocalTemperature null when offline (timeout) once aged", "[mapping]")
{
    fixture_t f;
    fixture_init(&f);
    reg_model_set_timeout(&f.model, 0, T0 + 1);  /* raw and t_ok kept: still fresh by age */
    TEST_ASSERT_TRUE(map_now(&f).local_temp_valid);
    TEST_ASSERT_FALSE(map_at(&f, T0 + STALE_FAST_MS + 1).local_temp_valid);
}

TEST_CASE("M1: LocalTemperature null when unsupported", "[mapping]")
{
    fixture_t f;
    fixture_init(&f);
    reg_model_set_unsupported(&f.model, 0, 2, T0);
    TEST_ASSERT_FALSE(map_now(&f).local_temp_valid);
}

TEST_CASE("M1: LocalTemperature null when out of range (1001)", "[mapping]")
{
    fixture_t f;
    fixture_init(&f);
    set(&f, 0, 1001);
    TEST_ASSERT_FALSE(map_now(&f).local_temp_valid);
}

/* ------------------------------------------------------------------ M2 setpoints */

TEST_CASE("M2: both setpoints mirror 231 x 10", "[mapping]")
{
    fixture_t f;
    fixture_init(&f);
    set(&f, 231, 235);
    map_thermostat_t t = map_now(&f);
    TEST_ASSERT_EQUAL_INT16(2350, t.heat_sp);
    TEST_ASSERT_EQUAL_INT16(2350, t.cool_sp);
    TEST_ASSERT_TRUE(t.sp_observed);
}

TEST_CASE("M2: 008 is not used", "[mapping]")
{
    fixture_t f;
    fixture_init(&f);
    set(&f, 8, 250);
    TEST_ASSERT_EQUAL_INT16(2000, map_now(&f).heat_sp);
}

TEST_CASE("M2: unknown 231 keeps the default and sp_observed false", "[mapping]")
{
    fixture_t f;
    fixture_init(&f);
    reg_model_reset(&f.model);
    map_thermostat_t t = map_now(&f);
    TEST_ASSERT_EQUAL_INT16(2000, t.heat_sp);
    TEST_ASSERT_EQUAL_INT16(2000, t.cool_sp);
    TEST_ASSERT_FALSE(t.sp_observed);
}

/* Fixture with 231 = 230 already mapped, so "keep" means 2300. */
static void fixture_with_kept_setpoint(fixture_t *f)
{
    fixture_init(f);
    set(f, 231, 230);
    TEST_ASSERT_EQUAL_INT16(2300, map_now(f).heat_sp);
}

static void assert_setpoint_kept(fixture_t *f, int64_t now)
{
    map_thermostat_t t = map_at(f, now);
    TEST_ASSERT_EQUAL_INT16(2300, t.heat_sp);
    TEST_ASSERT_EQUAL_INT16(2300, t.cool_sp);
    TEST_ASSERT_TRUE(t.sp_observed);
}

TEST_CASE("M2: stale 231 keeps the last value", "[mapping]")
{
    fixture_t f;
    fixture_with_kept_setpoint(&f);
    assert_setpoint_kept(&f, T0 + STALE_FAST_MS + 1);
}

TEST_CASE("M2: offline 231 keeps the last value", "[mapping]")
{
    fixture_t f;
    fixture_with_kept_setpoint(&f);
    reg_model_set_timeout(&f.model, 231, T0 + 1);
    assert_setpoint_kept(&f, T0 + STALE_FAST_MS + 1);
}

TEST_CASE("M2: unsupported 231 keeps the last value", "[mapping]")
{
    fixture_t f;
    fixture_with_kept_setpoint(&f);
    reg_model_set_unsupported(&f.model, 231, 2, T0);
    assert_setpoint_kept(&f, T0);
}

TEST_CASE("M2: out-of-range 231 (49 and 401) keeps the last value", "[mapping]")
{
    fixture_t f;
    fixture_with_kept_setpoint(&f);
    set(&f, 231, 49);
    assert_setpoint_kept(&f, T0);
    set(&f, 231, 401);
    assert_setpoint_kept(&f, T0);
}

TEST_CASE("M2: 231 boundaries 50 and 400 are accepted", "[mapping]")
{
    fixture_t f;
    fixture_init(&f);
    set(&f, 231, 50);
    TEST_ASSERT_EQUAL_INT16(500, map_now(&f).heat_sp);
    set(&f, 231, 400);
    TEST_ASSERT_EQUAL_INT16(4000, map_now(&f).heat_sp);
}

/* ------------------------------------------------------------------ M3 limits */

TEST_CASE("M3: limits from 202/203, heat equals cool", "[mapping]")
{
    fixture_t f;
    fixture_init(&f);
    set(&f, 202, 180);
    set(&f, 203, 260);
    map_thermostat_t t = map_now(&f);
    TEST_ASSERT_EQUAL_INT16(1800, t.min_heat);
    TEST_ASSERT_EQUAL_INT16(2600, t.max_heat);
    TEST_ASSERT_EQUAL_INT16(1800, t.min_cool);
    TEST_ASSERT_EQUAL_INT16(2600, t.max_cool);
    TEST_ASSERT_FALSE(t.limits_conflict);
}

TEST_CASE("M3: 202 == 203 is a valid single-point range", "[mapping]")
{
    fixture_t f;
    fixture_init(&f);
    set(&f, 202, 200);
    set(&f, 203, 200);
    map_thermostat_t t = map_now(&f);
    TEST_ASSERT_EQUAL_INT16(2000, t.min_heat);
    TEST_ASSERT_EQUAL_INT16(2000, t.max_heat);
    TEST_ASSERT_FALSE(t.limits_conflict);
}

TEST_CASE("M3: 202 > 203 keeps the limits and sets limits_conflict", "[mapping]")
{
    fixture_t f;
    fixture_init(&f);
    map_now(&f);                       /* 1600/2800 become the kept value */
    set(&f, 202, 250);
    set(&f, 203, 200);
    map_thermostat_t t = map_now(&f);
    TEST_ASSERT_EQUAL_INT16(1600, t.min_heat);
    TEST_ASSERT_EQUAL_INT16(2800, t.max_heat);
    TEST_ASSERT_TRUE(t.limits_conflict);
}

TEST_CASE("M3: unknown limits keep the initial 500/4000 and set limits_conflict", "[mapping]")
{
    fixture_t f;
    fixture_init(&f);
    reg_model_reset(&f.model);
    map_thermostat_t t = map_now(&f);
    TEST_ASSERT_EQUAL_INT16(500, t.min_heat);
    TEST_ASSERT_EQUAL_INT16(4000, t.max_heat);
    TEST_ASSERT_EQUAL_INT16(500, t.min_cool);
    TEST_ASSERT_EQUAL_INT16(4000, t.max_cool);
    TEST_ASSERT_TRUE(t.limits_conflict);
}

TEST_CASE("M3: stale 202 or 203 keeps the last limits (slow threshold 180000 ms)", "[mapping]")
{
    fixture_t f;
    fixture_init(&f);
    map_now(&f);
    TEST_ASSERT_FALSE(map_at(&f, T0 + STALE_SLOW_MS).limits_conflict);
    map_thermostat_t t = map_at(&f, T0 + STALE_SLOW_MS + 1);
    TEST_ASSERT_TRUE(t.limits_conflict);
    TEST_ASSERT_EQUAL_INT16(1600, t.min_heat);
    TEST_ASSERT_EQUAL_INT16(2800, t.max_heat);
}

TEST_CASE("M3: unsupported and out-of-range 202/203 keep and flag", "[mapping]")
{
    fixture_t f;
    fixture_init(&f);
    map_now(&f);
    reg_model_set_unsupported(&f.model, 202, 2, T0);
    map_thermostat_t t = map_now(&f);
    TEST_ASSERT_TRUE(t.limits_conflict);
    TEST_ASSERT_EQUAL_INT16(1600, t.min_heat);
    fixture_init(&f);
    set(&f, 203, 351);                 /* above the fixed maximum 35.0 */
    TEST_ASSERT_TRUE(map_now(&f).limits_conflict);
    fixture_init(&f);
    set(&f, 202, 49);                  /* below the fixed minimum 5.0 */
    TEST_ASSERT_TRUE(map_now(&f).limits_conflict);
}

TEST_CASE("M3: 203 beyond the published absolute range cannot overflow and is rejected", "[mapping]")
{
    fixture_t f;
    fixture_init(&f);
    set(&f, 202, 65535);               /* 202 has no fixed maximum */
    TEST_ASSERT_TRUE(map_now(&f).limits_conflict);
    TEST_ASSERT_EQUAL_INT16(500, map_now(&f).min_heat);
}

TEST_CASE("M3: 247 bit 5 intersects with 245/246", "[mapping]")
{
    fixture_t f;
    fixture_init(&f);
    set(&f, 247, WEB_SP_RESTRICT);
    set(&f, 245, 180);
    set(&f, 246, 240);
    map_thermostat_t t = map_now(&f);
    TEST_ASSERT_EQUAL_INT16(1800, t.min_heat);
    TEST_ASSERT_EQUAL_INT16(2400, t.max_heat);
    TEST_ASSERT_EQUAL_INT16(1800, t.min_cool);
    TEST_ASSERT_EQUAL_INT16(2400, t.max_cool);
    TEST_ASSERT_FALSE(t.limits_conflict);
}

TEST_CASE("M3: 247 bit 5 with a wider WEB range leaves 202/203", "[mapping]")
{
    fixture_t f;
    fixture_init(&f);
    set(&f, 247, WEB_SP_RESTRICT);
    set(&f, 245, 50);
    set(&f, 246, 400);
    map_thermostat_t t = map_now(&f);
    TEST_ASSERT_EQUAL_INT16(1600, t.min_heat);
    TEST_ASSERT_EQUAL_INT16(2800, t.max_heat);
}

TEST_CASE("M3: 247 bit 5 with an empty intersection keeps and flags", "[mapping]")
{
    fixture_t f;
    fixture_init(&f);
    map_now(&f);
    set(&f, 247, WEB_SP_RESTRICT);
    set(&f, 245, 300);
    set(&f, 246, 350);                 /* [30.0, 35.0] vs [16.0, 28.0] */
    map_thermostat_t t = map_now(&f);
    TEST_ASSERT_TRUE(t.limits_conflict);
    TEST_ASSERT_EQUAL_INT16(1600, t.min_heat);
    TEST_ASSERT_EQUAL_INT16(2800, t.max_heat);
}

TEST_CASE("M3: 245/246 are ignored without 247 bit 5 or when 247 or 245/246 are not fresh", "[mapping]")
{
    fixture_t f;
    fixture_init(&f);
    set(&f, 245, 200);
    set(&f, 246, 220);
    set(&f, 247, 0x0001);              /* bit 5 clear */
    TEST_ASSERT_EQUAL_INT16(2800, map_now(&f).max_heat);
    set(&f, 247, WEB_SP_RESTRICT);
    reg_model_set_unsupported(&f.model, 245, 2, T0);
    TEST_ASSERT_EQUAL_INT16(2800, map_now(&f).max_heat);
    set(&f, 245, 200);
    TEST_ASSERT_EQUAL_INT16(2200, map_now(&f).max_heat);
    make_stale(&f, 247, WEB_SP_RESTRICT);
    TEST_ASSERT_EQUAL_INT16(2800, map_now(&f).max_heat);
}

/* ------------------------------------------------------------------ M4 SystemMode */

TEST_CASE("M4: 233 = 3 is Heat, 5 is Cool", "[mapping]")
{
    fixture_t f;
    fixture_init(&f);
    TEST_ASSERT_EQUAL_UINT8(MAP_MODE_HEAT, map_now(&f).system_mode);
    set(&f, 233, 5);
    map_thermostat_t t = map_now(&f);
    TEST_ASSERT_EQUAL_UINT8(MAP_MODE_COOL, t.system_mode);
    TEST_ASSERT_FALSE(t.season_auto_flag);
    set(&f, 233, 3);
    TEST_ASSERT_EQUAL_UINT8(MAP_MODE_HEAT, map_now(&f).system_mode);
}

TEST_CASE("M4: 201 bit 7 gives Off and wins over 233", "[mapping]")
{
    fixture_t f;
    fixture_init(&f);
    set(&f, 201, PRG_STBY);
    TEST_ASSERT_EQUAL_UINT8(MAP_MODE_OFF, map_now(&f).system_mode);
    set(&f, 233, 5);
    TEST_ASSERT_EQUAL_UINT8(MAP_MODE_OFF, map_now(&f).system_mode);
    set(&f, 201, 0);
    TEST_ASSERT_EQUAL_UINT8(MAP_MODE_COOL, map_now(&f).system_mode);
}

TEST_CASE("M4: Off applies even when 233 is not fresh", "[mapping]")
{
    fixture_t f;
    fixture_init(&f);
    set(&f, 201, PRG_STBY | 0x0003);   /* other PRG bits do not matter */
    reg_model_reset(&f.model);
    set(&f, 201, PRG_STBY);
    TEST_ASSERT_EQUAL_UINT8(MAP_MODE_OFF, map_now(&f).system_mode);
}

TEST_CASE("M4: 201 not fresh keeps the mode", "[mapping]")
{
    fixture_t f;
    fixture_init(&f);
    set(&f, 233, 5);
    TEST_ASSERT_EQUAL_UINT8(MAP_MODE_COOL, map_now(&f).system_mode);
    reg_model_set_unsupported(&f.model, 201, 2, T0);
    set(&f, 233, 3);
    TEST_ASSERT_EQUAL_UINT8(MAP_MODE_COOL, map_now(&f).system_mode);
}

TEST_CASE("M4: 233 unknown, stale, unsupported or out of range (4) keeps the mode; initial is Heat", "[mapping]")
{
    fixture_t f;
    fixture_init(&f);
    reg_model_reset(&f.model);
    set(&f, 201, 0);
    TEST_ASSERT_EQUAL_UINT8(MAP_MODE_HEAT, map_now(&f).system_mode);  /* unknown */
    set(&f, 233, 5);
    TEST_ASSERT_EQUAL_UINT8(MAP_MODE_COOL, map_now(&f).system_mode);
    set(&f, 233, 4);
    TEST_ASSERT_EQUAL_UINT8(MAP_MODE_COOL, map_now(&f).system_mode);  /* out of range */
    set(&f, 233, 3);
    reg_model_set_unsupported(&f.model, 233, 2, T0);
    TEST_ASSERT_EQUAL_UINT8(MAP_MODE_COOL, map_now(&f).system_mode);
    set(&f, 233, 3);
    TEST_ASSERT_EQUAL_UINT8(MAP_MODE_COOL, map_at(&f, T0 + STALE_FAST_MS + 1).system_mode);  /* stale */
}

TEST_CASE("M4: 233 = 0 takes the mode from STAT bit 0 / bit 1", "[mapping]")
{
    fixture_t f;
    fixture_init(&f);
    set(&f, 233, 0);
    set(&f, 104, STAT_COOL);
    map_thermostat_t t = map_now(&f);
    TEST_ASSERT_EQUAL_UINT8(MAP_MODE_COOL, t.system_mode);
    TEST_ASSERT_FALSE(t.season_auto_flag);
    set(&f, 104, STAT_HEAT);
    t = map_now(&f);
    TEST_ASSERT_EQUAL_UINT8(MAP_MODE_HEAT, t.system_mode);
    TEST_ASSERT_FALSE(t.season_auto_flag);
}

TEST_CASE("M4: 233 = 0 with STAT both or neither bits uses the last season and raises the flag", "[mapping]")
{
    fixture_t f;
    fixture_init(&f);
    set(&f, 233, 5);
    map_now(&f);                       /* last explicit season = Cool */
    set(&f, 233, 0);
    set(&f, 104, STAT_COOL | STAT_HEAT);
    map_thermostat_t t = map_now(&f);
    TEST_ASSERT_EQUAL_UINT8(MAP_MODE_COOL, t.system_mode);
    TEST_ASSERT_TRUE(t.season_auto_flag);
    set(&f, 104, 0x0100);              /* neither mode bit */
    t = map_now(&f);
    TEST_ASSERT_EQUAL_UINT8(MAP_MODE_COOL, t.system_mode);
    TEST_ASSERT_TRUE(t.season_auto_flag);
}

TEST_CASE("M4: 233 = 0 with STAT unavailable uses the last season (default Heat) and raises the flag", "[mapping]")
{
    fixture_t f;
    fixture_init(&f);
    set(&f, 233, 0);
    map_thermostat_t t = map_now(&f);  /* 104 never read */
    TEST_ASSERT_EQUAL_UINT8(MAP_MODE_HEAT, t.system_mode);
    TEST_ASSERT_TRUE(t.season_auto_flag);
    set(&f, 233, 5);
    map_now(&f);
    set(&f, 233, 0);
    reg_model_set_unsupported(&f.model, 104, 2, T0);
    t = map_now(&f);
    TEST_ASSERT_EQUAL_UINT8(MAP_MODE_COOL, t.system_mode);
    TEST_ASSERT_TRUE(t.season_auto_flag);
}

TEST_CASE("M4: STAT stale counts as unavailable", "[mapping]")
{
    fixture_t f;
    fixture_init(&f);
    set(&f, 233, 0);
    set(&f, 104, STAT_COOL);
    TEST_ASSERT_EQUAL_UINT8(MAP_MODE_COOL, map_now(&f).system_mode);
    reg_model_set_ok(&f.model, 104, STAT_COOL, T0 - STALE_FAST_MS - 1);  /* only 104 aged out */
    map_thermostat_t t = map_now(&f);
    TEST_ASSERT_EQUAL_UINT8(MAP_MODE_HEAT, t.system_mode);  /* last season, default Heat */
    TEST_ASSERT_TRUE(t.season_auto_flag);
}

TEST_CASE("M4: the flag clears again once 233 is explicit", "[mapping]")
{
    fixture_t f;
    fixture_init(&f);
    set(&f, 233, 0);
    TEST_ASSERT_TRUE(map_now(&f).season_auto_flag);
    set(&f, 233, 3);
    TEST_ASSERT_FALSE(map_now(&f).season_auto_flag);
}

/* ------------------------------------------------------------------ M5 setpoint outside the limits */

TEST_CASE("M5: setpoint outside the limits is published unclamped with limits_conflict", "[mapping]")
{
    fixture_t f;
    fixture_init(&f);
    set(&f, 231, 300);                 /* 30.0 > 28.0 */
    map_thermostat_t t = map_now(&f);
    TEST_ASSERT_EQUAL_INT16(3000, t.heat_sp);
    TEST_ASSERT_EQUAL_INT16(3000, t.cool_sp);
    TEST_ASSERT_TRUE(t.limits_conflict);
    set(&f, 231, 100);                 /* 10.0 < 16.0 */
    t = map_now(&f);
    TEST_ASSERT_EQUAL_INT16(1000, t.heat_sp);
    TEST_ASSERT_TRUE(t.limits_conflict);
    set(&f, 231, 160);                 /* exactly on the limit */
    TEST_ASSERT_FALSE(map_now(&f).limits_conflict);
    set(&f, 231, 280);
    TEST_ASSERT_FALSE(map_now(&f).limits_conflict);
}

TEST_CASE("M5: setpoint outside the WEB-narrowed limits is flagged", "[mapping]")
{
    fixture_t f;
    fixture_init(&f);
    set(&f, 247, WEB_SP_RESTRICT);
    set(&f, 245, 200);
    set(&f, 246, 240);
    set(&f, 231, 250);
    TEST_ASSERT_TRUE(map_now(&f).limits_conflict);
}

/* ------------------------------------------------------------------ availability per register */

typedef enum { AV_OFFLINE, AV_UNSUPPORTED, AV_UNKNOWN } avail_case_t;

/* What a register that is not usable does to the mapped output (mapping.h keep / null / conflict rules). */
typedef enum {
    OUT_LOCAL_TEMP_NULL,     /* M1 */
    OUT_SETPOINT_KEPT,       /* M2: 231 */
    OUT_LIMITS_KEPT,         /* M3: 202/203, limits_conflict raised */
    OUT_WEB_IGNORED,         /* M3: 245/246/247, plain 202/203 limits */
    OUT_MODE_KEPT,           /* M4: 201/233 */
    OUT_MODE_LAST_SEASON,    /* M4: 104, season_auto_flag raised */
} unusable_effect_t;

#define WEB_LO_RAW 180
#define WEB_HI_RAW 240

/* Every register of the mapping set (008 is deliberately not mapped, see "M2: 008 is not used") is seeded
 * so that it matters: 233 = 0 makes 104 decide the mode, 247 bit 5 makes 245/246 narrow the limits. */
static void seed_all_but(fixture_t *f, uint16_t skip_reg)
{
    static const struct { uint16_t reg, raw; } seeds[] = {
        {0, 215}, {104, STAT_COOL}, {201, 0}, {202, 160}, {203, 280}, {231, 230}, {233, 0},
        {245, WEB_LO_RAW}, {246, WEB_HI_RAW}, {247, WEB_SP_RESTRICT},
    };
    for (size_t i = 0; i < sizeof(seeds) / sizeof(seeds[0]); i++) {
        if (seeds[i].reg != skip_reg) {
            set(f, seeds[i].reg, seeds[i].raw);
        }
    }
}

/* Maps a fully usable model once (the kept values), then makes only `reg` unusable. */
static map_thermostat_t map_with_unusable(fixture_t *f, uint16_t reg, avail_case_t how)
{
    fixture_init(f);
    seed_all_but(f, 0xFFFF);
    map_now(f);
    switch (how) {
    case AV_OFFLINE:
        make_stale(f, reg, 1);                       /* old value, then a timeout: aged and not OK */
        reg_model_set_timeout(&f->model, reg, T0);
        break;
    case AV_UNSUPPORTED:
        reg_model_set_unsupported(&f->model, reg, 2, T0);
        break;
    case AV_UNKNOWN:
        reg_model_reset(&f->model);
        seed_all_but(f, reg);
        break;
    }
    return map_now(f);
}

static void assert_unusable_effect(unusable_effect_t effect, const map_thermostat_t *t)
{
    switch (effect) {
    case OUT_LOCAL_TEMP_NULL:
        TEST_ASSERT_FALSE(t->local_temp_valid);
        break;
    case OUT_SETPOINT_KEPT:
        TEST_ASSERT_EQUAL_INT16(2300, t->heat_sp);
        TEST_ASSERT_TRUE(t->sp_observed);
        TEST_ASSERT_FALSE(t->limits_conflict);
        break;
    case OUT_LIMITS_KEPT:
        TEST_ASSERT_EQUAL_INT16(WEB_LO_RAW * 10, t->min_heat);  /* kept: the WEB-narrowed limits */
        TEST_ASSERT_EQUAL_INT16(WEB_HI_RAW * 10, t->max_heat);
        TEST_ASSERT_TRUE(t->limits_conflict);
        break;
    case OUT_WEB_IGNORED:
        TEST_ASSERT_EQUAL_INT16(1600, t->min_heat);
        TEST_ASSERT_EQUAL_INT16(2800, t->max_heat);
        TEST_ASSERT_FALSE(t->limits_conflict);
        break;
    case OUT_MODE_KEPT:
        TEST_ASSERT_EQUAL_UINT8(MAP_MODE_COOL, t->system_mode);  /* kept from the STAT-derived Cool */
        TEST_ASSERT_FALSE(t->season_auto_flag);
        break;
    case OUT_MODE_LAST_SEASON:
        TEST_ASSERT_EQUAL_UINT8(MAP_MODE_HEAT, t->system_mode);  /* last explicit season = default Heat */
        TEST_ASSERT_TRUE(t->season_auto_flag);
        break;
    }
}

TEST_CASE("mapping: every mapped register offline, unsupported or never read has its documented effect", "[mapping]")
{
    static const struct { uint16_t reg; unusable_effect_t effect; } rows[] = {
        {0, OUT_LOCAL_TEMP_NULL}, {104, OUT_MODE_LAST_SEASON}, {201, OUT_MODE_KEPT}, {202, OUT_LIMITS_KEPT},
        {203, OUT_LIMITS_KEPT}, {231, OUT_SETPOINT_KEPT}, {233, OUT_MODE_KEPT}, {245, OUT_WEB_IGNORED},
        {246, OUT_WEB_IGNORED}, {247, OUT_WEB_IGNORED},
    };
    static const avail_case_t cases[] = {AV_OFFLINE, AV_UNSUPPORTED, AV_UNKNOWN};
    for (size_t r = 0; r < sizeof(rows) / sizeof(rows[0]); r++) {
        for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
            fixture_t f;
            map_thermostat_t t = map_with_unusable(&f, rows[r].reg, cases[c]);
            assert_unusable_effect(rows[r].effect, &t);
        }
    }
}

TEST_CASE("M3: out-of-range 245 (49) and 246 (401) are ignored like unusable ones", "[mapping]")
{
    static const struct { uint16_t reg, raw; } rows[] = {{245, 49}, {246, 401}};
    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
        fixture_t f;
        fixture_init(&f);
        seed_all_but(&f, 0xFFFF);
        set(&f, rows[i].reg, rows[i].raw);
        map_thermostat_t t = map_now(&f);
        assert_unusable_effect(OUT_WEB_IGNORED, &t);
    }
}

/* ------------------------------------------------------------------ determinism */

TEST_CASE("mapping: two runs on an unchanged model give identical output", "[mapping]")
{
    fixture_t f;
    fixture_init(&f);
    map_thermostat_t a = map_now(&f);
    map_thermostat_t b = map_now(&f);
    TEST_ASSERT_EQUAL_UINT32(0, mapping_changed(&a, &b, false));
}
