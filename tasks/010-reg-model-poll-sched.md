# 010 - `reg_model` + `poll_sched`: register state, polling schedule, link health, transaction log (pure)

Phase: 1. Status: spec ready (v0.3, 2026-11-08; amended 2026-11-09: user decisions marked below). Depends on: 009. May run in parallel with 002.
Review: `firmware-reviewer` reviews this task independently (it decides what goes on the bus and how often).

## Goal
The pure decision layer between `bus_task` and `mb_link`:
- `reg_model`: per-register value, age and availability `unknown` / `ok` / `unsupported(exc)` / `timeout` (architecture section 2).
- `poll_sched`: next FC03 request from time, groups, availability and backoff; health state machine ONLINE / DEGRADED / OFFLINE (D21) plus STOPPED (`mb poll off`) and LATCHED (silence check, D17); one-shot MANUAL reads; SWEEP of every documented register (`mb probe`, exit item 4); statistics incl. response-time histogram (exit items 6, 7).
- `poll_step`: one schedule -> link -> complete -> model step over injected link and clock functions (seam for `bus_task` and the 011 fake slave).
- `txn_log`: one-line text format for transactions and health changes, format and parse (probe log, fixtures).

No IDF, FreeRTOS or esp-modbus includes. Time = `int64_t` ms from an injected clock (no wrap handling).

## Files / directories owned
- `firmware/components/reg_model/` (new): `CMakeLists.txt` (`REQUIRES daikin_regs`), `include/reg_model.h`, `src/reg_model.c`, `test_host/test_reg_model.c`
- `firmware/components/poll_sched/` (new): `CMakeLists.txt` (`REQUIRES daikin_regs reg_model`), `include/poll_sched.h`, `include/txn_log.h`, `src/poll_sched.c`, `src/poll_step.c`, `src/txn_log.c`, `test_host/test_poll_sched.c`, `test_host/test_txn_log.c`
- `firmware/test/host/main/CMakeLists.txt` (append `reg_model poll_sched` to `REQUIRES`)

All components `-Wall -Wextra -Werror`.

## Wiki pages relied on
- `concepts/firmware-architecture`, sections "1. Phases and exit criteria" (items 4-7), "2. Components and threading", "3. Bus-safety rules" (Timing D16, Polling D6, Read errors and health D21, Fallbacks), "6. Decisions"
- `concepts/register-map` via `daikin_regs` (009); `concepts/modbus-rs485-link` (qty > 1 not stated -> automatic polling qty 1)

## Interfaces

### `reg_model.h`
```c
typedef enum { RM_UNKNOWN, RM_OK, RM_UNSUPPORTED, RM_TIMEOUT } rm_avail_t;
typedef struct {
    uint16_t reg; rm_avail_t avail; bool has_value; uint16_t raw; bool out_of_range;
    int64_t t_ok_ms, t_attempt_ms; uint8_t exc_code; uint32_t ok_count, fail_count;
} rm_entry_t;
typedef struct { rm_entry_t e[32]; int64_t stale_fast_ms, stale_slow_ms; } reg_model_t;

void reg_model_init(reg_model_t *m, int64_t stale_fast_ms, int64_t stale_slow_ms);
void reg_model_reset(reg_model_t *m);                                   /* back to init, thresholds kept */
int  reg_model_set_ok(reg_model_t *m, uint16_t reg, uint16_t raw, int64_t now_ms);           /* -1 if undocumented */
int  reg_model_set_unsupported(reg_model_t *m, uint16_t reg, uint8_t exc, int64_t now_ms);
int  reg_model_set_timeout(reg_model_t *m, uint16_t reg, int64_t now_ms);
bool reg_model_get(const reg_model_t *m, uint16_t reg, rm_entry_t *out);
bool reg_model_fresh(const reg_model_t *m, uint16_t reg, int64_t now_ms);
/* fresh = has_value && avail != UNSUPPORTED && now - t_ok <= stale threshold of the reg's group (SLOW threshold for group NONE) */
```
Failures keep `raw`/`t_ok_ms` (a value ages out by time). `out_of_range = !daikin_regs_in_range()` of the last OK value. The mutex-protected snapshot is made by `bus_task` (006); `reg_model` itself has no locking. `bus_task` passes stale = 3 x group period (15 s / 180 s; section 5 uses 15 s for LocalTemperature; user-approved 2026-11-09).

