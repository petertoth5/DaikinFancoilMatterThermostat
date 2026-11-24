/* test_daikin_regs.c – host Unity tests for the daikin_regs component.
 *
 * The expected table below is typed independently from spec 009-daikin-regs.md
 * and wiki concepts/register-map.md.  It must NOT reuse values from the
 * implementation; that is the whole point of this transcription test.
 *
 * Test strategy (spec 009 "Test strategy"):
 *   - Transcription: count 32, ascending unique regs, every field per row,
 *     starred set, group sizes.
 *   - find: every documented reg found; undocumented regs -> NULL.
 *   - read_allowed: valid and invalid cases.
 *   - value: sign-extension, identity.
 *   - in_range: boundary inclusive, one-below/above, ENUM set, FLAGS.
 *   - write_mask: exact values for 201/231/233; 0 for others.
 */

#include "unity.h"
#include "daikin_regs.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* ----------------------------------------------------------------
 * Independent expected table (typed from spec 009, NOT from the impl).
 * Columns: reg, name, writable, starred, is_signed, kind,
 *          min_raw, max_raw, has_min, has_max,
 *          has_default, default_raw, group.
 * "dep" bounds have has_min or has_max = false; placeholder value 0.
 * ---------------------------------------------------------------- */
typedef struct {
    uint16_t   reg;
    const char *name;
    bool       writable;
    bool       starred;
    bool       is_signed;
    dr_kind_t  kind;
    int32_t    min_raw;
    int32_t    max_raw;
    bool       has_min;
    bool       has_max;
    bool       has_default;
    int32_t    default_raw;
    dr_group_t group;
} expected_row_t;

/* F = FAST, S = SLOW, N = NONE */
#define F DR_GROUP_FAST
#define S DR_GROUP_SLOW
#define N DR_GROUP_NONE

/* Layout: reg, name, W, *, sgn, kind, min_raw, max_raw, has_min, has_max, has_default, default_raw, grp
 * FLAGS: bounds not applicable; min=0, max=0, has_min=true, has_max=true (dummy).
 * dep:   bound is another register; has = false, placeholder value = 0. */
static const expected_row_t k_expected[] = {
    /*  reg   name        W      *      sgn    kind           min_raw max_raw hmin   hmax   hdef   dflt  grp */
    {   0, "T1",      false, false, false, DR_KIND_MLT01,     0,  1000, true,  true,  false, 0,    F },
    {   1, "T2",      false, true,  false, DR_KIND_MLT01,     0,  1000, true,  true,  false, 0,    S },
    {   8, "SP",      false, false, false, DR_KIND_MLT01,     0,  1000, true,  true,  false, 0,    F },
    {   9, "OUT",     false, true,  false, DR_KIND_FLAGS,     0,     0, true,  true,  false, 0,    S },
    {  15, "MOT_SET", false, true,  false, DR_KIND_MLT1,      0,  1700, true,  true,  false, 0,    N },
    { 104, "STAT",    false, true,  false, DR_KIND_FLAGS,     0,     0, true,  true,  false, 0,    F },
    { 105, "ALR_STAT",false, false, false, DR_KIND_FLAGS,     0,     0, true,  true,  false, 0,    S },
    { 200, "ADR",     true,  false, false, DR_KIND_MLT1,      1,   255, true,  true,  true,  1,    N },
    { 201, "PRG",     true,  false, false, DR_KIND_FLAGS,     0,     0, true,  true,  true,  0,    F },
    { 202, "SPL",     true,  false, false, DR_KIND_MLT01,    50,     0, true,  false, true,  160,  S },
    { 203, "SPH",     true,  false, false, DR_KIND_MLT01,     0,   350, false, true,  true,  280,  S },
    { 209, "E_SAVING",true,  false, false, DR_KIND_MLT01,     0,    85, true,  true,  true,  0,    N },
    { 210, "MVV5",    true,  true,  false, DR_KIND_MLT1,    400,     0, true,  false, true,  400,  N },
    { 211, "MVV4",    true,  true,  false, DR_KIND_MLT1,      0,     0, false, false, true,  550,  N },
    { 212, "MVV3",    true,  true,  false, DR_KIND_MLT1,      0,     0, false, false, true,  680,  N },
    { 213, "MVV2",    true,  true,  false, DR_KIND_MLT1,      0,  1500, false, true,  true,  1100, N },
    { 214, "MVV1",    true,  true,  false, DR_KIND_MLT1,      0,  1500, false, true,  true,  1500, N },
    { 215, "MVVP1",   true,  true,  false, DR_KIND_MLT1,      0,  1700, false, true,  true,  1700, N },
    { 218, "LLO",     true,  true,  false, DR_KIND_MLT01,     0,  1000, true,  true,  true,  300,  N },
    { 219, "LHI",     true,  true,  false, DR_KIND_MLT01,     0,  1000, true,  true,  true,  200,  N },
    { 221, "ACL",     true,  false, false, DR_KIND_MLT1,      0, 32000, true,  true,  true,  0,    N },
    { 222, "ACL_TIM", true,  false, false, DR_KIND_MLT1,      0,     0, true,  false, false, 0,    N },
    { 230, "MVVP3",   true,  false, false, DR_KIND_MLT1,      0,     0, false, false, true,  920,  N },
    { 231, "SP",      true,  false, false, DR_KIND_MLT01,    50,   400, true,  true,  true,  200,  F },
    { 233, "Man",     true,  false, false, DR_KIND_ENUM,      0,     5, true,  true,  true,  3,    F },
    { 234, "MVVP2",   true,  false, false, DR_KIND_MLT1,      0,  1500, false, true,  true,  1220, N },
    { 242, "OS1",     true,  false, true,  DR_KIND_MLT01,  -120,   120, true,  true,  true,  0,    N },
    { 243, "OS2",     true,  true,  true,  DR_KIND_MLT01,  -120,   120, true,  true,  true,  0,    N },
    { 244, "OS3",     true,  true,  true,  DR_KIND_MLT01,  -120,   120, true,  true,  true,  0,    N },
    { 245, "SPL_W",   true,  false, false, DR_KIND_MLT01,    50,     0, true,  false, true,  200,  S },
    { 246, "SPH_W",   true,  false, false, DR_KIND_MLT01,     0,   400, false, true,  true,  240,  S },
    { 247, "WEB",     true,  false, false, DR_KIND_FLAGS,     0,     0, true,  true,  true,  0,    S },
};

