#pragma once

/* daikin_regs.h – register table, scaling, access whitelists (pure C99, no IDF).
 * Single source of register facts for the N420384A p4 register map.
 * Sources: N420384A p4 (table), p5 (PRG bits), p6 (247 / setpoint write).
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ------------------------------------------------------------------ types */

typedef enum {
    DR_KIND_MLT1,   /* raw × 1   = value in engineering unit */
    DR_KIND_MLT01,  /* raw × 0.1 = value; stored in raw tenths */
    DR_KIND_FLAGS,  /* bit field; no scaling */
    DR_KIND_ENUM    /* discrete allowed set */
} dr_kind_t;

typedef enum {
    DR_GROUP_FAST,  /* polled every 5 s  (architecture D6) */
    DR_GROUP_SLOW,  /* polled every 60 s (architecture D6) */
    DR_GROUP_NONE   /* not polled automatically; read by sweep / manual */
} dr_group_t;

typedef struct {
    uint16_t    reg;         /* decimal register address */
    const char *name;        /* as printed, e.g. "T1" */
    bool        writable;    /* R/W column; informational, not a permission */
    bool        starred;     /* "* Only for INN-FR-B32 board" (p4) */
    bool        is_signed;   /* true only where inferred (242–244) */
    dr_kind_t   kind;
    int32_t     min_raw;     /* fixed printed lower bound in raw units */
    int32_t     max_raw;     /* fixed printed upper bound in raw units */
    bool        has_min;     /* false where min bound is another register */
    bool        has_max;     /* false where max bound is another register */
    bool        has_default;
    int32_t     default_raw;
    dr_group_t  group;
    const char *page;        /* source page, e.g. "p4" */
} dr_reg_t;

/* ---------------------------------------------------------------- API */

/** Returns the total number of documented registers (32). */
size_t daikin_regs_count(void);

/** Returns the i-th entry in ascending register-address order.
 *  Returns NULL if i >= daikin_regs_count(). */
const dr_reg_t *daikin_regs_at(size_t i);

/** Returns the entry for the given register address, or NULL if undocumented. */
const dr_reg_t *daikin_regs_find(uint16_t reg);

/** Returns true if an FC03 read of [start, start+qty) is fully documented.
 *  Requires 1 <= qty <= 4 and every register in the range to be in the table.
 *  start+qty overflow is detected and returns false. */
bool daikin_regs_read_allowed(uint16_t start, uint16_t qty);

/** Returns the signed value of raw.
 *  If is_signed, raw is treated as int16_t (sign-extended to int32_t).
 *  MLT01 stays in raw tenths (the caller divides by 10 for display). */
int32_t daikin_regs_value(const dr_reg_t *r, uint16_t raw);

/** Returns true if raw is within the fixed bounds.
 *  FLAGS: always true (no fixed range check).
 *  ENUM: true only if raw is in the allowed set.
 *  dep bounds (has_min/has_max false) are not checked here. */
bool daikin_regs_in_range(const dr_reg_t *r, uint16_t raw);

/** Phase-3 data: bitmask of bits a write may change.
 *  0 = this node never writes this register.
 *  201 -> 0x0087 (bits 0-2 and 7; bit 4 LOCK, undocumented bits 3/5/6, reserved 8-15 preserved).
 *  231 -> 0xFFFF, 233 -> 0xFFFF.
 *  Every other register (including 247, 200) -> 0. */
uint16_t daikin_regs_write_mask(uint16_t reg);

/* --------------------------------------------------------------- PRG macros */

/* PRG register (201) bit definitions (p5, [[concepts/flag-registers]]). */
#define DR_PRG_REG          201u
#define DR_PRG_MODE_MASK    0x0007u  /* bits 0-2: operating mode */
#define DR_PRG_LOCK_BIT     0x0010u  /* bit 4: keyboard lock */
#define DR_PRG_STBY_BIT     0x0080u  /* bit 7: standby */
#define DR_PRG_RESERVED     0xFF00u  /* bits 8-15: "reserved, do not change" (p5) */