### `poll_sched.h`
```c
typedef enum { PS_STOPPED, PS_ONLINE, PS_DEGRADED, PS_OFFLINE, PS_LATCHED } ps_health_t;
typedef enum { PS_RES_OK, PS_RES_TIMEOUT, PS_RES_EXCEPTION, PS_RES_BAD_FRAME, PS_RES_ERROR } ps_res_t;
typedef enum { PS_KIND_POLL, PS_KIND_RETRY, PS_KIND_HEALTH, PS_KIND_SWEEP, PS_KIND_MANUAL } ps_kind_t;
typedef struct { uint8_t addr; uint16_t reg, qty; ps_kind_t kind; } ps_req_t;
typedef struct { ps_req_t req; ps_res_t res; uint8_t exc_code; uint16_t val[4]; int64_t t_start_ms, t_end_ms; } ps_txn_t;
typedef struct { uint8_t addr; uint32_t gap_ms, fast_ms, slow_ms, offline_probe_ms; uint8_t retries, offline_after; } ps_config_t;
#define PS_LAT_BUCKET_MS 10
#define PS_LAT_BUCKETS   201          /* 0..2000 ms */
typedef struct {
    uint32_t txns, ok, timeout, exception, bad_frame, error, retries, health_probes, sweeps, manual, offline_entries;
    uint32_t lat_min_ms, lat_max_ms, lat_hist[PS_LAT_BUCKETS];   /* answered transactions only (OK or EXCEPTION) */
} ps_stats_t;
typedef struct poll_sched poll_sched_t;   /* defined in the header for static allocation; fields private by convention */

int  poll_sched_init(poll_sched_t *s, const ps_config_t *cfg); /* -1 outside the 001 Kconfig ranges or slow_ms < fast_ms; state STOPPED */
void poll_sched_start(poll_sched_t *s, int64_t now_ms);        /* -> ONLINE, all polled regs due now; no effect when LATCHED */
void poll_sched_stop(poll_sched_t *s);                         /* -> STOPPED unless LATCHED; pending RETRY/SWEEP dropped */
void poll_sched_latch(poll_sched_t *s);                        /* -> LATCHED, permanent until re-init (reboot) */
int  poll_sched_set_addr(poll_sched_t *s, uint8_t addr, int64_t now_ms); /* 1..247 else -1; clears unsupported marks, counter, pending work */
int  poll_sched_request_manual(poll_sched_t *s, uint16_t reg, uint16_t qty); /* -1: LATCHED, !daikin_regs_read_allowed, or one pending */
int  poll_sched_request_sweep(poll_sched_t *s);                /* -1: LATCHED or a sweep running */
bool poll_sched_next(poll_sched_t *s, int64_t now_ms, ps_req_t *out, int64_t *wait_until_ms);
void poll_sched_complete(poll_sched_t *s, const ps_txn_t *t);
ps_health_t poll_sched_health(const poll_sched_t *s);
bool poll_sched_is_unsupported(const poll_sched_t *s, uint16_t reg);
void poll_sched_get_stats(const poll_sched_t *s, ps_stats_t *out);
uint32_t poll_sched_lat_median_ms(const ps_stats_t *st);       /* upper edge of the median bucket; 0 if no samples */

typedef ps_res_t (*ps_link_fn)(void *ctx, uint8_t addr, uint16_t reg, uint16_t qty, uint16_t *val, uint8_t *exc);
typedef int64_t  (*ps_clock_fn)(void *ctx);
typedef struct { ps_link_fn link; void *link_ctx; ps_clock_fn clock; void *clock_ctx; } ps_env_t;
/* true if a transaction was performed (*txn filled); false: nothing before *wait_until_ms (INT64_MAX = idle). */
bool poll_step(poll_sched_t *s, reg_model_t *m, const ps_env_t *env, ps_txn_t *txn, int64_t *wait_until_ms);
```
`ps_link_fn` has the semantics of `mb_link_read` (002); `bus_task` adapts it, 011 fakes it (seam user-approved 2026-11-09). `mb_link.h` stays the only hardware interface.

