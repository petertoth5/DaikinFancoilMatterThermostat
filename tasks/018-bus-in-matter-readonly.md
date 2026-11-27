# 018 Bus stack in the Matter image, read-only, shared console (phase 2b, part A)

Status: spec ready. Phase-2b plan P1-P10 approved by the user 2026-11-27 (relayed by the orchestrator; wiki-keeper records it in `concepts/firmware-architecture` section 6). Gate: P1. 2b-A is specified and built **before** the probe log. The BUS image may run on the bench **only with A/B open**. No live bus until the log is in `raw/` and captured. Bus-topology items 1, 1b, 2, 3, 4, 5 and 7 stay open ([P1] probe). Nothing here depends on them: address, timing and mapping stay at the current Kconfig defaults, and the mapping stays provisional. Depends on 006, 011, 014, 015, 016, 017. Model: **sonnet**. Why: dual-config CMake, a strict boot order and bus-safety rules. This is judgement work, not mechanical. After the task: `build-verifier`, then a **full independent `firmware-reviewer`** review, because the task touches the bus (S3). Part B is one separate commit.

## Goal
**Part A.** `firmware/matter/` gets a Kconfig source choice `SIM | BUS`:
- **SIM (default)** behaves exactly like 2a. No bus code is linked and DE is not driven.
- **BUS** links the phase-1 bus stack unchanged (`rs485_port`, `mb_rtu`, `mb_link`, `poll_sched`, `reg_model`, `bus_task`) and feeds the **unchanged 016 mapping/bridge** from `bus_task_get_snapshot`. 2b-B then only swaps the real mapping.
- Reads only. Both variants use one `dmt>` REPL, created before `bus_task`/`log_out` start.

**Part B (P4).** The same console-race fix in the phase-1 `firmware/main/main.c`.

**Recommendation: SIM stays the default.** A default build then never drives DE and never transmits. The pending 2a on-device checks (D6, D7, O7) keep working. A BUS image is always a deliberate choice (overlay file below), which matches P1.

## Files allowed to change
Part A, all under `firmware/matter/`:
- New: `main/Kconfig.projbuild`, `main/dmt_console.c/.h`, `main/bus_source.c/.h`, `sdkconfig.defaults.bus`, `tools/check_matter_bus.py`, `test/host/main/test_bus_mapping.c`.
- Edited: `CMakeLists.txt`, `main/CMakeLists.txt`, `main/app_main.cpp`, `main/sim_console.c/.h`, `sdkconfig.defaults`, `.gitignore` (`build_bus/`), `test/host/CMakeLists.txt`, `test/host/main/CMakeLists.txt`.
- Deleted: `tools/check_no_bus.py`.

Part B: `firmware/main/main.c` only.

**Not allowed:**
- `firmware/components/**`, `firmware/tools/**`, `firmware/test/**` (read-only reuse only), any other phase-1 file.
- `managed_components`, both `dependencies.lock` files (no new dependency; registry-only rule, `concepts/dev-environment`).
- The wiki.

## Wiki pages relied on
- `concepts/firmware-architecture`:
  - sec. 2: threading, console rule.
  - sec. 3: timing D16, polling D6, health D21, fallbacks D17, gate/lint D10/D20.
  - sec. 4: DE rules.
  - sec. 5: EP1, stale -> null.
  - sec. 6: T4, 006-a/b/c, U1-U6, "Phase-1 bench decisions" (console race).
  - sec. 7: bench run, console-race defect.
- `concepts/bus-topology` (open items).
- Specs 006, 011, 014, 016, 017.

