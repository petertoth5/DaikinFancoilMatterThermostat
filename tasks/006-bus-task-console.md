# 006 - `bus_task` poll loop, probe console (`mb ...`), `app_main`

Phase: 1. Status: done (accepted 2026-11-24: build-verifier PASS, firmware-reviewer APPROVE; committed b8ad00d) (amended 2026-11-24, user decision: 006 accepted). History: spec ready (v0.3, 2026-11-08; amended 2026-11-09: automatic polling start, `mb log` default on, queue-drain and `mb silence` behaviour confirmed; user decisions; amended 2026-11-24, user decision E2: own RTU master, no esp-modbus; `mb_link` from 014, link counters, ELF required symbols; amended 2026-11-24, user decision: 014 review: `uart` log level, fatal boot hold failure, read-before-init rule; implemented 2026-11-24, firmware-reviewer APPROVE WITH MINORS; amended 2026-11-24, user decision: 006 review: rules M1-M6 and the follow-up below, all minors fixed before commit). Depends on: 014 (was 002), 010.
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
- Console never on the bus UART: compile-time `#error` if a UART console uses `CONFIG_DMT_BUS_UART_PORT` (check the v6.0.2 console symbol names) plus a runtime check at start (architecture section 2, console rule). The runtime check runs **before** `bus_task_start` (rule M4; amended 2026-11-24, user decision: 006 review).

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
                 mb_link_counters_t link_ctr;
                 bool link_closed;       /* rule M5 (amended 2026-11-24, user decision: 006 review) */
                 bool silence_running;   /* rule M2 (architect proposal 2026-11-24, user-confirmed 2026-11-24) (amended 2026-11-24, user decision: 006 accepted) */
               } bus_snapshot_t;   /* link_ctr: amended 2026-11-24, user decision (014 counters) */
void bus_task_get_snapshot(bus_snapshot_t *out);   /* copy under mutex; console now, Matter in phase 2 */
typedef enum { BT_CMD_POLL_ON, BT_CMD_POLL_OFF, BT_CMD_READ, BT_CMD_PROBE, BT_CMD_ADDR, BT_CMD_SILENCE, BT_CMD_LOG } bt_cmd_type_t;
esp_err_t bus_task_command(bt_cmd_type_t type, uint32_t a, uint32_t b); /* queued, executed by bus_task; ESP_ERR_TIMEOUT if full */
```
Threading (normative):
- `bus_task`: static task, priority 10, pinned to core 1 (architecture section 2), stack 4096 (report high-water mark). It is the only bus-related task; there are no Modbus library tasks to coordinate with (amended 2026-11-24, user decision E2). `mb_link_read` blocks for at most TX frame time + `resp_timeout_ms` + 2 silence windows + 1 tick (014). Loop: take at most one command (`xQueueReceive`, timeout = `min(wait_until - now, 50 ms)`, 0 if a step is due); `poll_step()` with env = { adapter `mb_link_read` -> `ps_res_t` (`MBL_REFUSED` -> `PS_RES_ERROR`), clock = `esp_timer_get_time() / 1000` }; after each transaction copy model, stats, health and `mb_link_get_counters()` into the snapshot under the mutex (copy only).
- Output never blocks `bus_task`: `T`/`H` lines (`txn_log` v1, 010) go into a ring buffer drained by a `log_out` task (priority 2); full -> drop and count `log_dropped`. `H` lines, MANUAL and SWEEP `T` lines are always emitted; other `T` lines only while logging is on (default on from power-up, so the first automatic polls are logged; `mb log off` suppresses them).
- `main.c`: `mb_link_hold_receive()`, then the runtime console-UART check (rule M4; amended 2026-11-24, user decision: 006 review), then `esp_log_level_set("uart", ESP_LOG_WARN)` so IDF's "queue free spaces" INFO line from `uart_driver_install` on the silence-check path (`uart.c:2039`, IDF v6.0.2, external) does not print from `bus_task` context (amended 2026-11-24, user decision: 014 review; if the implementer finds this not possible, document the line as accepted in the report instead), build `bus_task_config_t` from `CONFIG_DMT_*` (001; stale thresholds 3 x period), `bus_task_start()`, then the REPL (UART or USB-Serial-JTAG per console Kconfig, as in the IDF console examples). No NVS, Wi-Fi, Matter.

Boot sequence in `bus_task`: emit `# dmt-probe v1 addr=<a> baud=<b> fw=<esp_app_get_description()->version>`; publish the initial snapshot (rule M2); `mb_link_silence_check(boot_silence_ms)`; publish the snapshot again right after the check (extra publish, amended 2026-11-24, user decision: 006 accepted; harmless, it keeps `silence_running` false before the drained commands run); latch, or `mb_link_init`, drain the command queue, then `poll_sched_start(now)` unless a queued `mb poll off` was executed; emit an `H` line and exactly one hint line chosen from the real state (rule M1; amended 2026-11-24, user decision: 006 review).