Initial state: `poll_sched_init` leaves the scheduler STOPPED, so a failed boot silence check can latch before any request is possible. Polling after boot starts automatically (user decision 2026-11-09): `bus_task` (006) calls `poll_sched_start` right after a clean silence check and `mb_link_init`. This module has no auto-start of its own.

### Normative schedule rules
1. Polled set = registers with group FAST (5 s) or SLOW (60 s) in `daikin_regs` (009); group NONE is read only by SWEEP/MANUAL. Automatic requests: `qty = 1`, `addr = cfg.addr`, FC03.
2. Gap: no request of any kind before `last_end + gap_ms` (`last_end` = `t_end_ms` of the previous transaction); `next` returns false with that time.
3. Priority when the gap allows: MANUAL > RETRY > due POLL (ONLINE/DEGRADED) or due HEALTH (OFFLINE) > SWEEP. Among due POLLs: FAST before SLOW, then earliest due, then lowest reg.
4. After a final POLL result the reg's next due = `max(prev_due + period, t_end)`; period = group period after OK, `slow_ms` after a final timeout/bad frame/error (backoff, D21).
5. Retry: TIMEOUT, BAD_FRAME, ERROR on a POLL are retried once (kind RETRY) if `retries = 1`. EXCEPTION is never retried.
6. EXCEPTION code 02 on POLL, SWEEP or MANUAL qty 1: register `unsupported` until reboot (or `set_addr`), never polled again (D21). Other exception codes: not retried, backoff as rule 4, not counted in rule 7 (user-approved 2026-11-09).
7. Consecutive-failure counter: +1 per final TIMEOUT/BAD_FRAME/ERROR of POLL or HEALTH; reset by any OK or EXCEPTION of those kinds. Health: 0 -> ONLINE, 1..`offline_after`-1 -> DEGRADED (user-approved 2026-11-09), reaching `offline_after` -> OFFLINE (pending RETRY dropped, first HEALTH at `t_end + offline_probe_ms`).
8. OFFLINE: only HEALTH reads of reg 000, qty 1, every `offline_probe_ms` after the previous one ended; OK -> ONLINE with every polled reg due at `t_end`. SWEEP is suspended while OFFLINE.
9. STOPPED: no POLL/HEALTH; MANUAL and SWEEP allowed. LATCHED: `next` always false with `INT64_MAX`; MANUAL and SWEEP refused.
10. SWEEP: every documented register (all 32, ascending) once, qty 1, no retry, no effect on counter/health/schedule except rule 6. MANUAL: qty 1..4, once, no retry, no effect except rule 6.
11. Every completed transaction updates `ps_stats_t`; answered ones (OK, EXCEPTION) add `t_end - t_start` to the histogram (clamped to the last bucket) and min/max.

### `poll_step` model updates
OK: `reg_model_set_ok` for each reg in `[reg, reg+qty)`. EXCEPTION, qty 1: `set_unsupported(exc)`. Final TIMEOUT/BAD_FRAME/ERROR, qty 1 (after the retry, if any): `set_timeout`. Failures of qty > 1 requests do not touch the model. `t_start_ms`/`t_end_ms` from the clock before/after the link call.

