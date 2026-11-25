/* test_mapping_changed.c - host Unity tests for mapping_changed (task 016). */

#include "unity.h"
#include "mapping.h"

static map_thermostat_t base(void)
{
    map_state_t st;
    mapping_init(&st);
    map_thermostat_t t = st.kept;
    t.local_temp_valid = true;
    t.local_temp = 2150;
    return t;
}

TEST_CASE("mapping_changed: identical outputs give 0", "[mapping_changed]")
{
    map_thermostat_t a = base();
    map_thermostat_t b = base();
    TEST_ASSERT_EQUAL_UINT32(0, mapping_changed(&a, &b, false));
}

TEST_CASE("mapping_changed: first returns all five bits even when identical", "[mapping_changed]")
{
    map_thermostat_t a = base();
    map_thermostat_t b = base();
    uint32_t bits = mapping_changed(&a, &b, true);
    TEST_ASSERT_EQUAL_UINT32(MAP_CH_ALL, bits);
    TEST_ASSERT_EQUAL_UINT32(0x1F, bits);
}

TEST_CASE("mapping_changed: each limit field alone sets only the limits bit", "[mapping_changed]")
{
    map_thermostat_t a = base();
    map_thermostat_t b = base();
    b.min_heat++;
    TEST_ASSERT_EQUAL_UINT32(MAP_CH_LIMITS, mapping_changed(&a, &b, false));
    b = base();
    b.max_heat++;
    TEST_ASSERT_EQUAL_UINT32(MAP_CH_LIMITS, mapping_changed(&a, &b, false));
    b = base();
    b.min_cool++;
    TEST_ASSERT_EQUAL_UINT32(MAP_CH_LIMITS, mapping_changed(&a, &b, false));
    b = base();
    b.max_cool++;
    TEST_ASSERT_EQUAL_UINT32(MAP_CH_LIMITS, mapping_changed(&a, &b, false));
}

TEST_CASE("mapping_changed: heat and cool setpoints have their own bits", "[mapping_changed]")
{
    map_thermostat_t a = base();
    map_thermostat_t b = base();
    b.heat_sp++;
    TEST_ASSERT_EQUAL_UINT32(MAP_CH_HEAT_SP, mapping_changed(&a, &b, false));
    b = base();
    b.cool_sp++;
    TEST_ASSERT_EQUAL_UINT32(MAP_CH_COOL_SP, mapping_changed(&a, &b, false));
}

TEST_CASE("mapping_changed: SystemMode alone sets only its bit", "[mapping_changed]")
{
    map_thermostat_t a = base();
    map_thermostat_t b = base();
    b.system_mode = MAP_MODE_OFF;
    TEST_ASSERT_EQUAL_UINT32(MAP_CH_SYSTEM_MODE, mapping_changed(&a, &b, false));
}

TEST_CASE("mapping_changed: LocalTemperature value and valid <-> null set only its bit", "[mapping_changed]")
{
    map_thermostat_t a = base();
    map_thermostat_t b = base();
    b.local_temp = 2160;
    TEST_ASSERT_EQUAL_UINT32(MAP_CH_LOCAL_TEMP, mapping_changed(&a, &b, false));
    b = base();
    b.local_temp_valid = false;
    TEST_ASSERT_EQUAL_UINT32(MAP_CH_LOCAL_TEMP, mapping_changed(&a, &b, false));
    TEST_ASSERT_EQUAL_UINT32(MAP_CH_LOCAL_TEMP, mapping_changed(&b, &a, false));
}

TEST_CASE("mapping_changed: two nulls are equal whatever the stored number", "[mapping_changed]")
{
    map_thermostat_t a = base();
    map_thermostat_t b = base();
    a.local_temp_valid = b.local_temp_valid = false;
    b.local_temp = 1;
    TEST_ASSERT_EQUAL_UINT32(0, mapping_changed(&a, &b, false));
}

TEST_CASE("mapping_changed: flags alone never set a bit", "[mapping_changed]")
{
    map_thermostat_t a = base();
    map_thermostat_t b = base();
    b.sp_observed = !a.sp_observed;
    TEST_ASSERT_EQUAL_UINT32(0, mapping_changed(&a, &b, false));
    b = base();
    b.season_auto_flag = !a.season_auto_flag;
    TEST_ASSERT_EQUAL_UINT32(0, mapping_changed(&a, &b, false));
    b = base();
    b.limits_conflict = !a.limits_conflict;
    TEST_ASSERT_EQUAL_UINT32(0, mapping_changed(&a, &b, false));
}

TEST_CASE("mapping_changed: several changes give several bits", "[mapping_changed]")
{
    map_thermostat_t a = base();
    map_thermostat_t b = base();
    b.heat_sp++;
    b.system_mode = MAP_MODE_COOL;
    TEST_ASSERT_EQUAL_UINT32(MAP_CH_HEAT_SP | MAP_CH_SYSTEM_MODE, mapping_changed(&a, &b, false));
}
