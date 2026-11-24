/* daikin_regs.c – 32-row register table for the N420384A p4 register map.
 *
 * Column abbreviations used in ROW() below:
 *   reg, name, writable, starred, is_signed, kind,
 *   min_raw, max_raw, has_min, has_max,
 *   has_default, default_raw,
 *   group, page
 *
 * Transcription notes (spec 009, "Transcription notes"):
 *   - 231 bounds 50..400 are the printed absolute "5-40 °C"; the printed
 *     dependent limits SPL-SPH / SPL_W-SPH_W ("SHP" typo, inferred) are
 *     applied by later phases, not here.
 *   - 233 Min/Max columns are the enum span; validity is the set {0, 3, 5}.
 *   - Signedness of 242-244 is inferred from "±12.0 K"; all others unsigned.
 *   - 210 printed min "400-(MVV3-4)": min 400 fixed, max dep. 230/234 printed
 *     unstarred, kept as printed.
 *   - Poll groups are architecture decisions (v0.3 D6, section 3), not manual
 *     facts: fast 000, 008, 201, 231, 233 + 104; slow 105, 202, 203, 245, 246,
 *     247 + 009, 001.
 */

#include "daikin_regs.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Bounds shorthands: produce min_raw, max_raw, has_min, has_max in struct order.
 * dep = bound is another register (has = false); fix = printed absolute bound.
 * Struct layout: min_raw(int32), max_raw(int32), has_min(bool), has_max(bool). */
#define BOUNDS_FF(mn, mx)  (mn), (mx), true,  true   /* both fixed */
#define BOUNDS_FD(mn)      (mn), 0,    true,  false  /* fixed min, dep max */
#define BOUNDS_DF(mx)      0,    (mx), false, true   /* dep min, fixed max */
#define BOUNDS_DD          0,    0,    false, false  /* both dep */
#define BOUNDS_NONE        0,    0,    true,  true   /* FLAGS: values unused */
#define NO_DEFAULT  false, 0
#define HAS_DEFAULT(v) true, (v)

/* ENUM 233 allowed set: {0, 3, 5} (spec 009, interface section). */
static const uint8_t k_man_allowed[] = {0, 3, 5};

/* Table in ascending register order (spec: "table order = ascending reg").
 * Columns: reg, name, W, *, sgn, kind, [min_raw,max_raw,has_min,has_max], def, grp, page */
