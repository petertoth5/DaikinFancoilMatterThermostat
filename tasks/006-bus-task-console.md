# 006 - `bus_task` poll loop, probe console (`mb ...`), `app_main`

Phase: 1. Status: spec ready (v0.3, 2026-11-08; amended 2026-11-09: automatic polling start, `mb log` default on, queue-drain and `mb silence` behaviour confirmed; user decisions; amended 2026-11-24, user decision E2: own RTU master, no esp-modbus; `mb_link` from 014, link counters, ELF required symbols; amended 2026-11-24, user decision: 014 review: `uart` log level, fatal boot hold failure, read-before-init rule). Depends on: 014 (was 002), 010.
Review: **bus-touching**. `firmware-reviewer` reviews this task independently.

## Goal
The phase-1 probe image: one task, the single owner of `mb_link`, runs the boot silence check, then drives `poll_step` (010) over `mb_link_read` (014; was 002, amended 2026-11-24, user decision), publishes snapshots and serves the `mb` console commands the on-site checklist (008) uses. `app_main` holds the transceiver in receive first, then starts the task and the REPL.

## Bus-safety rules (binding; architecture section 3)
- First statement of `app_main`: `mb_link_hold_receive()`.
- Failed boot hold is fatal (amended 2026-11-24, user decision: 014 review): if that first `mb_link_hold_receive()` returns an error (e.g. invalid cfg), there is no silence check, no `mb_link_init` and no polling until reboot. `app_main` still starts `bus_task` and the REPL and hands the error over (mechanism is the implementer's choice, e.g. an `esp_err_t hold_err` field in `bus_task_config_t`); `bus_task` then calls `poll_sched_latch()`, skips the silence check and emits `# mb_link: hold_receive failed: <esp_err_to_name>` via the ring buffer (same comment-line format as the other `mb_link` errors); `mb status` shows it; `mb silence` is refused with a message in this state (reboot only).
- Never call `mb_link_read` before a clean silence check **and** a successful `mb_link_init` (restated, amended 2026-11-24, user decision: 014 review; `mb_link_read` returns `MBL_ERROR` when not initialised, but `bus_task` must not rely on that).
- **Nothing is sent before the boot silence check passed** (`CONFIG_DMT_BUS_BOOT_SILENCE_MS`, D17). Activity > 0, or any error of the check or of `mb_link_init`, -> `poll_sched_latch()`; `mb_link_init` is then never called until reboot.
- After a passed check and a successful `mb_link_init`, **polling starts automatically** (`poll_sched_start`, user decision 2026-11-09, architecture section 3 "Fallbacks"): the first frame goes out about `boot_silence_ms` (5 s) after boot. This is the only automatic start path. `mb poll off` / `mb poll on` stop and resume it; `mb probe` / `mb read` also work while stopped (010 rules 9, 10).
- Commands queued while the silence check runs are all executed before the first `poll_step`, so a `mb poll off` typed during the 5 s window prevents the first automatic frame (abort path used by 008; user-approved 2026-11-09).
- Every request goes through `poll_sched`/`poll_step` (gap, retry, health); console reads are MANUAL/SWEEP requests, never direct `mb_link_read` calls.
- Only `bus_task` calls `mb_link_*` (after `app_main`'s first hold call).
- Seam rule (014, amended 2026-11-24, user decision): `bus_task` and `main` use only `mb_link.h` of the link layer; no `mb_rtu.h`, `rs485_port.h`, `MB_RTU_*`, `RS485_*` or `rs485_port_*` anywhere in owned files, and no logic that depends on how `mb_link` frames, times or classifies (so an E1b revert, 014 "Fallback E1b", does not touch this task's code).
- No write path exists (lint R7, ELF check); `mb status` prints `write: compiled out`.
- Port fault (amended 2026-11-24, user decision E2; 014): after a short write, TX-done timeout or read error `mb_link` closes the port and holds receive; every later `mb_link_read` returns `MBL_ERROR` without bus access (-> `PS_RES_ERROR`, health goes DEGRADED/OFFLINE). `bus_task` never re-inits on its own; recovery only via `mb silence` (deinit, clean check, init) or reboot.
- `mb_link` and `rs485_port` do not log (014). `bus_task` reports `mb_link_init` / silence-check errors as one comment line `# mb_link: <call> failed: <esp_err_to_name>` via the ring buffer (comment lines are ignored by `txn_log_parse`), and never logs per failed read beyond the `T`/`H` lines (amended 2026-11-24, user decision: no log flood).
- Console never on the bus UART: compile-time `#error` if a UART console uses `CONFIG_DMT_BUS_UART_PORT` (check the v6.0.2 console symbol names) plus a runtime check at start (architecture section 2, console rule).

## Files / directories owned
- `firmware/components/bus_task/` (new): `CMakeLists.txt` (`REQUIRES mb_link poll_sched reg_model daikin_regs console esp_timer freertos esp_app_format`, `-Wall -Wextra -Werror`), `include/bus_task.h`, `src/bus_task.c`, `src/mb_console.c`, `src/log_out.c`
- `firmware/main/main.c`, `firmware/main/CMakeLists.txt` (`REQUIRES bus_task`)
- `firmware/CMakeLists.txt`: only the line `set(DMT_ELF_REQUIRED_SYMBOLS ...)`, set to `mb_link_read,mb_rtu_build_fc03` (ownership grant, amended 2026-11-24, user decision; the hook itself is 013's)

## Wiki pages relied on
- `concepts/firmware-architecture`, sections "1. Phases and exit criteria", "2. Components and threading" (component table: console commands; Threading), "3. Bus-safety rules" (Fallbacks, D16, D17, D21), "6. Decisions" (D19 address)
- `concepts/register-map`, `concepts/flag-registers` (via `daikin_regs`: names, scaling; flag registers printed in binary)

## Interfaces
```c
typedef struct { mb_link_config_t link; ps_config_t sched; uint32_t boot_silence_ms; } bus_task_config_t;
esp_err_t bus_task_start(const bus_task_config_t *cfg);
typedef struct { reg_model_t model; ps_stats_t stats; ps_health_t health; uint8_t addr;
                 uint32_t boot_activity; bool log_on; uint32_t log_dropped; uint32_t stack_hwm;
                 mb_link_counters_t link_ctr; } bus_snapshot_t;   /* link_ctr: amended 2026-11-24, user decision (014 counters) */
void bus_task_get_snapshot(bus_snapshot_t *out);   /* copy under mutex; console now, Matter in phase 2 */
typedef enum { BT_CMD_POLL_ON, BT_CMD_POLL_OFF, BT_CMD_READ, BT_CMD_PROBE, BT_CMD_ADDR, BT_CMD_SILENCE, BT_CMD_LOG } bt_cmd_type_t;
esp_err_t bus_task_command(bt_cmd_type_t type, uint32_t a, uint32_t b); /* queued, executed by bus_task; ESP_ERR_TIMEOUT if full */
```
Threading (normative):
- `bus_task`: static task, priority 10, pinned to core 1 (architecture section 2), stack 4096 (report high-water mark). It is the only bus-related task; there are no Modbus library tasks to coordinate with (amended 2026-11-24, user decision E2). `mb_link_read` blocks for at most TX frame time + `resp_timeout_ms` + 2 silence windows + 1 tick (014). Loop: take at most one command (`xQueueReceive`, timeout = `min(wait_until - now, 50 ms)`, 0 if a step is due); `poll_step()` with env = { adapter `mb_link_read` -> `ps_res_t` (`MBL_REFUSED` -> `PS_RES_ERROR`), clock = `esp_timer_get_time() / 1000` }; after each transaction copy model, stats, health and `mb_link_get_counters()` into the snapshot under the mutex (copy only).
- Output never blocks `bus_task`: `T`/`H` lines (`txn_log` v1, 010) go into a ring buffer drained by a `log_out` task (priority 2); full -> drop and count `log_dropped`. `H` lines, MANUAL and SWEEP `T` lines are always emitted; other `T` lines only while logging is on (default on from power-up, so the first automatic polls are logged; `mb log off` suppresses them).
- `main.c`: `mb_link_hold_receive()`, then `esp_log_level_set("uart", ESP_LOG_WARN)` so IDF's "queue free spaces" INFO line from `uart_driver_install` on the silence-check path (`uart.c:2039`, IDF v6.0.2, external) does not print from `bus_task` context (amended 2026-11-24, user decision: 014 review; if the implementer finds this not possible, document the line as accepted in the report instead), build `bus_task_config_t` from `CONFIG_DMT_*` (001; stale thresholds 3 x period), `bus_task_start()`, then the REPL (UART or USB-Serial-JTAG per console Kconfig, as in the IDF console examples). No NVS, Wi-Fi, Matter.

Boot sequence in `bus_task`: emit `# dmt-probe v1 addr=<a> baud=<b> fw=<esp_app_get_description()->version>`; `mb_link_silence_check(boot_silence_ms)`; latch, or `mb_link_init`, drain the command queue, then `poll_sched_start(now)` unless a queued `mb poll off` was executed; emit an `H` line and a one-line hint (`silence ok, polling on: mb poll off to stop`).

Console commands (argument errors print usage and never reach the bus). The first seven are named in architecture section 2; `mb addr` and `mb log` are additions this spec needs (Q6/D19 override; probe log; user-approved 2026-11-09).
| Command | Effect |
|---|---|
| `mb status` | config from `bus_task_config_t` (not `CONFIG_*`, lint R6): port, TX/RX/DE GPIO, baud, Kconfig and current addr, gap, timeout, periods, retries, offline_after, probe period; health; boot silence activity; log state, `log_dropped`; stack HWM; `write: compiled out`; link counters `gap_bytes`, `faults` (with `link closed: mb silence or reboot to recover` when > 0), `max_byte_interval_us` (amended 2026-11-24, user decision; approved 2026-11-24; 014, 008); if `measured` is false: `link counters: n/a` instead of the three values (E1b fallback, amended 2026-11-24, user decision) |
| `mb regs` | per register: reg (3 digits), name, `*` if starred, availability, raw dec/hex, value (MLT01 as `x.y`, signed where `is_signed`), FLAGS in binary, fresh/stale, age ms, ok/fail counts, exception code, `!range` if out of range |
| `mb read <reg> [qty]` | MANUAL read, qty 1..4 (default 1); refusal text if `poll_sched_request_manual` returns -1 |
| `mb probe` | SWEEP of all 32 documented registers (exit item 4); refused when LATCHED |
| `mb poll on\|off` | `poll_sched_start` / `poll_sched_stop` (on refused with a message when LATCHED) |
| `mb silence [s]` | s = 1..600, default 60 (D17): stop polling, `mb_link_deinit`, `mb_link_silence_check(s)`; activity > 0 or error -> latch; else `mb_link_init`, polling stays off until `mb poll on` (deliberately unlike boot; user-approved 2026-11-09) |
| `mb stats` | `ps_stats_t`: counts per result and kind, error rate = (timeout + bad_frame + error) / txns in %, offline entries, latency min / median (`poll_sched_lat_median_ms`) / max (exit items 6, 7) |
| `mb addr <1-247>` | RAM override (Q6, lost at reboot; no scan): `reg_model_reset`, `poll_sched_set_addr`, new header line |
| `mb log on\|off` | all `T` lines on/off (default **on** from power-up, user decision 2026-11-09; the boot header line covers the boot-time start); `on` first emits the header line again |

## Acceptance checks
1. `cd firmware && idf.py build` passes, zero warnings in owned code; lint and ELF check pass; report `idf.py size`; no Wi-Fi/Matter components linked. The ELF summary shows `required: mb_link_read,mb_rtu_build_fc03 present`; guard proof: temporarily require `dmt_no_such_symbol` in addition -> build fails (revert) (amended 2026-11-24, user decision).
2. Code inspection checklist in the report, with file:line: hold-receive is the first statement of `app_main`; its error path latches without silence check or init and reports via the console; `esp_log_level_set("uart", ESP_LOG_WARN)` before `bus_task_start` (or the accepted-line note) (both amended 2026-11-24, user decision: 014 review); no `mb_link_read` path before a clean silence check and a successful `mb_link_init`; no `mb_link_init` path before a passed silence check; the single `poll_sched_start` call of the boot path comes after a passed check, a successful `mb_link_init` and the queue drain; every latch path; only `bus_task` calls `mb_link_*` (grep output); no printing from `bus_task` except via the ring buffer; mutex scope = copy only; `mb_link_init` is called only in the boot path and in `mb silence` after a clean check (no re-init after a port fault) (amended 2026-11-24, user decision); seam rule: `grep -rnE 'mb_rtu\.h|rs485_port\.h|rs485_port_|MB_RTU_|RS485_' firmware/components/bus_task firmware/main` is empty (amended 2026-11-24, user decision).
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