## Design
1. **Kconfig.** `main/Kconfig.projbuild`:
   - Adds the choice `DMT_SOURCE` with `DMT_SOURCE_SIM` (default) and `DMT_SOURCE_BUS`.
   - Pulls in the phase-1 bus menu **unchanged** with `rsource "../../main/Kconfig.projbuild"`, so there is one source of truth for names, ranges and defaults.
   - If the IDF v6.0.2 Kconfig parser rejects `rsource`, copy the `DMT_*` options verbatim instead and say so. `build-verifier` then diffs the blocks.
   - `sdkconfig.defaults` adds `CONFIG_UART_ISR_IN_IRAM=y` (needed in both variants: `rs485_port` is compiled whenever it is in the component list) and `CONFIG_DMT_BUS_WRITE=n`.
   - `sdkconfig.defaults.bus` holds the one line `CONFIG_DMT_SOURCE_BUS=y`.
   - `main/CMakeLists.txt`: `FATAL_ERROR` if `CONFIG_DMT_BUS_WRITE`, as in phase 1.
2. **Components.**
   - `EXTRA_COMPONENT_DIRS` adds `../components/{mb_rtu,rs485_port,mb_link,poll_sched,bus_task}` in both variants.
   - `main` REQUIRES `bus_task` and `mb_link` unconditionally.
   - The variant is chosen in the sources with `#if CONFIG_DMT_SOURCE_BUS`. Unreferenced code is not linked, and the ELF check proves it for each variant.
3. **`bus_source.c/.h` (C, main, BUS only)** is the only Matter file that includes `bus_task.h` or `mb_link.h`.
   - It owns `bus_task_config_t s_cfg`, filled from Kconfig field for field as in phase-1 `main.c`.
   - It keeps the compile-time `#error` "console UART == bus UART".
   - Functions: `bus_source_hold_receive()` (only body: `s_cfg.hold_err = mb_link_hold_receive(&s_cfg.link)`), `bus_source_console_ok()` (the T4 runtime check), `bus_source_register_console()` (calls `mb_console_register`), `bus_source_start()` (calls `bus_task_start`).
   - `bus_source_snapshot(reg_model_t *out, int64_t *now_ms)` matches `bridge_snapshot_fn_t`. It calls `bus_task_get_snapshot` into a static `bus_snapshot_t` (only the Matter context calls it), then copies `.model` and `esp_timer_get_time()/1000`. That is the same clock `bus_task` feeds the model with.
   - The header has `extern "C"` guards and exposes no bus type.
4. **`dmt_console.c/.h` (neutral).** `dmt_console_create()` creates the REPL and registers `help`:
   - prompt `dmt>`, task stack 8192;
   - UART or USB-Serial-JTAG branch as in phase-1 `create_repl`.

   `dmt_console_start()` starts it. Callers register their commands in between. `esp_console_new_repl_*` appears nowhere else in the Matter tree.

   `sim_console`:
   - no longer creates a REPL;
   - `sim_console_start()` keeps mutex, seeds and the 1 s tick;
   - new `sim_console_register()` registers `sim`;
   - command texts are unchanged.

   Build contents:
   - SIM build: `help` + `sim` only.
   - BUS build: `help` + `mb` only, with the full 006 command set unchanged.
5. **`app_main` order.** BUS build:
   1. `bus_source_hold_receive()`, the **first statement**.
   2. `bus_source_console_ok()`. On failure: log, return. Nothing else starts, the hold stays.
   3. `esp_log_level_set("uart", ESP_LOG_WARN)` (006-a).
   4. NVS init, Wi-Fi init plus force STA (017), node, EP1, reset button, all unchanged.
   5. `dmt_console_create()` + `bus_source_register_console()`.
   6. `bus_source_start()`.
   7. `esp_matter::start`.
   8. `matter_bridge_start(..., bus_source_snapshot, ...)`.
   9. `dmt_console_start()`.

   Rules for this order:
   - Any failure in step 5 -> return **before** step 6. No bus traffic without the `mb poll off` fallback.
   - The 5 s silence check runs alongside the Matter start (P5).
   - Polling starts after a clean check whatever the commissioning state.

   SIM build: same order without steps 1-3 and 6. `sim_console_start()` + `sim_console_register()` replace step 5's bus registration, and the bridge gets `sim_console_snapshot`.
