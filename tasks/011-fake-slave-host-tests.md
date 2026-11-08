# 011 - Fake slave, probe-log fixtures, end-to-end host tests

Phase: 1. Status: spec ready (v0.3, 2026-11-08; amended 2026-11-09: automatic polling start, scenario 10). Depends on: 010.
Review: `firmware-reviewer` reviews this task independently (these tests are the evidence for the pacing and fallback rules).

## Goal
Prove on the host, end to end through `poll_step` -> `poll_sched` -> `reg_model`, that phase-1 polling behaves as specified against a scripted fake slave behind the link seam (D12; fake at the `ps_link_fn` seam user-approved 2026-11-09): boot auto-start, pacing, retry, backoff, DEGRADED/OFFLINE and recovery, unsupported registers, STOPPED/LATCHED, sweep. The fake can also be driven by a probe log in `txn_log` v1 format, so an on-site log from 008 becomes a fixture without conversion.

## Files / directories owned
- `firmware/test/host/main/fake_slave.h`, `fake_slave.c` (test-only; never linked into firmware; lint skips `test/`)
- `firmware/test/host/main/test_e2e_poll.c`
- `firmware/test/fixtures/README.md`, `healthy-synthetic.log`, `starred-timeout-synthetic.log`
- `firmware/test/host/main/CMakeLists.txt` (add the files; `FIXTURE_DIR` exists since 001)

## Wiki pages relied on
- `concepts/firmware-architecture`, sections "1. Phases and exit criteria", "2. Components and threading" (seam), "3. Bus-safety rules" (D16, D6, D21), "6. Decisions" (D12)
- `concepts/register-map` (starred list; values used lie in the documented ranges)

## Interfaces (`fake_slave.h`)
```c
typedef struct fake_slave fake_slave_t;     /* defined in the header, static allocation */
void fake_init(fake_slave_t *f, uint8_t addr, int64_t *clock_ms, uint32_t resp_timeout_ms); /* advances *clock_ms */
void fake_set_reg(fake_slave_t *f, uint16_t reg, uint16_t raw, uint32_t latency_ms);
void fake_set_exception(fake_slave_t *f, uint16_t reg, uint8_t code);
void fake_set_silent(fake_slave_t *f, uint16_t reg);                 /* no response -> TIMEOUT */
void fake_set_dead(fake_slave_t *f, int64_t from_ms, int64_t to_ms); /* every request TIMEOUT in [from, to) */
void fake_fail_every(fake_slave_t *f, uint32_t n);                   /* every n-th request TIMEOUT, 0 = off */
int  fake_load_log(fake_slave_t *f, const char *path);               /* replay mode; -1 on parse error (line number printed) */
ps_res_t fake_link(void *ctx, uint8_t addr, uint16_t reg, uint16_t qty, uint16_t *val, uint8_t *exc); /* ps_link_fn */
int64_t  fake_clock(void *ctx);                                     /* ps_clock_fn */
size_t   fake_journal(const fake_slave_t *f, const ps_txn_t **out); /* every request with timing, for assertions */
```
Behaviour: TIMEOUT advances the clock by `resp_timeout_ms` (500); an answer by its latency (default 25 ms); other addresses TIMEOUT; the fake fails the test (`TEST_FAIL`) on any request with `addr` outside 1..247, `!daikin_regs_read_allowed(reg, qty)`, or qty > 1 with kind != MANUAL.

Replay mode: reads a `txn_log` v1 file via `txn_log_parse`; per `(addr, reg, qty)` keeps the recorded results in order (result, exc code, values, `dur_ms` as latency); a request gets the next recorded result for its key, then the last one repeats; unrecorded keys -> TIMEOUT. `H` and ignored lines are skipped.

Fixture header (extends the 010 header): `# dmt-probe v1 addr=<a> baud=<b> fw=<x> source=synthetic|capture origin=<free text>`.