Console commands (argument errors print usage and never reach the bus). The first seven are named in architecture section 2; `mb addr` and `mb log` are additions this spec needs (Q6/D19 override; probe log; user-approved 2026-11-09).
| Command | Effect |
|---|---|
| `mb status` | config from `bus_task_config_t` (not `CONFIG_*`, lint R6): port, TX/RX/DE GPIO, baud, Kconfig and current addr, gap, timeout, periods, retries, offline_after, probe period; health; boot silence activity; log state, `log_dropped`; stack HWM; `write: compiled out`; link counters `gap_bytes`, `max_byte_interval_us`, then `faults since boot: N` and, only while the snapshot's `link_closed` is true, `link closed: mb silence or reboot to recover` (rule M5; amended 2026-11-24, user decision: 006 review; was: shown whenever `faults` > 0) (amended 2026-11-24, user decision; approved 2026-11-24; 014, 008); if `measured` is false: `link counters: n/a` instead of the three values (E1b fallback, amended 2026-11-24, user decision) |
| `mb regs` | per register: reg (3 digits), name, `*` if starred, availability, raw dec/hex, value (MLT01 as `x.y`, signed where `is_signed`), FLAGS in binary, fresh/stale, age ms, ok/fail counts, exception code, `!range` if out of range |
| `mb read <reg> [qty]` | MANUAL read, qty 1..4 (default 1); refusal text if `poll_sched_request_manual` returns -1 |
| `mb probe` | SWEEP of all 32 documented registers (exit item 4); refused when LATCHED |
| `mb poll on\|off` | `poll_sched_start` / `poll_sched_stop` (on refused with a message when LATCHED) |
| `mb silence [s]` | s = 1..600, default 60 (D17): cancel a pending MANUAL read, stop polling, publish the snapshot, `mb_link_deinit`, `mb_link_silence_check(s)`; activity > 0 or error -> latch; else `mb_link_init`, polling stays off until `mb poll on` (deliberately unlike boot; user-approved 2026-11-09); publish the snapshot with the result (rules M2, M3; amended 2026-11-24, user decision: 006 review) |
| `mb stats` | `ps_stats_t`: counts per result and kind, error rate = (timeout + bad_frame + error) / txns in %, offline entries, latency min / median (`poll_sched_lat_median_ms`) / max (exit items 6, 7) |
| `mb addr <1-247>` | RAM override (Q6, lost at reboot; no scan): `reg_model_reset`, `poll_sched_set_addr`, new header line |
| `mb log on\|off` | all `T` lines on/off (default **on** from power-up, user decision 2026-11-09; the boot header line covers the boot-time start); `on` first emits the header line again |

## Review rules M1-M6 (normative; amended 2026-11-24, user decision: 006 review)
- **M1 Boot hint from the real state.** After the boot path has finished (hold error, check, init, drain and start decision), `bus_task` emits exactly one hint line in **every** boot path, chosen from `is_latched()`, `link_ready` and `poll_sched_health()`, never from `auto_start_cancelled`:
  - LATCHED (hold error, activity, check/init error, or a drained `mb silence` that latched): `# latched until reboot: no transmission`
  - `link_ready` and health STOPPED: `# silence ok, polling off: mb poll on to start`
  - `link_ready` and health not STOPPED: `# silence ok, polling on: mb poll off to stop`
  "silence ok" is never printed while latched. (The current code, `bus_task.c:306-324`, decides by `auto_start_cancelled` and prints no hint on the latched paths.)