6. **Faults in Matter (P6).** No fault cluster and no new code. LATCHED, OFFLINE or never-started polling leaves the model not fresh:
   - 016 M1 makes LocalTemperature null after the 15 s stale window.
   - Setpoints, limits and SystemMode keep their last value (016 M2-M4).

   The host tests below prove this.
7. **Threading.** `bus_task` stays priority 10, pinned to core 1, 4 KB stack, unchanged.
   - `bus_task` and `log_out` never include esp_matter/CHIP headers and never call into `main`.
   - The bridge's getter takes the `bus_task` snapshot mutex for the copy only. `bus_task` holds it only for copies and never waits on the CHIP lock, so there is no deadlock *(inferred; inspection item)*.
   - No Matter callback queues a `bus_task_command`. 2b-A has no Matter -> bus path.
8. **Guard `tools/check_matter_bus.py`** replaces `check_no_bus.py`.
   - `--self-test`.
   - `--sources <root>`: same importlib reuse of `firmware/tools/check_bus_access.py` `scan_tree` as in 015, unchanged.
   - `--nm <nm> --mode sim|bus <elf>`. Symbol names are matched after stripping leading `_`, defined **and** undefined.

   | Mode | Forbidden prefixes | Required (defined) |
   |---|---|---|
   | `sim` | `mb_link_`, `rs485_port_`, `mb_rtu_`, `bus_task_`, `mbc_`, `mbm_`, `mbs_` (as 015) | `app_main` |
   | `bus` | `mbc_`, `mbm_`, `mbs_`, `mb_link_write`, `write_guard_` | `app_main`, `mb_link_hold_receive`, `mb_link_read`, `mb_rtu_build_fc03`, `bus_task_start` |

   In both modes, empty `nm` output or a missing `app_main` fails.

   Wiring:
   - pre-link target: `--self-test` and `--sources`;
   - POST_BUILD: `--nm ... --mode` from `CONFIG_DMT_SOURCE_BUS`, read after `project()` (`idf_build_get_config` or the `CONFIG_` variable, whichever works on IDF v6.0.2; say which);
   - any failure fails the build.

## Bus-safety rules (BUS build; all verified by inspection plus the checks below)
- S1. "+ A B -" and CP are never touched. The node is the sole master (D1). This spec changes **no** bus component. Timing, pacing, retries, health, silence check, latch, address and whitelist are exactly the phase-1 code and Kconfig defaults (D6, D16, D17, D19, D21): gap 100 ms, timeout 500 ms, 1 retry, fast 5 s, slow 60 s, OFFLINE after 5, probe 30 s, silence 5 s, addr 1.
- S2. The hold is the first statement of `app_main`, before NVS, Wi-Fi and BLE. If the hold fails, the error stays fatal (006-b). No read before a clean silence check (006-c). Bus activity latches until reboot.
- S3. FC03 only. There is no write source, `CONFIG_DMT_BUS_WRITE=y` fails CMake, and the ELF check forbids write symbols.
- S4. The console is never on the bus UART: compile-time `#error` plus the runtime check before `bus_task` starts (T4). If the console or REPL cannot be created, `bus_task` is not started.
- S5. Only `rs485_port` touches the UART and the DE/TX GPIOs. The phase-1 lint rules R4-R6 on the Matter tree enforce this. `reset_button` uses GPIO0 only.
- S6. Bench only, A/B open (P1). A BUS image on a live bus is forbidden until the probe log is captured.
- S7. **Known difference, recorded, not changed here.** The Matter image runs `CONFIG_FREERTOS_HZ=1000` (esp_matter, 015), while phase 1 runs 100 Hz. The `rs485_port` end-of-frame window is therefore about 5-6 ms instead of about 20-30 ms. That is still at or above t3.5 = 4.0 ms *(external, Modbus spec)*. `dur_ms` figures will differ from the phase-1 log. With A/B open no reply exists, so the effect cannot be seen in 2b-A. Re-tune (P10) and decide in 2b-B, before any live bus.

