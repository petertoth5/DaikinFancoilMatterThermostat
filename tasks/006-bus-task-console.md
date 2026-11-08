# 006 - `bus_task` poll loop, probe console (`mb ...`), `app_main`

Phase: 1. Status: spec ready (v0.3, 2026-11-08; amended 2026-11-09: automatic polling start, `mb log` default on, queue-drain and `mb silence` behaviour confirmed; user decisions). Depends on: 002, 010.
Review: **bus-touching**. `firmware-reviewer` reviews this task independently.

## Goal
The phase-1 probe image: one task, the single owner of `mb_link`, runs the boot silence check, then drives `poll_step` (010) over `mb_link_read` (002), publishes snapshots and serves the `mb` console commands the on-site checklist (008) uses. `app_main` holds the transceiver in receive first, then starts the task and the REPL.

## Bus-safety rules (binding; architecture section 3)
- First statement of `app_main`: `mb_link_hold_receive()`.
- **Nothing is sent before the boot silence check passed** (`CONFIG_DMT_BUS_BOOT_SILENCE_MS`, D17). Activity > 0, or any error of the check or of `mb_link_init`, -> `poll_sched_latch()`; `mb_link_init` is then never called until reboot.
- After a passed check and a successful `mb_link_init`, **polling starts automatically** (`poll_sched_start`, user decision 2026-11-09, architecture section 3 "Fallbacks"): the first frame goes out about `boot_silence_ms` (5 s) after boot. This is the only automatic start path. `mb poll off` / `mb poll on` stop and resume it; `mb probe` / `mb read` also work while stopped (010 rules 9, 10).
- Commands queued while the silence check runs are all executed before the first `poll_step`, so a `mb poll off` typed during the 5 s window prevents the first automatic frame (abort path used by 008; user-approved 2026-11-09).
- Every request goes through `poll_sched`/`poll_step` (gap, retry, health); console reads are MANUAL/SWEEP requests, never direct `mb_link_read` calls.
- Only `bus_task` calls `mb_link_*` (after `app_main`'s first hold call).
- No write path exists (lint R7, ELF check); `mb status` prints `write: compiled out`.
- Console never on the bus UART: compile-time `#error` if a UART console uses `CONFIG_DMT_BUS_UART_PORT` (check the v6.0.2 console symbol names) plus a runtime check at start (architecture section 2, console rule).

## Files / directories owned
- `firmware/components/bus_task/` (new): `CMakeLists.txt` (`REQUIRES mb_link poll_sched reg_model daikin_regs console esp_timer freertos esp_app_format`, `-Wall -Wextra -Werror`), `include/bus_task.h`, `src/bus_task.c`, `src/mb_console.c`, `src/log_out.c`
- `firmware/main/main.c`, `firmware/main/CMakeLists.txt` (`REQUIRES bus_task`)

## Wiki pages relied on
- `concepts/firmware-architecture`, sections "1. Phases and exit criteria", "2. Components and threading" (component table: console commands; Threading), "3. Bus-safety rules" (Fallbacks, D16, D17, D21), "6. Decisions" (D19 address)
- `concepts/register-map`, `concepts/flag-registers` (via `daikin_regs`: names, scaling; flag registers printed in binary)

## Interfaces
```c
typedef struct { mb_link_config_t link; ps_config_t sched; uint32_t boot_silence_ms; } bus_task_config_t;
esp_err_t bus_task_start(const bus_task_config_t *cfg);
typedef struct { reg_model_t model; ps_stats_t stats; ps_health_t health; uint8_t addr;
                 uint32_t boot_activity; bool log_on; uint32_t log_dropped; uint32_t stack_hwm; } bus_snapshot_t;
void bus_task_get_snapshot(bus_snapshot_t *out);   /* copy under mutex; console now, Matter in phase 2 */
typedef enum { BT_CMD_POLL_ON, BT_CMD_POLL_OFF, BT_CMD_READ, BT_CMD_PROBE, BT_CMD_ADDR, BT_CMD_SILENCE, BT_CMD_LOG } bt_cmd_type_t;
esp_err_t bus_task_command(bt_cmd_type_t type, uint32_t a, uint32_t b); /* queued, executed by bus_task; ESP_ERR_TIMEOUT if full */
```
Threading (normative):
- `bus_task`: static task, priority 10, pinned to core 1 (architecture section 2), stack 4096 (report high-water mark). Loop: take at most one command (`xQueueReceive`, timeout = `min(wait_until - now, 50 ms)`, 0 if a step is due); `poll_step()` with env = { adapter `mb_link_read` -> `ps_res_t` (`MBL_REFUSED` -> `PS_RES_ERROR`), clock = `esp_timer_get_time() / 1000` }; after each transaction copy model, stats and health into the snapshot under the mutex (copy only).
- Output never blocks `bus_task`: `T`/`H` lines (`txn_log` v1, 010) go into a ring buffer drained by a `log_out` task (priority 2); full -> drop and count `log_dropped`. `H` lines, MANUAL and SWEEP `T` lines are always emitted; other `T` lines only while logging is on (default on from power-up, so the first automatic polls are logged; `mb log off` suppresses them).
- `main.c`: `mb_link_hold_receive()`, build `bus_task_config_t` from `CONFIG_DMT_*` (001; stale thresholds 3 x period), `bus_task_start()`, then the REPL (UART or USB-Serial-JTAG per console Kconfig, as in the IDF console examples). No NVS, Wi-Fi, Matter.

Boot sequence in `bus_task`: emit `# dmt-probe v1 addr=<a> baud=<b> fw=<esp_app_get_description()->version>`; `mb_link_silence_check(boot_silence_ms)`; latch, or `mb_link_init`, drain the command queue, then `poll_sched_start(now)` unless a queued `mb poll off` was executed; emit an `H` line and a one-line hint (`silence ok, polling on: mb poll off to stop`).

Console commands (argument errors print usage and never reach the bus). The first seven are named in architecture section 2; `mb addr` and `mb log` are additions this spec needs (Q6/D19 override; probe log; user-approved 2026-11-09).
| Command | Effect |
|---|---|
| `mb status` | config from `bus_task_config_t` (not `CONFIG_*`, lint R6): port, TX/RX/DE GPIO, baud, Kconfig and current addr, gap, timeout, periods, retries, offline_after, probe period; health; boot silence activity; log state, `log_dropped`; stack HWM; `write: compiled out` |
| `mb regs` | per register: reg (3 digits), name, `*` if starred, availability, raw dec/hex, value (MLT01 as `x.y`, signed where `is_signed`), FLAGS in binary, fresh/stale, age ms, ok/fail counts, exception code, `!range` if out of range |
| `mb read <reg> [qty]` | MANUAL read, qty 1..4 (default 1); refusal text if `poll_sched_request_manual` returns -1 |
| `mb probe` | SWEEP of all 32 documented registers (exit item 4); refused when LATCHED |
| `mb poll on\|off` | `poll_sched_start` / `poll_sched_stop` (on refused with a message when LATCHED) |
| `mb silence [s]` | s = 1..600, default 60 (D17): stop polling, `mb_link_deinit`, `mb_link_silence_check(s)`; activity > 0 or error -> latch; else `mb_link_init`, polling stays off until `mb poll on` (deliberately unlike boot; user-approved 2026-11-09) |
| `mb stats` | `ps_stats_t`: counts per result and kind, error rate = (timeout + bad_frame + error) / txns in %, offline entries, latency min / median (`poll_sched_lat_median_ms`) / max (exit items 6, 7) |
| `mb addr <1-247>` | RAM override (Q6, lost at reboot; no scan): `reg_model_reset`, `poll_sched_set_addr`, new header line |
| `mb log on\|off` | all `T` lines on/off (default **on** from power-up, user decision 2026-11-09; the boot header line covers the boot-time start); `on` first emits the header line again |

## Acceptance checks
1. `cd firmware && idf.py build` passes, zero warnings in owned code; lint and ELF check pass; report `idf.py size`; no Wi-Fi/Matter components linked.
2. Code inspection checklist in the report, with file:line: hold-receive is the first statement of `app_main`; no `mb_link_init` path before a passed silence check; the single `poll_sched_start` call of the boot path comes after a passed check, a successful `mb_link_init` and the queue drain; every latch path; only `bus_task` calls `mb_link_*` (grep output); no printing from `bus_task` except via the ring buffer; mutex scope = copy only.
3. List of registered console commands with arguments (from code; `help` cannot be captured without a board).
4. On-target behaviour is verified by the user through 008.

## Test strategy
- Glue and console: target-only, no host tests (thin layer over host-tested 009/010 and the 011 end-to-end tests on the same `poll_step` seam).
- `T`/`H` line formats come from `txn_log` (host-tested in 010); no own formatting of those lines here.

## Recommended implementer model
sonnet: FreeRTOS task/queue/mutex design with a non-blocking output path, IDF console API and a safety-ordered boot sequence.

## Out of scope
- Writes, write queue (phase 3), Matter, Wi-Fi, NVS (address override is RAM only), OTA.
- Persisting logs on the device; parsing logs (011). Scheduling (project-wide).