static const dr_reg_t k_regs[] = {
    { 0,   "T1",      false, false, false, DR_KIND_MLT01, BOUNDS_FF(0, 1000),  NO_DEFAULT,       DR_GROUP_FAST, "p4" }, /* p4 */
    { 1,   "T2",      false, true,  false, DR_KIND_MLT01, BOUNDS_FF(0, 1000),  NO_DEFAULT,       DR_GROUP_SLOW, "p4" }, /* p4 */
    { 8,   "SP",      false, false, false, DR_KIND_MLT01, BOUNDS_FF(0, 1000),  NO_DEFAULT,       DR_GROUP_FAST, "p4" }, /* p4 */
    { 9,   "OUT",     false, true,  false, DR_KIND_FLAGS, BOUNDS_NONE,         NO_DEFAULT,       DR_GROUP_SLOW, "p4" }, /* p4 */
    { 15,  "MOT_SET", false, true,  false, DR_KIND_MLT1,  BOUNDS_FF(0, 1700),  NO_DEFAULT,       DR_GROUP_NONE, "p4" }, /* p4 */
    { 104, "STAT",    false, true,  false, DR_KIND_FLAGS, BOUNDS_NONE,         NO_DEFAULT,       DR_GROUP_FAST, "p4" }, /* p4 */
    { 105, "ALR_STAT",false, false, false, DR_KIND_FLAGS, BOUNDS_NONE,         NO_DEFAULT,       DR_GROUP_SLOW, "p4" }, /* p4 */
    { 200, "ADR",     true,  false, false, DR_KIND_MLT1,  BOUNDS_FF(1, 255),   HAS_DEFAULT(1),   DR_GROUP_NONE, "p4" }, /* p4 */
    { 201, "PRG",     true,  false, false, DR_KIND_FLAGS, BOUNDS_NONE,         HAS_DEFAULT(0),   DR_GROUP_FAST, "p4" }, /* p4 p5 */
    { 202, "SPL",     true,  false, false, DR_KIND_MLT01, BOUNDS_FD(50),       HAS_DEFAULT(160), DR_GROUP_SLOW, "p4" }, /* p4 */
    { 203, "SPH",     true,  false, false, DR_KIND_MLT01, BOUNDS_DF(350),      HAS_DEFAULT(280), DR_GROUP_SLOW, "p4" }, /* p4 */
    { 209, "E_SAVING",true,  false, false, DR_KIND_MLT01, BOUNDS_FF(0, 85),    HAS_DEFAULT(0),   DR_GROUP_NONE, "p4" }, /* p4 */
    /* 210 printed min "400-(MVV3-4)": min 400 fixed, max dep (p4). */
    { 210, "MVV5",    true,  true,  false, DR_KIND_MLT1,  BOUNDS_FD(400),      HAS_DEFAULT(400), DR_GROUP_NONE, "p4" }, /* p4 */
    { 211, "MVV4",    true,  true,  false, DR_KIND_MLT1,  BOUNDS_DD,           HAS_DEFAULT(550), DR_GROUP_NONE, "p4" }, /* p4 */
    { 212, "MVV3",    true,  true,  false, DR_KIND_MLT1,  BOUNDS_DD,           HAS_DEFAULT(680), DR_GROUP_NONE, "p4" }, /* p4 */
    { 213, "MVV2",    true,  true,  false, DR_KIND_MLT1,  BOUNDS_DF(1500),     HAS_DEFAULT(1100),DR_GROUP_NONE, "p4" }, /* p4 */
    { 214, "MVV1",    true,  true,  false, DR_KIND_MLT1,  BOUNDS_DF(1500),     HAS_DEFAULT(1500),DR_GROUP_NONE, "p4" }, /* p4 */
    { 215, "MVVP1",   true,  true,  false, DR_KIND_MLT1,  BOUNDS_DF(1700),     HAS_DEFAULT(1700),DR_GROUP_NONE, "p4" }, /* p4 */
    { 218, "LLO",     true,  true,  false, DR_KIND_MLT01, BOUNDS_FF(0, 1000),  HAS_DEFAULT(300), DR_GROUP_NONE, "p4" }, /* p4 */
    { 219, "LHI",     true,  true,  false, DR_KIND_MLT01, BOUNDS_FF(0, 1000),  HAS_DEFAULT(200), DR_GROUP_NONE, "p4" }, /* p4 */
    { 221, "ACL",     true,  false, false, DR_KIND_MLT1,  BOUNDS_FF(0, 32000), HAS_DEFAULT(0),   DR_GROUP_NONE, "p4" }, /* p4 */
    { 222, "ACL_TIM", true,  false, false, DR_KIND_MLT1,  BOUNDS_FD(0),        NO_DEFAULT,       DR_GROUP_NONE, "p4" }, /* p4 */
    /* 230/234 printed unstarred, kept as printed (p4). */
    { 230, "MVVP3",   true,  false, false, DR_KIND_MLT1,  BOUNDS_DD,           HAS_DEFAULT(920), DR_GROUP_NONE, "p4" }, /* p4 */
    /* 231 bounds 50..400 are the printed absolute "5-40 °C"; SPL-SPH / SPL_W-SPH_W
     * ("SHP" typo, inferred) dependent limits applied by later phases, not here. */
    { 231, "SP",      true,  false, false, DR_KIND_MLT01, BOUNDS_FF(50, 400),  HAS_DEFAULT(200), DR_GROUP_FAST, "p4" }, /* p4 p6 */
    /* 233 Min/Max columns are the enum span; validity is the set {0, 3, 5}. */
    { 233, "Man",     true,  false, false, DR_KIND_ENUM,  BOUNDS_FF(0, 5),     HAS_DEFAULT(3),   DR_GROUP_FAST, "p4" }, /* p4 */
    { 234, "MVVP2",   true,  false, false, DR_KIND_MLT1,  BOUNDS_DF(1500),     HAS_DEFAULT(1220),DR_GROUP_NONE, "p4" }, /* p4 */
    /* 242-244 signedness inferred from "±12.0 K"; all others unsigned. */
    { 242, "OS1",     true,  false, true,  DR_KIND_MLT01, BOUNDS_FF(-120, 120),HAS_DEFAULT(0),   DR_GROUP_NONE, "p4" }, /* p4 */
    { 243, "OS2",     true,  true,  true,  DR_KIND_MLT01, BOUNDS_FF(-120, 120),HAS_DEFAULT(0),   DR_GROUP_NONE, "p4" }, /* p4 */
    { 244, "OS3",     true,  true,  true,  DR_KIND_MLT01, BOUNDS_FF(-120, 120),HAS_DEFAULT(0),   DR_GROUP_NONE, "p4" }, /* p4 */
    { 245, "SPL_W",   true,  false, false, DR_KIND_MLT01, BOUNDS_FD(50),       HAS_DEFAULT(200), DR_GROUP_SLOW, "p4" }, /* p4 */
    { 246, "SPH_W",   true,  false, false, DR_KIND_MLT01, BOUNDS_DF(400),      HAS_DEFAULT(240), DR_GROUP_SLOW, "p4" }, /* p4 p6 */
    { 247, "WEB",     true,  false, false, DR_KIND_FLAGS, BOUNDS_NONE,         HAS_DEFAULT(0),   DR_GROUP_SLOW, "p4" }, /* p4 p6 */
};