## Part B: phase-1 `main.c` (P4)
Same fix with no other change. The new order:
1. hold (still the first statement);
2. console-UART check;
3. `uart` log WARN;
4. `create_repl` + help + `mb_console_register`;
5. `bus_task_start`;
6. `esp_console_start_repl`.

A REPL creation or registration failure -> log, return, `bus_task` not started. Before, `bus_task` ran without a console.

## Acceptance checks
1. **SIM build.** Default build of `firmware/matter` with the capped command of 015 passes:
   - 0 warnings from our files;
   - guard `--mode sim` passes;
   - image < 75 % of `factory` (report bytes and %).
2. **BUS build.** Separate build directory:
   ```
   idf.py -B build_bus -D SDKCONFIG=build_bus/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.bus" reconfigure
   ninja -C build_bus -j2
   ```
   It passes with 0 own warnings and guard `--mode bus` passing. Report the image size.
3. **Generated sdkconfigs.**
   - SIM has `CONFIG_DMT_SOURCE_SIM=y`; BUS has `CONFIG_DMT_SOURCE_BUS=y`.
   - Both have `CONFIG_UART_ISR_IN_IRAM=y`, `CONFIG_DMT_BUS_WRITE` not set, and every `CONFIG_DMT_BUS_*` / `CONFIG_DMT_POLL_*` value equal to `firmware/sdkconfig`.
   - Report whether `rsource` worked.
4. **Guard self-test.** Every forbidden prefix in each mode is caught: defined, undefined, `_` and `__`. Near-misses are not caught. Each missing required symbol in `bus` fails. Empty output fails, a missing `app_main` fails, and `sim` mode with a bus symbol fails. Report the case count.
5. **Failure proofs.** Use a scratch copy or revert before hand-back, and report the outputs.
   - (a) BUS build with a referenced `mb_link_write_dummy()` fails at POST_BUILD.
   - (b) SIM build referencing `bus_task_start` fails.
   - (c) `CONFIG_DMT_BUS_WRITE=y` fails CMake in `firmware/matter`.
6. **Grep checks (Matter tree).**
   - `mb_rtu.h` and `rs485_port.h` are never included.
   - `bus_task.h` and `mb_link.h` are included only in `main/bus_source.c`.
   - The only `mb_link_*` call is `mb_link_hold_receive` in `bus_source.c`.
   - `bus_task_command` is never called.
   - `esp_console_new_repl_` appears only in `dmt_console.c`.
   - `mapping`, `matter_bridge` and `sim_source` include no bus header.
   - `bus_source.c` includes no esp_matter/CHIP header.
   - `tools/check_no_bus.py` is gone and `CMakeLists.txt` no longer references it.
7. **Host tests** in `firmware/matter/test/host`: all pass with 0 failures; report the count (was 62).
   - `EXTRA_COMPONENT_DIRS` adds `poll_sched`.
   - `firmware/test/host/main/fake_slave.c` is compiled by path, read-only.
   - New `test_bus_mapping.c` drives fake slave -> `poll_step` -> `reg_model` (stale 15000/180000) -> `mapping_thermostat`. The fake's 100 ms gap rule is active in every scenario. Scenarios:
     - all fast and slow registers answer -> M1-M4 outputs from bus values;
     - dead slave -> OFFLINE, LocalTemperature null at the stale boundary, setpoints/limits/SystemMode kept;
     - recovery -> ONLINE and fresh values;
     - starred 104/245/246/247 answering exception 02 -> unsupported, mapping degrades (233 = 0 -> last season + `season_auto_flag`);
     - polling never started (latched view) -> LocalTemperature null, defaults kept;
     - exception 02 on 000 via HEALTH while OFFLINE stays a failed read (U5).
8. **Phase 1 (Part B).**
   - `git diff --stat -- firmware ':!firmware/matter'` shows only `firmware/main/main.c`.
   - `idf.py build` in `firmware/` passes, including lint and the ELF check.
   - `firmware/test/host` gives 139 / 0 / 0.
