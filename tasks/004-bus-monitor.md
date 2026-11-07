# 004 - `bus_monitor`: request/response pairing, register mirror, bus statistics (pure)

Phase: P1. Status: spec ready. Depends on: 003.

## Goal
Pure C module that consumes `rtu_frame_t` from `rtu_sniff` and maintains (a) a mirror of every register value seen on the bus with provenance and age, (b) traffic counters, (c) timing statistics needed to answer the P1 questions (who polls what, how often, how long the bus is idle between transactions) and to design the P3 idle-window rule. Includes the documented Daikin register table for naming. No IDF, FreeRTOS or hardware calls; all time comes in as arguments.

## Zero-TX rules (P1/P2, binding)
- Receive-side analysis only; no transmit, UART or esp-modbus controller call, no "poll if missing" logic (the node never asks the bus anything in P1/P2).
- Forbidden outside `components/bus_tx/`: `mbc_master_*`, `mbc_slave_*`, `uart_write_bytes*`, `uart_tx_chars`, `uart_set_mode`, `uart_set_rts`, `uart_set_dtr`, `uart_set_loop_back`, `_uart_set_pin4/6`, esp-modbus controller headers. Build lint enforces it.
- Hardware: DE and /RE to GND, DI unconnected; UART in `UART_MODE_UART`, RX only.

## Files / directories owned
- `firmware/components/bus_monitor/` (new): `CMakeLists.txt` (`REQUIRES rtu_sniff`, `-Wall -Wextra -Werror`), `include/bus_monitor.h`, `include/daikin_regs.h`, `src/bus_monitor.c`, `src/daikin_regs.c`, `test_host/test_bus_monitor.c`, `test_host/test_daikin_regs.c`
- `firmware/test/host/main/CMakeLists.txt` (add `bus_monitor` to `REQUIRES`)

## Wiki pages relied on
- `concepts/firmware-architecture` (sections "bus_monitor", "P1 measurement goals")
- `concepts/register-map` (register numbers, names, R/W, multipliers, `*` = INN-FR-B32 only) - the table below is copied from it; if the wiki differs from this spec, the wiki wins and you report the difference
- `concepts/flag-registers` (009, 104, 105, 201 are flag registers)
- `concepts/bus-topology` (open items 1, 1b, 2, 4: what P1 must measure)
- `concepts/device-addressing` (address 1-255; 0 = broadcast is external Modbus knowledge)

