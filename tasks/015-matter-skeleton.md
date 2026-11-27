# 015 Matter skeleton (phase 2a, part A)

Status: done (committed 9dbace6; reviewed with 016). Plan approved by the user 2026-11-24. Model: **sonnet** (new framework integration, build-system judgement; not mechanical). After the task: `build-verifier`; the single feature review comes after 016 (S3).

## Goal
A separate, flashable IDF project `firmware/matter/` with esp_matter, BLE + Wi-Fi commissioning and a Thermostat endpoint EP1 holding **static** values, plus a post-build guard proving that no bus code is linked. The phase-1 project stays byte-for-byte unchanged.

## Step 0 - stop condition (do first, before any other file)
Create only `CMakeLists.txt`, `main/idf_component.yml` (`espressif/esp_matter: "^1.6.1"`), a minimal `main/app_main.c`; run `idf.py set-target esp32s3 && idf.py build` on ESP-IDF v6.0.2. Pass = all of:
1. `dependencies.lock`: every entry has source type `service` (components.espressif.com) or `idf`. No `git`, `local` or URL sources.
2. Nothing fetched outside the component manager during configure/build: no `git clone` / `git submodule`, no CMake `FetchContent` / `ExternalProject` download, no `curl`/`wget`, no `pip install`, no extra host tool (e.g. `gn`, ZAP) that the IDF v6.0.2 installer did not install. Check: grep the full configure/build log and `managed_components/**/CMakeLists.txt`, `*.cmake` for these; then `rm -rf build` and rebuild with the network off if possible (else state "not tested offline").
3. Build completes.

Any violation -> **stop, report the exact item and log lines to the user, do not work around** (no manual installs, no copying esp-matter repo/example code, no alternative component).

Build RAM: WSL2 has ~7 GB. Cap parallel jobs at 2: check which works on this IDF (an `idf.py` jobs option, or `idf.py reconfigure` then `ninja -C build -j2`; `CMAKE_BUILD_PARALLEL_LEVEL` may be ignored when ninja is called directly) and state the command used. If the compiler is killed, retry with 1 job; if that fails too, escalate. Do not change WSL settings. "build" below = this capped build.

## Files owned (all new)
`firmware/matter/`: `CMakeLists.txt`, `sdkconfig.defaults`, `partitions.csv`, `dependencies.lock`, `.gitignore` (build/, managed_components/, sdkconfig, sdkconfig.old), `main/` (`CMakeLists.txt`, `idf_component.yml`, `app_main.c`, `thermostat_ep.c/.h`, `reset_button.c/.h`), `tools/check_no_bus.py`. Nothing outside `firmware/matter/`.

## Wiki pages relied on
`concepts/firmware-architecture` sec. 1 (phase 2a), 2 (threading), 3 "NVS wear", 5 (EP1, Commissioning D11), 6 (Working mode); `concepts/dev-environment` (registry-only rule, WSL2 RAM); `entities/esp32-s3-wroom-1` (N16R8).

## Build configuration
- `CMakeLists.txt`: `EXTRA_COMPONENT_DIRS` lists **only** `../components/daikin_regs` and `../components/reg_model` (016 adds `components/` of this project). No other phase-1 component, no bus code, no `CONFIG_DMT_*` bus options.
- `sdkconfig.defaults`: esp32s3, 16 MB flash *(inferred from N16R8)*, octal PSRAM, BLE on (esp_matter default stack), custom partition table, console on the same channel as the phase-1 image (copy the setting, do not reference the phase-1 file), test VID 0xFFF1 / PID 0x8000, esp_matter built-in test DAC provider. Exact Kconfig names from the esp_matter / CHIP sources *(external)*.
- `partitions.csv` (recommended, adjust if esp_matter needs more): `nvs` 48 KB (0xC000), `phy_init` 4 KB, single `factory` app 4 MB; add `fctry` / `nvs_keys` / `esp_secure_cert` only if the chosen DAC/config requires them (say which and why). No OTA partitions (out of scope).
- Warnings: our sources (`firmware/matter/main`, later `firmware/matter/components`) compile with `-Wall -Wextra -Werror` via `target_compile_options` on our component libs. Third-party (managed_components, IDF) warnings are **not** suppressed globally and not fixed: count them and list file + warning class in the report.