- **M2 Snapshot never zeroed or stale.** `boot_sequence` publishes the snapshot right after the header line and **before** the silence check (addr, health STOPPED, `log_on`, `log_dropped`, the initialised model with its config-derived stale thresholds, zero stats, link counters, stack HWM). `mb silence` publishes after `poll_sched_cancel_manual` + `poll_sched_stop` (before `mb_link_deinit`) and again after the result (pass, activity latch or error latch). So `mb status` / `mb stats` show valid values during the 5 s boot window and during a long `mb silence`. Before each silence check (boot and `mb silence`) `bus_task` emits `# silence check: started <s> s` (required; chosen over optional so the log shows the window). Snapshot field `silence_running` (architect proposal 2026-11-24, user-confirmed 2026-11-24): it is true in the publish before each check and false in the publish after it; while true `mb status` prints the single shared text `boot silence: <ms> ms, activity running` for any silence check (boot or `mb silence`), instead of an activity value that could be misread as a passed check. One snapshot flag cannot distinguish boot from `mb silence`, so the earlier separate `silence check: running` text for `mb silence` is dropped and the spec is aligned to the code (reviewer minor, resolved 2026-11-24). (amended 2026-11-24, user decision: 006 accepted)
- **M3 `mb silence` cancels a pending MANUAL read.** First action after the LATCHED refusal check: `poll_sched_cancel_manual(&sched)` (new 010 API, see 010 "Interfaces"), then `poll_sched_stop` (which already drops pending RETRY/SWEEP). Only a read pending at the start of `mb silence` is cancelled; commands queued while the check runs are executed after it, in order (the user typed them later; 008 tells the user to wait). No comment line for the cancel (the API is `void`).
- **M4 Console-UART runtime check before `bus_task_start`.** Order in `app_main`: `mb_link_hold_receive()` (first statement, unchanged), runtime check (UART console only: `ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT().channel == s_cfg.link.port.uart_port`), `esp_log_level_set("uart", ESP_LOG_WARN)`, `bus_task_start`, REPL. On failure: one `ESP_LOGE`, then `return` from `app_main`: **`bus_task` is not started and no REPL is created**, so the hold from the first statement stays (DE low, no driver on the bus UART) until reboot. Chosen as the safest option: "started latched" would still create a console on the bus UART, which is exactly the forbidden state. The check in `create_repl` (`main.c:53-55`) may stay as a second backstop or be removed; the one before `bus_task_start` is the binding one. With the USB-Serial-JTAG console the check is trivially passed.
- **M5 Faults since boot vs current link state.** `mb_link_counters_t.faults` counts port faults since boot (014); it does not say whether the link is closed now. `bus_task` keeps `faults_seen` (last observed value) and `link_closed` (bool, in the snapshot). After every `poll_step` that performed a transaction it reads `mb_link_get_counters()` (the copy it already makes for the snapshot); if `measured` and `faults > faults_seen`, it sets `link_closed = true` and, on the false -> true transition only, emits exactly one comment line via the ring buffer: `# mb_link: port fault, link closed (mb silence or reboot to recover)`; then `faults_seen = faults`. A successful `mb_link_init` (boot or `mb silence`) clears `link_closed`. `link_ready` is not changed by a fault: `poll_step` keeps running, `mb_link_read` returns `MBL_ERROR` without bus access and `T`/`H` lines continue per the existing rules (health goes DEGRADED/OFFLINE). This uses only the documented `mb_link.h` contract (counter + "a port fault leaves the link not initialised until `mb_link_init`"), so the seam rule holds. `mb status`: `faults since boot: N` always (when `measured`); `link closed: mb silence or reboot to recover` only while `link_closed`; `measured` false -> `link counters: n/a` and neither line.
- **M6 `log_out` priority comment.** `LOG_OUT_PRIORITY` stays 2; the comment in `log_out.c` must say it **equals** the default REPL task priority (`ESP_CONSOLE_REPL_CONFIG_DEFAULT`, 2 in IDF v6.0.2, external; verify in the installed header) and is far below `bus_task` (10). No value change.