### `txn_log.h` (format v1, normative; 006, 008 and 011 rely on it)
- Header line (written by 006): `# dmt-probe v1 addr=<a> baud=<b> fw=<version>`; any line starting with `#` is a comment.
- Transaction: `T <t_start_ms> <dur_ms> <addr> <kind> <reg> <qty> <res>[ <data>]`
  - kind `POLL|RETRY|HEALTH|SWEEP|MANUAL`; reg 3-digit zero-padded decimal; addr, qty, times decimal.
  - `OK` + qty values as 4-digit lowercase hex, space separated; `EXC` + code as 2-digit hex; `TIMEOUT`, `BADFRAME`, `ERROR` without data.
  - Examples: `T 120345 24 1 POLL 000 1 OK 00d7`, `T 121002 31 1 SWEEP 104 1 EXC 02`.
- Health change: `H <t_ms> <STOPPED|ONLINE|DEGRADED|OFFLINE|LATCHED>`.
```c
typedef struct { enum { TL_TXN, TL_HEALTH } type; ps_txn_t txn; int64_t t_ms; ps_health_t health; } tl_rec_t;
size_t txn_log_format_txn(char *buf, size_t n, const ps_txn_t *t);            /* length; 0 if n too small (buf NUL-terminated) */
size_t txn_log_format_health(char *buf, size_t n, int64_t t_ms, ps_health_t h);
int    txn_log_parse(const char *line, tl_rec_t *out);   /* 1 parsed, 0 ignored (not starting with "T " or "H "), -1 malformed */
```
No trailing newline in formatted lines; parse accepts trailing `\r\n`/`\n`. Malformed = a `T`/`H` line with wrong field count, unknown token, non-hex data or data count != qty.

## Acceptance checks
1. Host tests pass, exit 0. Target build passes with zero warnings; lint passes.
2. `grep -rE '#include *[<"](esp_|freertos)' firmware/components/{reg_model,poll_sched}` is empty.

## Test strategy (host, Unity; fake clock and inline fake link in the test files)
- `reg_model`: init (all UNKNOWN), set_ok/get, fresh at age == threshold and stale at +1 ms per group, UNSUPPORTED -> not fresh but raw kept, TIMEOUT keeps freshness by age, out_of_range (231 raw 401), undocumented reg -> -1, reset.
- `poll_sched`: one test per rule 1-11, plus: init range rejections; init state STOPPED; `start` right after `init` -> first request POLL 000 at `now`; first cycle order 000, 008, 104, 201, 231, 233, then 001, 009, 105, 202, 203, 245, 246, 247; exact gap waits; fast reg due again at start + 5000 ms with drift rule; retries 0 vs 1; health ONLINE -> DEGRADED (1-4) -> OFFLINE at the 5th counted failure, DEGRADED -> ONLINE on OK; OFFLINE HEALTH every 30 s, OK -> ONLINE; 104 EXC 02 -> never polled again; sweep of all 32 exactly once, behind due POLLs, suspended in OFFLINE; manual in STOPPED, refused when LATCHED / invalid range / already pending; latch from each state; set_addr applies to the next request and clears unsupported; latency histogram, min/max and median.
- `poll_step`: model updates per the rules above, durations from the clock, `wait_until` propagated.
- `txn_log`: exact strings per res and kind; round trip parse(format(x)) == x; ignored lines (`I (123) foo`, empty, `#...`); malformed cases; truncation.

## Recommended implementer model
sonnet: a stateful scheduler with eleven interacting rules and a test per rule needs judgement in edge cases (gap vs priority, DEGRADED/OFFLINE transitions, sweep suspension).

## Out of scope
- Any bus, UART, esp-modbus, FreeRTOS code (002, 006).
- Matter null/stale policy (phase 2); writes, readback, debounce and caps (phase 3).
- Automatic qty > 1 polling (only after exit item 5); re-tuning timeout to max(3 x observed max, 100 ms) (after the probe log).