## Behaviour
1. `app_main`: NVS init, create node, EP1 Thermostat (device type 0x0301 *(external)*), start esp_matter. No GPIO 4/5/6 configuration, no UART driver, DE not driven.
2. EP1: Thermostat cluster with features Heat + Cool, **no Auto**; ControlSequenceOfOperation 4 CoolingAndHeating *(external)*; static values: LocalTemperature null, both setpoints 2000, limits 500/4000 (Min/Max Heat/Cool and AbsMin/AbsMax), SystemMode Heat (4) *(inferred placeholder; 016 replaces)*.
3. Mirrored attributes (LocalTemperature, both setpoints, the four setpoint limits, SystemMode) without `NONVOLATILE`. If esp_matter creates them NONVOLATILE and offers no clean way to clear the flag -> escalate with options, do not patch managed_components.
4. External writes to the Thermostat cluster on EP1 are rejected in the `PRE_UPDATE` callback (non-OK return); values unchanged. Own updates (016) are let through by a flag set only around our own `attribute::update` call in the Matter context.
5. Not commissioned at boot -> QR payload and manual pairing code printed on the console. Commissioned -> rejoins Wi-Fi/fabric after reboot.
6. BOOT button (GPIO0) held >= 5 s *(inferred hold time)* -> esp_matter factory reset. Own minimal GPIO poll (e.g. 50 ms task); no new dependency, no copied example `app_reset` code.

## Bus guard `tools/check_no_bus.py`
- `--self-test`: synthetic `nm` text; every forbidden prefix caught (also with leading `_`), near-misses (e.g. `xmb_link_a`, `mb_linkx`) not caught; empty `nm` output and missing `app_main` fail.
- `--nm <nm> <elf>`: fails if any defined or undefined symbol, after stripping leading `_`, starts with `mb_link_`, `rs485_port_`, `mb_rtu_`, `bus_task_`, `mbc_`, `mbm_`, `mbs_`. Limit: C++-mangled names not matched (bus code is C).
- `--sources <matter root>`: loads `firmware/tools/check_bus_access.py` read-only via `importlib` and calls its `scan_tree(root)` on the Matter project (it scans `main/` and `components/`, skips `managed_components`). Verified by the architect: that script has no root argument and its own scan (root `firmware/`) never sees `firmware/matter/`, so this import is the only unmodified way. Known limits: depends on an internal function; reused components are covered by the phase-1 lint, not here. If it flags legitimate Matter code -> escalate, do not edit the phase-1 script.
- Wiring in `CMakeLists.txt`: pre-link target runs `--self-test` and `--sources`; POST_BUILD on the ELF runs `--nm ${CMAKE_NM}`. Any failure fails the build.

## Acceptance checks
1. Step 0 passed and reported (lock entries, log grep result, offline rebuild yes/no).
2. Clean build of `firmware/matter`: 0 warnings from our files; third-party warnings listed.
3. Image size: `idf.py size` app < 75 % of `factory` (report bytes and %).
4. Guard: self-test and both scans pass. Proof of failure: in a scratch copy (or reverted before handing back) add a referenced function `mb_link_dummy()` called from `app_main` -> build fails at the ELF check; report the output.
5. Phase-1 unchanged: `git status --porcelain firmware | grep -v '^?? firmware/matter/'` empty; in `firmware/`: `idf.py build` passes incl. lint + ELF check; host tests in `firmware/test/host` still 139 / 0 failures / 0 ignored.
6. Grep: no `#include` of `mb_link.h`, `rs485_port.h`, `mb_rtu.h`, `bus_task.h` under `firmware/matter/`.

## On-device checks (for the user; agents cannot run them without a board)
D1 QR/manual code on the console. D2 commission with chip-tool over BLE (`pairing ble-wifi`); if BLE does not work under WSL, use a native host, or commission with HA/Apple first, open a commissioning window and use `chip-tool pairing onnetwork`/`code`. D3 read all EP1 attributes (values as in Behaviour 2). D4 write OccupiedHeatingSetpoint and SystemMode -> error status, values unchanged. D5 reboot -> rejoins without re-commissioning. D6 BOOT >= 5 s -> factory reset, QR code shown again. D7 Home Assistant (Core in Docker + separate python-matter-server container) and Apple Home commission and show a thermostat.

## Out of scope
sim/mapping/bridge loop (016), Fan endpoint, any bus code, writes, OTA, scheduling, SmartThings, production DAC, changes outside `firmware/matter/`.

## Escalate (stop, report)
Step 0 violation; NONVOLATILE not clearable (Behaviour 3); `-j1` build fails for RAM; guard source scan false positives; image > 75 % of a 4 MB factory partition; any need to touch phase-1 files.