## Acceptance checks
1. `cd firmware && idf.py build` passes, zero warnings in owned code; lint and ELF check pass; report `idf.py size`; no Wi-Fi/Matter components linked. The ELF summary shows `required: mb_link_read,mb_rtu_build_fc03 present`; guard proof: temporarily require `dmt_no_such_symbol` in addition -> build fails (revert) (amended 2026-11-24, user decision).
2. Code inspection checklist in the report, with file:line: hold-receive is the first statement of `app_main`; its error path latches without silence check or init and reports via the console; `esp_log_level_set("uart", ESP_LOG_WARN)` before `bus_task_start` (or the accepted-line note) (both amended 2026-11-24, user decision: 014 review); no `mb_link_read` path before a clean silence check and a successful `mb_link_init`; no `mb_link_init` path before a passed silence check; the single `poll_sched_start` call of the boot path comes after a passed check, a successful `mb_link_init` and the queue drain; every latch path; only `bus_task` calls `mb_link_*` (grep output); no printing from `bus_task` except via the ring buffer; mutex scope = copy only; `mb_link_init` is called only in the boot path and in `mb silence` after a clean check (no re-init after a port fault) (amended 2026-11-24, user decision); seam rule: `grep -rnE 'mb_rtu\.h|rs485_port\.h|rs485_port_|MB_RTU_|RS485_' firmware/components/bus_task firmware/main` is empty (amended 2026-11-24, user decision).
3. List of registered console commands with arguments (from code; `help` cannot be captured without a board).
4. On-target behaviour is verified by the user through 008.
5. Review minors (amended 2026-11-24, user decision: 006 review), code inspection with file:line in the report:
   a. M1: the boot hint is selected from `is_latched()` / `link_ready` / health after the drain; each boot path (hold error, activity, check error, init error, drained `mb silence` latch, drained `mb poll off`, normal) emits exactly one hint and none prints "silence ok" while latched (table path -> line in the report).
   b. M2: `publish_snapshot` is called in `boot_sequence` before `mb_link_silence_check`, and in `command_silence` after `poll_sched_stop` (before `mb_link_deinit`) and after the result; `# silence check: started <s> s` precedes every check.
   c. M3: `command_silence` calls `poll_sched_cancel_manual` before `poll_sched_stop`, after the LATCHED refusal.
   d. M4: in `app_main` the runtime console-UART check precedes `esp_log_level_set` and `bus_task_start`; its failure path returns without `bus_task_start` and without a REPL; `mb_link_hold_receive` is still the first statement.
   e. M5: `link_closed` set only on an observed `faults` increase after a transaction, cleared only by a successful `mb_link_init`; the port-fault comment line is emitted only on the false -> true transition; `mb status` prints `faults since boot` and the `link closed` line per M5.
   f. M6: `log_out.c` priority comment corrected, value 2 unchanged.
   g. 010 host tests pass with the new `poll_sched_cancel_manual` test; acceptance checks 1 and 2 still pass (build, lint, ELF, seam grep).

## Test strategy
- Glue and console: target-only, no host tests (thin layer over host-tested 009/010 and the 011 end-to-end tests on the same `poll_step` seam).
- `T`/`H` line formats come from `txn_log` (host-tested in 010); no own formatting of those lines here.

## Recommended implementer model
sonnet: FreeRTOS task/queue/mutex design with a non-blocking output path, IDF console API and a safety-ordered boot sequence.

## Review follow-up (amended 2026-11-24, user decision: 006 review)
Scope: rules M1-M6 and acceptance 5, one `firmware-dev` pass, then `build-verifier` (incl. host tests) and `firmware-reviewer`; commit after both.
Files owned by the follow-up:
- `firmware/components/bus_task/include/bus_task.h` (`bus_snapshot_t`: `link_closed`, `silence_running` if confirmed)
- `firmware/components/bus_task/src/bus_task.c` (M1, M2, M3, M5)
- `firmware/components/bus_task/src/mb_console.c` (`mb status` lines for M2, M5)
- `firmware/components/bus_task/src/log_out.c` (M6 comment only)
- `firmware/main/main.c` (M4)
- Ownership grant (010 change for M3, only this change): `firmware/components/poll_sched/include/poll_sched.h`, `firmware/components/poll_sched/src/poll_sched.c`, `firmware/components/poll_sched/test_host/test_poll_sched.c` (new API `poll_sched_cancel_manual` and its host test, per 010 "Amendment 2026-11-24 (006 review)").
Model: sonnet. Each change is small, but M1 and M5 need the boot paths and the fault-transition logic traced correctly, and the change spans a pure component with a host test plus target glue; haiku is likely to miss a boot path or put the check in the wrong place relative to `bus_task_start`.

## Out of scope
- Writes, write queue (phase 3), Matter, Wi-Fi, NVS (address override is RAM only), OTA.
- Persisting logs on the device; parsing logs (011). Scheduling (project-wide).