#undef F
#undef S
#undef N

#define EXPECTED_COUNT ((size_t)(sizeof(k_expected) / sizeof(k_expected[0])))

/* ================================================================
 * Transcription tests
 * ================================================================ */

TEST_CASE("count is 32", "[daikin_regs]")
{
    TEST_ASSERT_EQUAL_size_t(32, daikin_regs_count());
    TEST_ASSERT_EQUAL_size_t(32, EXPECTED_COUNT);
}

TEST_CASE("table ascending unique regs", "[daikin_regs]")
{
    for (size_t i = 1; i < daikin_regs_count(); i++) {
        const dr_reg_t *prev = daikin_regs_at(i - 1);
        const dr_reg_t *cur  = daikin_regs_at(i);
        TEST_ASSERT_NOT_NULL(prev);
        TEST_ASSERT_NOT_NULL(cur);
        TEST_ASSERT_TRUE(cur->reg > prev->reg);
    }
}

TEST_CASE("at returns NULL past end", "[daikin_regs]")
{
    TEST_ASSERT_NULL(daikin_regs_at(32));
    TEST_ASSERT_NULL(daikin_regs_at(100));
}

TEST_CASE("every field matches expected table", "[daikin_regs]")
{
    TEST_ASSERT_EQUAL_size_t(EXPECTED_COUNT, daikin_regs_count());

    for (size_t i = 0; i < EXPECTED_COUNT; i++) {
        const expected_row_t *exp = &k_expected[i];
        const dr_reg_t       *got = daikin_regs_at(i);
        TEST_ASSERT_NOT_NULL(got);

        TEST_ASSERT_EQUAL_UINT16_MESSAGE(exp->reg,         got->reg,         exp->name);
        TEST_ASSERT_EQUAL_STRING_MESSAGE(exp->name,        got->name,        exp->name);
        TEST_ASSERT_EQUAL_INT_MESSAGE(  (int)exp->writable,(int)got->writable, exp->name);
        TEST_ASSERT_EQUAL_INT_MESSAGE(  (int)exp->starred, (int)got->starred,  exp->name);
        TEST_ASSERT_EQUAL_INT_MESSAGE(  (int)exp->is_signed,(int)got->is_signed,exp->name);
        TEST_ASSERT_EQUAL_INT_MESSAGE(  (int)exp->kind,    (int)got->kind,     exp->name);
        TEST_ASSERT_EQUAL_INT_MESSAGE(  (int)exp->has_min, (int)got->has_min,  exp->name);
        TEST_ASSERT_EQUAL_INT_MESSAGE(  (int)exp->has_max, (int)got->has_max,  exp->name);
        if (exp->has_min) {
            TEST_ASSERT_EQUAL_INT32_MESSAGE(exp->min_raw, got->min_raw, exp->name);
        }
        if (exp->has_max) {
            TEST_ASSERT_EQUAL_INT32_MESSAGE(exp->max_raw, got->max_raw, exp->name);
        }
        TEST_ASSERT_EQUAL_INT_MESSAGE(  (int)exp->has_default,(int)got->has_default, exp->name);
        if (exp->has_default) {
            TEST_ASSERT_EQUAL_INT32_MESSAGE(exp->default_raw, got->default_raw, exp->name);
        }
        TEST_ASSERT_EQUAL_INT_MESSAGE(  (int)exp->group,   (int)got->group,    exp->name);
    }
}

