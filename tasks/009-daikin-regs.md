# 009 - `daikin_regs`: register table, scaling, access whitelists (pure)

Phase: 1. Status: spec ready (v0.3, 2026-11-08). Depends on: 001. May run in parallel with 002.
Review: `firmware-reviewer` reviews this task independently and checks the expected table below against the wiki row by row (the read whitelist gates every bus request).

## Goal
A pure C component that is the single source of register facts in the firmware: the 32 registers of the N420384A p4 map with name, access, scaling, range, default, starred flag and poll group; the read whitelist (FC03) and the phase-3 write whitelist data (no write code); PRG 201 bit masks. Host tests compare the table with the expected table in this spec.

## Files / directories owned
- `firmware/components/daikin_regs/` (new): `CMakeLists.txt` (no `REQUIRES` beyond libc; `-Wall -Wextra -Werror`), `include/daikin_regs.h`, `src/daikin_regs.c`, `test_host/test_daikin_regs.c`
- `firmware/test/host/main/CMakeLists.txt` (append `daikin_regs` to `REQUIRES`)

## Wiki pages relied on
- `concepts/register-map` (table, star list, notes; source N420384A p4, p6)
- `concepts/flag-registers` (PRG 201 bits, p5), `concepts/web-restriction-flags` (247, p6)
- `concepts/firmware-architecture`, sections "2. Components and threading", "3. Bus-safety rules" (Polling D6; Writes: whitelist D9, never-written list, 247 read-only D18), "6. Decisions"

## Interfaces (`include/daikin_regs.h`, C99, no IDF headers)
```c
typedef enum { DR_KIND_MLT1, DR_KIND_MLT01, DR_KIND_FLAGS, DR_KIND_ENUM } dr_kind_t;
typedef enum { DR_GROUP_FAST, DR_GROUP_SLOW, DR_GROUP_NONE } dr_group_t; /* NONE: read only by sweep/manual */
typedef struct {
    uint16_t reg;          /* decimal register address */
    const char *name;      /* as printed, e.g. "T1" */
    bool writable;         /* R/W column; informational, not a permission */
    bool starred;          /* "* Only for INN-FR-B32 board" */
    bool is_signed;        /* true only where inferred (242-244) */
    dr_kind_t kind;
    int32_t min_raw, max_raw;   /* fixed printed bounds in raw units; see has_min/has_max */
    bool has_min, has_max;      /* false where the bound is another register */
    bool has_default; int32_t default_raw;
    dr_group_t group;
    const char *page;      /* "p4" */
} dr_reg_t;

size_t daikin_regs_count(void);                 /* 32 */
const dr_reg_t *daikin_regs_at(size_t i);       /* table order = ascending reg */
const dr_reg_t *daikin_regs_find(uint16_t reg); /* NULL if undocumented */
bool daikin_regs_read_allowed(uint16_t start, uint16_t qty); /* 1 <= qty <= 4 and every reg in [start, start+qty) documented */
int32_t daikin_regs_value(const dr_reg_t *r, uint16_t raw);  /* raw as int32, sign-extended if is_signed; MLT01 stays in tenths */
bool daikin_regs_in_range(const dr_reg_t *r, uint16_t raw);  /* fixed bounds only; ENUM: member of the allowed set; FLAGS: true */
uint16_t daikin_regs_write_mask(uint16_t reg);   /* phase-3 data: bits a write may change; 0 = not writable by the node */

#define DR_PRG_REG          201
#define DR_PRG_MODE_MASK    0x0007u  /* bits 0-2 */
#define DR_PRG_LOCK_BIT     0x0010u  /* bit 4 */
#define DR_PRG_STBY_BIT     0x0080u  /* bit 7 */
#define DR_PRG_RESERVED     0xFF00u  /* bits 8-15: "reserved, do not change" (p5) */
```
- 233 ENUM allowed set: {0, 3, 5}.
- `daikin_regs_write_mask`: 201 -> `0x0087` (bits 0-2 and 7; bit 4, undocumented bits 3/5/6 and reserved 8-15 preserved), 231 -> `0xFFFF`, 233 -> `0xFFFF`, every other register (incl. 247, 200) -> `0`. Data only; nothing calls it in phase 1.
- Each table row carries a comment `/* p4 */` (and `p5`/`p6` where a note is used).

## Expected table (transcribed from `concepts/register-map`, N420384A p4; raw units, MLT01 = tenths)
`dep` = bound is another register (has_min/has_max false). Group F = fast (5 s), S = slow (60 s), N = not polled automatically (architecture section 3 "Polling (D6)").