## Interfaces
`daikin_regs.h` (documented registers only; anything else is "undocumented"):
```c
typedef enum { DREG_SCALE_1, DREG_SCALE_0_1, DREG_FLAGS } dreg_scale_t;
typedef struct { uint16_t reg; const char *name; dreg_scale_t scale; bool writable; bool inn_fr_b32_only; bool signed_inferred; } dreg_info_t;
const dreg_info_t *daikin_reg_find(uint16_t reg);   /* NULL if undocumented */
```
Table content (from `concepts/register-map`): 000 T1 0.1 R; 001 T2 0.1 R *; 008 SP 0.1 R; 009 OUT flags R *; 015 MOT_SET 1 R *; 104 STAT flags R *; 105 ALR_STAT flags R; 200 ADR 1 RW; 201 PRG flags RW; 202 SPL 0.1 RW; 203 SPH 0.1 RW; 209 E_SAVING 0.1 RW; 210 MVV5 1 RW *; 211 MVV4 1 RW *; 212 MVV3 1 RW *; 213 MVV2 1 RW *; 214 MVV1 1 RW *; 215 MVVP1 1 RW *; 218 LLO 0.1 RW *; 219 LHI 0.1 RW *; 221 ACL 1 RW; 222 ACL_TIM 1 RW; 230 MVVP3 1 RW; 231 SP 0.1 RW; 233 Man 1 RW; 234 MVVP2 1 RW; 242 OS1 0.1 RW; 243 OS2 0.1 RW *; 244 OS3 0.1 RW *; 245 SPL_W 0.1 RW; 246 SPH_W 0.1 RW; 247 WEB flags RW. `signed_inferred = true` only for 242-244 (range +-12.0 K; two's complement is inferred, not documented). No ranges, defaults or semantics are encoded here (P2 mapping work).

`bus_monitor.h`:
```c
#define BUS_MON_MAX_REGS 64
typedef enum { BUS_SRC_FC03_RESP, BUS_SRC_FC06_CONFIRMED, BUS_SRC_FC06_BROADCAST } bus_src_t;
typedef struct {
    uint8_t addr; uint16_t reg; uint16_t value; bus_src_t last_src;
    int64_t t_last_us; uint32_t n_reads; uint32_t n_writes;
    int64_t min_interval_us, max_interval_us;      /* between successive observations; -1 until known */
} bus_reg_entry_t;
typedef struct { uint32_t count; int64_t min_us, max_us, sum_us; uint32_t hist[10]; } bus_time_stat_t;
/* histogram edges in ms: <2, <5, <10, <20, <50, <100, <200, <500, <1000, >=1000 */
typedef struct {
    uint32_t fc03_req, fc03_resp_matched, fc06_req, fc06_confirmed, fc06_broadcast, exceptions,
             unanswered, orphan_responses, other_fc, crc_errors, oversize, undocumented_reg_hits, mirror_full;
    bus_time_stat_t gap;          /* start of frame n+1 minus end of frame n, all frames */
    bus_time_stat_t idle_gap;     /* end of a completed transaction to start of the next request (P3 input) */
    bus_time_stat_t latency;      /* end of request to start of its response */
    uint8_t addrs_seen[8]; uint32_t addr_requests[8]; uint8_t n_addrs;   /* slave addresses addressed by requests */
    uint8_t exc_fc[8], exc_code[8]; uint32_t exc_count[8]; uint8_t n_exc;
} bus_monitor_stats_t;
typedef struct { uint32_t baud; int64_t resp_timeout_us; } bus_monitor_cfg_t;
typedef struct bus_monitor bus_monitor_t;   /* defined in the header for static allocation */

void bus_monitor_init(bus_monitor_t *m, const bus_monitor_cfg_t *cfg);
void bus_monitor_on_frame(bus_monitor_t *m, const rtu_frame_t *f);
void bus_monitor_tick(bus_monitor_t *m, int64_t now_us);      /* expires a pending request */
void bus_monitor_get_stats(const bus_monitor_t *m, bus_monitor_stats_t *out);
size_t bus_monitor_copy_regs(const bus_monitor_t *m, bus_reg_entry_t *out, size_t max);   /* sorted by addr, reg */
const bus_reg_entry_t *bus_monitor_find(const bus_monitor_t *m, uint8_t addr, uint16_t reg);
void bus_monitor_reset(bus_monitor_t *m, bool keep_mirror);
```
Pairing (normative; Modbus RTU has one outstanding request at a time):
- `FC03_REQ`: an existing pending request counts `unanswered`; the new one becomes pending.
- `FC03_RESP`: matched if a pending FC03 request has the same address, byte count = 2 x count and `t_start(resp) - t_end(req) <= resp_timeout_us`; then update the mirror for `start .. start+count-1` (source `FC03_RESP`, `n_reads++`), record latency, clear pending. Otherwise `orphan_responses++`.
- `FC06`: if a pending FC06 has identical bytes within the timeout -> confirmed write (mirror source `FC06_CONFIRMED`, `n_writes++`), clear pending. Else, address 0 -> `fc06_broadcast++`, mirror under addr 0, no pending. Else a pending request counts `unanswered` and this frame becomes the pending FC06. Known limitation, document in the header: a master retry of the same FC06 within the timeout is indistinguishable from an echo.
- `EXCEPTION`: matched to a pending request with same address and `fc & 0x7F`; counted per (fc, code) (first 8 distinct pairs, later ones only in `exceptions`); clear pending; otherwise orphan.
- `OTHER_FC`, `CRC_ERROR`, `OVERSIZE`: counters only; they clear nothing.
- Every register touched that `daikin_reg_find` does not know increments `undocumented_reg_hits` (still mirrored).
- Mirror full (`BUS_MON_MAX_REGS`): count `mirror_full`, do not evict.
- Timing uses `t_start = t_end_us - len * rtu_char_time_us(baud)`; negative gaps (estimate error) are clamped to 0 and still counted.

## Acceptance checks
1. Target build and lint pass; host tests pass (exit 0).
2. No IDF/FreeRTOS includes in `bus_monitor` sources; no dynamic allocation.
3. `daikin_regs` table matches `concepts/register-map` row by row (the test enumerates all 32 documented registers).

## Test strategy
Host unit tests (Unity, `linux`); build frames through the `rtu_sniff` decode-compatible helpers (construct `rtu_frame_t` directly with valid CRC via `rtu_crc16`). Required cases:
- FC03 single and multi-register pairing updates the mirror with correct values, source and counters.
- Wrong address, wrong byte count, response after timeout -> orphan; request without response then new request -> unanswered; `bus_monitor_tick` past the timeout -> unanswered.
- FC06 request + echo -> confirmed write; FC06 without echo -> unanswered after tick; broadcast FC06.
- Exception matched/orphan; per-(fc, code) table incl. the 9th distinct pair.
- Mirror ordering, `min/max_interval_us`, `mirror_full`.
- Gap, idle_gap and latency stats incl. histogram bucket boundaries (exactly 2 ms, 1000 ms) and clamping.
- `addrs_seen` with more than 8 addresses (extra not listed, no overflow).
- Undocumented register counting (e.g. reg 500) vs documented (e.g. 231).
- `bus_monitor_reset` with and without `keep_mirror`.
- `daikin_reg_find` for every documented register and for 2, 100, 216, 232, 248 (undocumented -> NULL).

## Recommended implementer model
sonnet: stateful pairing logic with timing and many counters; correctness of the register table against the wiki needs care.

## Out of scope
- Scaling to engineering units or Matter types, flag-bit decoding, range checks (P2 `mapping`).
- Any decision logic for transmitting (P3).
- Printing (006), persistence.