TEST_CASE("starred set equals expected", "[daikin_regs]")
{
    /* Starred set: {1, 9, 15, 104, 210, 211, 212, 213, 214, 215, 218, 219, 243, 244}
     * (spec 009 "Test strategy"; wiki register-map.md p4). */
    static const uint16_t starred[] = {
        1, 9, 15, 104, 210, 211, 212, 213, 214, 215, 218, 219, 243, 244
    };
    const size_t n_starred = sizeof(starred) / sizeof(starred[0]);

    /* Every reg in the starred array must be starred in the table. */
    for (size_t i = 0; i < n_starred; i++) {
        const dr_reg_t *r = daikin_regs_find(starred[i]);
        TEST_ASSERT_NOT_NULL(r);
        TEST_ASSERT_TRUE_MESSAGE(r->starred, r->name);
    }

    /* Every starred reg in the table must be in the starred array. */
    for (size_t i = 0; i < daikin_regs_count(); i++) {
        const dr_reg_t *r = daikin_regs_at(i);
        if (!r->starred) {
            continue;
        }
        bool found = false;
        for (size_t j = 0; j < n_starred; j++) {
            if (r->reg == starred[j]) {
                found = true;
                break;
            }
        }
        TEST_ASSERT_TRUE_MESSAGE(found, r->name);
    }
}

TEST_CASE("group sizes: 6 fast, 8 slow, 18 none", "[daikin_regs]")
{
    size_t nf = 0, ns = 0, nn = 0;
    for (size_t i = 0; i < daikin_regs_count(); i++) {
        const dr_reg_t *r = daikin_regs_at(i);
        switch (r->group) {
        case DR_GROUP_FAST: nf++; break;
        case DR_GROUP_SLOW: ns++; break;
        case DR_GROUP_NONE: nn++; break;
        }
    }
    TEST_ASSERT_EQUAL_size_t(6,  nf);
    TEST_ASSERT_EQUAL_size_t(8,  ns);
    TEST_ASSERT_EQUAL_size_t(18, nn);
}

/* ================================================================
 * find tests
 * ================================================================ */

TEST_CASE("find: every documented reg found", "[daikin_regs]")
{
    for (size_t i = 0; i < EXPECTED_COUNT; i++) {
        const dr_reg_t *r = daikin_regs_find(k_expected[i].reg);
        TEST_ASSERT_NOT_NULL_MESSAGE(r, k_expected[i].name);
        TEST_ASSERT_EQUAL_UINT16(k_expected[i].reg, r->reg);
    }
}

TEST_CASE("find: undocumented regs return NULL", "[daikin_regs]")
{
    /* Regs explicitly listed as undocumented in spec 009 "Test strategy". */
    static const uint16_t undoc[] = {
        2, 16, 199, 216, 217, 220, 223, 232, 235, 248, 0xFFFF
    };
    for (size_t i = 0; i < sizeof(undoc) / sizeof(undoc[0]); i++) {
        TEST_ASSERT_NULL(daikin_regs_find(undoc[i]));
    }
}

/* ================================================================
 * read_allowed tests
 * ================================================================ */