| Reg | Name | W | * | Kind | Min | Max | Default | Signed | Grp |
|---|---|---|---|---|---|---|---|---|---|
| 0 | T1 | n | n | MLT01 | 0 | 1000 | - | n | F |
| 1 | T2 | n | y | MLT01 | 0 | 1000 | - | n | S |
| 8 | SP | n | n | MLT01 | 0 | 1000 | - | n | F |
| 9 | OUT | n | y | FLAGS | - | - | - | n | S |
| 15 | MOT_SET | n | y | MLT1 | 0 | 1700 | - | n | N |
| 104 | STAT | n | y | FLAGS | - | - | - | n | F |
| 105 | ALR_STAT | n | n | FLAGS | - | - | - | n | S |
| 200 | ADR | y | n | MLT1 | 1 | 255 | 1 | n | N |
| 201 | PRG | y | n | FLAGS | - | - | 0 | n | F |
| 202 | SPL | y | n | MLT01 | 50 | dep | 160 | n | S |
| 203 | SPH | y | n | MLT01 | dep | 350 | 280 | n | S |
| 209 | E_SAVING | y | n | MLT01 | 0 | 85 | 0 | n | N |
| 210 | MVV5 | y | y | MLT1 | 400 | dep | 400 | n | N |
| 211 | MVV4 | y | y | MLT1 | dep | dep | 550 | n | N |
| 212 | MVV3 | y | y | MLT1 | dep | dep | 680 | n | N |
| 213 | MVV2 | y | y | MLT1 | dep | 1500 | 1100 | n | N |
| 214 | MVV1 | y | y | MLT1 | dep | 1500 | 1500 | n | N |
| 215 | MVVP1 | y | y | MLT1 | dep | 1700 | 1700 | n | N |
| 218 | LLO | y | y | MLT01 | 0 | 1000 | 300 | n | N |
| 219 | LHI | y | y | MLT01 | 0 | 1000 | 200 | n | N |
| 221 | ACL | y | n | MLT1 | 0 | 32000 | 0 | n | N |
| 222 | ACL_TIM | y | n | MLT1 | 0 | dep | - | n | N |
| 230 | MVVP3 | y | n | MLT1 | dep | dep | 920 | n | N |
| 231 | SP | y | n | MLT01 | 50 | 400 | 200 | n | F |
| 233 | Man | y | n | ENUM | 0 | 5 | 3 | n | F |
| 234 | MVVP2 | y | n | MLT1 | dep | 1500 | 1220 | n | N |
| 242 | OS1 | y | n | MLT01 | -120 | 120 | 0 | y | N |
| 243 | OS2 | y | y | MLT01 | -120 | 120 | 0 | y | N |
| 244 | OS3 | y | y | MLT01 | -120 | 120 | 0 | y | N |
| 245 | SPL_W | y | n | MLT01 | 50 | dep | 200 | n | S |
| 246 | SPH_W | y | n | MLT01 | dep | 400 | 240 | n | S |
| 247 | WEB | y | n | FLAGS | - | - | 0 | n | S |

Transcription notes (keep as comments in `daikin_regs.c`):
- 231 bounds 50..400 are the printed absolute "5-40 °C"; the printed dependent limits SPL-SPH / SPL_W-SPH_W ("SHP" typo, inferred) are applied by later phases, not here.
- 233 Min/Max columns are the enum span; validity is the set {0, 3, 5}.
- Signedness of 242-244 is inferred from "±12.0 K"; all others unsigned.
- 210 printed min "400-(MVV3-4)": min 400 fixed, max dep. 230/234 printed unstarred, kept as printed.
- Poll groups are architecture decisions (v0.3 D6, section 3), not manual facts: fast 000, 008, 201, 231, 233 + 104; slow 105, 202, 203, 245, 246, 247 + 009, 001. The starred ones (104, 009, 001) are polled "if they answer": `poll_sched` drops them after exception 02 (010).

## Acceptance checks
1. Target build passes with the component required by nothing yet (add it to `main` `REQUIRES` only if needed to compile it; then also 0 warnings).
2. Host tests pass, exit 0 (`firmware/test/host`: `idf.py build` then run `build/*.elf`).
3. Lint passes.

## Test strategy (host, Unity, all pure)
- Transcription: the test file holds its own copy of the expected table above (typed from this spec, not generated from the implementation) and asserts count 32, ascending unique regs, every field per row, and the starred set equals {1, 9, 15, 104, 210, 211, 212, 213, 214, 215, 218, 219, 243, 244}; group sizes 6 F / 8 S / 18 N.
- `find`: every documented reg found; 2, 16, 199, 216, 217, 220, 223, 232, 235, 248, 0xFFFF -> NULL.
- `read_allowed`: (0,1) (0,2) (200,4) (242,4) (244,4) true; (1,2) (reg 2 undocumented), (247,2), (0,0), (0,5), (216,1), (0xFFFF,1) false; start+qty overflow handled.
- `value`: 242 raw 0xFF88 -> -120; 0 raw 215 -> 215; 15 raw 1700 -> 1700.
- `in_range`: boundaries inclusive, one below/above each fixed bound; 233: 0/3/5 true, 1/2/4/6 false; FLAGS always true; dep bounds ignored.
- `write_mask`: exact values for 201, 231, 233; 0 for 247, 200 and every other documented reg.

## Recommended implementer model
sonnet: transcription is mechanical, but the dependent-bound/enum/signedness rules and an independent expected table need care; haiku risks copying the implementation into the test.

## Out of scope
- Any bus, UART or Matter code; dependent-limit evaluation (202/203/245/246 interplay); Matter conversions (phase 2); write logic (phase 3).
- Bit definitions of 009, 104, 105, 247 (phase 2 spec adds what the mapping needs).