9. No `dependencies.lock` change (both projects) and no `managed_components` edit.
10. **Report, do not change:** from the BUS `sdkconfig`, the core affinity and priority of the CHIP task, NimBLE host, Wi-Fi task and esp_timer task. The reviewer judges whether any of them can starve `bus_task` on core 1.

## Code-inspection checklist (`firmware-reviewer`)
- I1. In the BUS build, the hold is the first statement. The SIM build has no bus call and no bus include.
- I2. The order is that of Design 5. The REPL is created strictly before `bus_task_start`. Every failure path before step 6 returns without starting `bus_task`.
- I3. `#error` and the runtime check of the console vs the bus UART are both present (S4).
- I4. `s_cfg` matches phase-1 `main.c` field for field. No hard-coded timing or address.
- I5. `bus_source_snapshot` blocks only for the copy, is called only from the bridge (Matter context) and uses the `bus_task` clock. Bus code has no CHIP-lock path.
- I6. No Matter -> bus path. No write path anywhere. `firmware/components/**` is unchanged (`git diff` empty).
- I7. Guard modes: wired from Kconfig, not hard-coded. A SIM/BUS mismatch cannot pass silently (proofs 5a/5b).
- I8. Part B: pure reorder plus the new failure return. The hold is still first.
- I9. S7 tick difference stated in the report. Acceptance 10 core/priority table reviewed.

## On-device checks (user; bench, MAX485-M per D23 or bare board, **A/B open**)
- E1. BUS image:
  - `# dmt-probe v1 ...` and `# silence check: started 5 s` appear on **every** boot without `mb log on` (5 boots);
  - prompt `dmt>`;
  - `help` lists `mb`, not `sim`.
- E2. `# silence ok, polling on`, then `H ... ONLINE` -> `DEGRADED` -> `OFFLINE` (TIMEOUT), while the node rejoins Wi-Fi and its fabrics. HA shows LocalTemperature unknown, setpoint 2000 (or the last NVS value until the first cycle, 016), Heat. Writes are still rejected.
- E3. `mb status`:
  - `write: compiled out`, faults 0, `gap_bytes` 0;
  - report the stack high-water (phase-1 bench: 2768 B).
  - `mb poll off` / `mb poll on` / `mb silence 5` work.
- E4. DE (multimeter vs GND): about 0 V idle and during OFFLINE probes. Near 3.3 V -> stop (F3).
- E5. A boot without fabrics (optional, only together with D6): polling still starts (P5).
- E6. SIM image: 016 O1-O6 pass at the `dmt>` prompt, and `help` lists `sim`, not `mb`.
- E7. Phase-1 image (Part B): the header and first silence line are visible on 5 boots in a row.

## Test strategy
- Pure logic is host-tested: the guard script (self-test, both modes) and the bus -> mapping chain (acceptance 7).
- The glue (`bus_source`, `dmt_console`, `app_main` order) is IDF/esp_matter-bound. It is covered by inspection (I1-I9), both builds, the ELF proofs and E1-E7.
- `mb_link` stays untested on the host (D12).

## Out of scope
- Real mapping and 231/008, 233, STAT decisions (2b-B).
- Fan EP2 (P7, 2b-B).
- DE re-check via MCPWM capture (P8, 2b-C).
- Timing and address re-tune (P10).
- Any change to bus components or phase-1 Kconfig.
- Writes and `write_guard`.
- Persisting bus settings.
- MAX3485 docs/BOM.
- Scheduling, OTA, SmartThings.

## Escalate (stop, report)
- A bus component or phase-1 file other than `main.c` needs a change.
- Both `rsource` and the verbatim copy are impossible.
- The guard mode cannot be read from Kconfig at POST_BUILD.
- esp_matter needs its own console/REPL on UART0.
- An image is > 75 % of `factory`.
- A third-party task is pinned to core 1 at a priority > 10 and the reviewer judges it a starvation risk.
- Any host scenario shows a request < 100 ms after the previous one.