TEST_CASE("read_allowed: valid ranges", "[daikin_regs]")
{
    TEST_ASSERT_TRUE(daikin_regs_read_allowed(0,   1)); /* T1 alone */
    TEST_ASSERT_TRUE(daikin_regs_read_allowed(0,   2)); /* T1 + T2 -> wait, 1 is starred but documented; both in table */
    TEST_ASSERT_TRUE(daikin_regs_read_allowed(200, 4)); /* ADR PRG SPL SPH */
    TEST_ASSERT_TRUE(daikin_regs_read_allowed(242, 4)); /* OS1 OS2 OS3... wait: 242,243,244 + 245 = all in table */
    TEST_ASSERT_TRUE(daikin_regs_read_allowed(244, 4)); /* 244,245,246,247 all documented */
}

TEST_CASE("read_allowed: invalid ranges", "[daikin_regs]")
{
    TEST_ASSERT_FALSE(daikin_regs_read_allowed(1,   2));  /* 1 documented, 2 not */
    TEST_ASSERT_FALSE(daikin_regs_read_allowed(247, 2));  /* 247 documented, 248 not */
    TEST_ASSERT_FALSE(daikin_regs_read_allowed(0,   0));  /* qty=0 invalid */
    TEST_ASSERT_FALSE(daikin_regs_read_allowed(0,   5));  /* qty=5 > 4 */
    TEST_ASSERT_FALSE(daikin_regs_read_allowed(216, 1));  /* undocumented */
    TEST_ASSERT_FALSE(daikin_regs_read_allowed(0xFFFF, 1)); /* 0xFFFF undocumented, not a wrap */
}

TEST_CASE("read_allowed: start+qty overflow", "[daikin_regs]")
{
    /* 0xFFFE + 3 would overflow uint16; must return false */
    TEST_ASSERT_FALSE(daikin_regs_read_allowed(0xFFFE, 3));
    TEST_ASSERT_FALSE(daikin_regs_read_allowed(0xFFFF, 2));
}

/* ================================================================
 * value tests
 * ================================================================ */

TEST_CASE("value: 242 raw 0xFF88 -> -120 (sign-extension)", "[daikin_regs]")
{
    /* 0xFF88 = 65416 unsigned; as int16_t = -120; MLT01 stays in tenths. */
    const dr_reg_t *r = daikin_regs_find(242);
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_TRUE(r->is_signed);
    TEST_ASSERT_EQUAL_INT32(-120, daikin_regs_value(r, 0xFF88u));
}

TEST_CASE("value: unsigned raw 215 -> 215 (reg 0 T1)", "[daikin_regs]")
{
    /* reg 0 T1, MLT01, unsigned: raw 215 stays 215 (tenths = 21.5 °C). */
    const dr_reg_t *r = daikin_regs_find(0);
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_FALSE(r->is_signed);
    TEST_ASSERT_EQUAL_INT32(215, daikin_regs_value(r, 215u));
}

TEST_CASE("value: MLT1 raw 1700 -> 1700 (reg 15 MOT_SET)", "[daikin_regs]")
{
    const dr_reg_t *r = daikin_regs_find(15);
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQUAL_INT32(1700, daikin_regs_value(r, 1700u));
}

/* ================================================================
 * in_range tests
 * ================================================================ */

TEST_CASE("in_range: MLT01 boundaries inclusive", "[daikin_regs]")
{
    const dr_reg_t *t1 = daikin_regs_find(0);  /* MLT01 0..1000 */
    TEST_ASSERT_NOT_NULL(t1);
    TEST_ASSERT_TRUE( daikin_regs_in_range(t1,    0));   /* min */
    TEST_ASSERT_TRUE( daikin_regs_in_range(t1, 1000));   /* max */
    TEST_ASSERT_FALSE(daikin_regs_in_range(t1, 1001));   /* one above */
    /* below min: 0 is min, uint16 wraps to 0xFFFF if decremented -> range check
     * relies on signed comparison via daikin_regs_value; 0xFFFF unsigned = 65535,
     * which is > 1000 for an unsigned register. */
    TEST_ASSERT_FALSE(daikin_regs_in_range(t1, 0xFFFFu));
}