Synthetic values: `daikin_regs` defaults where present, else 0, except 000 = 0x00d7 (21.5 °C) and 008 = 0x00c8; a test asserts all are within the documented ranges. `healthy-synthetic.log`: one OK line per unstarred register, `EXC 02` per starred register. `starred-timeout-synthetic.log`: same, starred registers `TIMEOUT`.

## Scenarios (`test_e2e_poll.c`; config = 001 Kconfig defaults: gap 100, fast 5 s, slow 60 s, retries 1, offline_after 5, health probe 30 s; stale = 3 x period; each starts with `poll_sched_start` unless stated)
1. Healthy, starred = exception 02, 130 s: ONLINE throughout; 104/009/001 polled exactly once then never again (`unsupported`); every consecutive pair has `t_start[i+1] - t_end[i] >= 100`; each unstarred fast reg polled 26 +/- 1 times, each unstarred slow reg 3 +/- 1 times; no request for a group-N register; average rate <= 2 requests/s.
2. Starred silent (TIMEOUT): never OFFLINE (at most DEGRADED); starred regs `RM_TIMEOUT`, each with one RETRY per attempt and then polled at most once per 60 s.
3. Dead link [20 s, 200 s): DEGRADED after the 1st, OFFLINE at the 5th counted failure; while OFFLINE only HEALTH reads of 000, spaced >= 30 s; ONLINE within one probe period after 200 s; fast regs fresh again within 10 s after that.
4. Intermittent `fake_fail_every(3)`, 300 s: never OFFLINE; `stats.retries > 0`; all unstarred fresh at the end.
5. STOPPED: in the post-`init` state before `start` and after `poll_sched_stop`: no automatic request for 120 s; one MANUAL read executes once. LATCHED: zero requests of any kind for 120 s; MANUAL and SWEEP refused.
6. Sweep from STOPPED: all 32 documented registers exactly once, ascending, gaps respected; health unchanged; starred EXC 02 marked unsupported.
7. Address change: slave at 1, `poll_sched_set_addr(2)` -> only addr-2 requests, all TIMEOUT -> OFFLINE.
8. Replay: `healthy-synthetic.log` reproduces the end state of scenario 1; `starred-timeout-synthetic.log` that of scenario 2.
9. Log round trip: each journal entry formatted with `txn_log_format_txn` and re-parsed equals the entry; replaying a run's own log reproduces its request sequence for the first 60 s.
10. Boot sequence as driven by 006 (user decision 2026-11-09: automatic start after a clean silence check), clock starting at 0: (a) clean: `init`, clock advanced by the 5000 ms silence window, `start` -> first request is POLL 000 at t = 5000, nothing before, then the first-cycle order of 010; (b) failed check: `init`, `latch` instead of `start`, then `start` -> zero requests of any kind for 120 s; (c) `mb poll off` queued during the window: `init`, `stop`, no `start` (006 skips it in this case) -> zero automatic requests for 120 s, a later `start` -> first request POLL 000 at that time.

## Acceptance checks
1. `firmware/test/host`: `idf.py build`, run `build/*.elf`: all tests pass, exit 0; runtime < 10 s (simulated time, no sleeps).
2. Each scenario broken once on purpose (e.g. assert gap >= 150, or offline_after 6 in the config) fails; report, revert.
3. Target build, lint and ELF check unchanged and passing (no firmware code touched).
4. `firmware/test/fixtures/README.md`: format (pointer to the 010 spec), naming (`<what>-synthetic.log`, `probe-<date>-capture.log`), how to turn an on-site serial log from 008 into a fixture.

## Test strategy
This task is the test strategy for the phase-1 control logic: pure host tests, fake clock, no hardware. `mb_link`/`rs485_port` are behind the seam by design.

## Recommended implementer model
sonnet: a deterministic fake with replay plus timing assertions across ten scenarios needs care; mistakes here hide real pacing bugs.

## Out of scope
- Real capture fixtures (added after the probe).
- Matter or write scenarios (phase 2/3).
- Changing `poll_sched` rules: if a scenario contradicts the 010 spec, stop and report.