#define REGS_COUNT ((size_t)(sizeof(k_regs) / sizeof(k_regs[0])))

/* ------------------------------------------------------------------ API */

size_t daikin_regs_count(void)
{
    return REGS_COUNT;
}

const dr_reg_t *daikin_regs_at(size_t i)
{
    if (i >= REGS_COUNT) {
        return NULL;
    }
    return &k_regs[i];
}

const dr_reg_t *daikin_regs_find(uint16_t reg)
{
    /* Linear scan over 32 entries is fast enough; avoids a binary-search
     * dependency and keeps the table order authoritative. */
    for (size_t i = 0; i < REGS_COUNT; i++) {
        if (k_regs[i].reg == reg) {
            return &k_regs[i];
        }
    }
    return NULL;
}

bool daikin_regs_read_allowed(uint16_t start, uint16_t qty)
{
    /* Architecture D6: FC03 qty 1 in phase 1, but the whitelist must work for
     * qty 1-4 as the probe will test multi-register reads (probe item 5). */
    if (qty < 1 || qty > 4) {
        return false;
    }
    /* Detect start + qty overflow (would address a register outside uint16 range). */
    if ((uint32_t)start + (uint32_t)qty >= 0x10000u) {
        return false;
    }
    for (uint16_t r = start; r < (uint16_t)(start + qty); r++) {
        if (daikin_regs_find(r) == NULL) {
            return false;
        }
    }
    return true;
}

int32_t daikin_regs_value(const dr_reg_t *r, uint16_t raw)
{
    if (r->is_signed) {
        /* Sign-extend 16-bit two's complement to int32. */
        return (int32_t)(int16_t)raw;
    }
    return (int32_t)raw;
}

bool daikin_regs_in_range(const dr_reg_t *r, uint16_t raw)
{
    if (r->kind == DR_KIND_FLAGS) {
        return true;  /* no fixed range on flag registers */
    }
    if (r->kind == DR_KIND_ENUM) {
        /* Check membership in the allowed set. */
        for (size_t i = 0; i < sizeof(k_man_allowed); i++) {
            if (raw == k_man_allowed[i]) {
                return true;
            }
        }
        return false;
    }
    /* MLT1 / MLT01: check fixed bounds only; dep bounds are ignored here. */
    int32_t v = daikin_regs_value(r, raw);
    if (r->has_min && v < r->min_raw) {
        return false;
    }
    if (r->has_max && v > r->max_raw) {
        return false;
    }
    return true;
}

uint16_t daikin_regs_write_mask(uint16_t reg)
{
    /* Phase-3 write whitelist (architecture D9):
     *   201 PRG:  bits 0-2 (mode) and bit 7 (standby) = 0x0087
     *             bit 4 (LOCK), undocumented bits 3/5/6, reserved 8-15 preserved.
     *   231 SP:   full word writable = 0xFFFF
     *   233 Man:  full word writable = 0xFFFF
     *   all others (incl. 247 WEB, 200 ADR): 0 — this node never writes them. */
    switch (reg) {
    case 201: return 0x0087u;  /* bits 0-2 mode + bit 7 standby (p5) */
    case 231: return 0xFFFFu;  /* absolute setpoint (p6) */
    case 233: return 0xFFFFu;  /* seasonal mode */
    default:  return 0x0000u;
    }
}