TEST_CASE("in_range: signed OS1 boundaries inclusive", "[daikin_regs]")
{
    const dr_reg_t *os1 = daikin_regs_find(242);  /* MLT01 signed -120..120 */
    TEST_ASSERT_NOT_NULL(os1);
    TEST_ASSERT_TRUE( daikin_regs_in_range(os1, (uint16_t)(int16_t)-120)); /* 0xFF88 */
    TEST_ASSERT_TRUE( daikin_regs_in_range(os1, 120));                     /* +120 */
    TEST_ASSERT_FALSE(daikin_regs_in_range(os1, 121));                     /* one above */
    TEST_ASSERT_FALSE(daikin_regs_in_range(os1, (uint16_t)(int16_t)-121)); /* one below */
}

TEST_CASE("in_range: ENUM 233 set {0,3,5}", "[daikin_regs]")
{
    const dr_reg_t *man = daikin_regs_find(233);
    TEST_ASSERT_NOT_NULL(man);
    TEST_ASSERT_TRUE( daikin_regs_in_range(man, 0));
    TEST_ASSERT_TRUE( daikin_regs_in_range(man, 3));
    TEST_ASSERT_TRUE( daikin_regs_in_range(man, 5));
    TEST_ASSERT_FALSE(daikin_regs_in_range(man, 1));
    TEST_ASSERT_FALSE(daikin_regs_in_range(man, 2));
    TEST_ASSERT_FALSE(daikin_regs_in_range(man, 4));
    TEST_ASSERT_FALSE(daikin_regs_in_range(man, 6));
}

TEST_CASE("in_range: FLAGS always true", "[daikin_regs]")
{
    const dr_reg_t *prg = daikin_regs_find(201);
    TEST_ASSERT_NOT_NULL(prg);
    TEST_ASSERT_EQUAL_INT(DR_KIND_FLAGS, (int)prg->kind);
    TEST_ASSERT_TRUE(daikin_regs_in_range(prg, 0));
    TEST_ASSERT_TRUE(daikin_regs_in_range(prg, 0xFFFFu));
    TEST_ASSERT_TRUE(daikin_regs_in_range(prg, 0x0087u));
}

TEST_CASE("in_range: dep bounds ignored", "[daikin_regs]")
{
    /* 202 SPL: has_min=true(50), has_max=false(dep). Any value passes max check. */
    const dr_reg_t *spl = daikin_regs_find(202);
    TEST_ASSERT_NOT_NULL(spl);
    TEST_ASSERT_TRUE(spl->has_min);
    TEST_ASSERT_FALSE(spl->has_max);
    TEST_ASSERT_TRUE( daikin_regs_in_range(spl, 50));    /* at fixed min */
    TEST_ASSERT_FALSE(daikin_regs_in_range(spl, 49));    /* below fixed min */
    TEST_ASSERT_TRUE( daikin_regs_in_range(spl, 65535)); /* dep max: not checked */
}

/* ================================================================
 * write_mask tests
 * ================================================================ */

TEST_CASE("write_mask: 201 -> 0x0087", "[daikin_regs]")
{
    TEST_ASSERT_EQUAL_HEX16(0x0087u, daikin_regs_write_mask(201));
}

TEST_CASE("write_mask: 231 -> 0xFFFF", "[daikin_regs]")
{
    TEST_ASSERT_EQUAL_HEX16(0xFFFFu, daikin_regs_write_mask(231));
}

TEST_CASE("write_mask: 233 -> 0xFFFF", "[daikin_regs]")
{
    TEST_ASSERT_EQUAL_HEX16(0xFFFFu, daikin_regs_write_mask(233));
}

TEST_CASE("write_mask: 247 -> 0 (read-only D18)", "[daikin_regs]")
{
    TEST_ASSERT_EQUAL_HEX16(0x0000u, daikin_regs_write_mask(247));
}

TEST_CASE("write_mask: 200 -> 0 (ADR never written)", "[daikin_regs]")
{
    TEST_ASSERT_EQUAL_HEX16(0x0000u, daikin_regs_write_mask(200));
}

TEST_CASE("write_mask: all other documented regs -> 0", "[daikin_regs]")
{
    /* Every documented reg except the three whitelisted ones must return 0. */
    for (size_t i = 0; i < daikin_regs_count(); i++) {
        const dr_reg_t *r = daikin_regs_at(i);
        uint16_t reg = r->reg;
        if (reg == 201 || reg == 231 || reg == 233) {
            continue;
        }
        TEST_ASSERT_EQUAL_HEX16_MESSAGE(0x0000u, daikin_regs_write_mask(reg), r->name);
    }
}
