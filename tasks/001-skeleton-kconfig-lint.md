# 001 - Firmware skeleton, Kconfig, bus-access lint, host-test runner

Phase: 1 (on-site read probe). Status: spec ready (v0.3, 2026-11-08). Depends on: nothing.
Review: `firmware-reviewer` reviews this task independently (the lint is a bus-safety control).

## Goal
Turn the existing `firmware/` skeleton into the phase-1 layout every later task builds on:
1. `firmware/components/` for project components.
2. Project Kconfig menu with the v0.3 bus, polling and safety parameters, incl. the write gate `CONFIG_DMT_BUS_WRITE` (default n, build fails if set in phase 1).
3. `sdkconfig.defaults`.
4. The **bus-access lint** `tools/check_bus_access.py` with `--self-test`, both run on every `idf.py build`; a violation or a failing self-test fails the build.
5. `-Wall -Wextra -Werror` for our own code.
6. A host-test project (IDF `linux` target, Unity) with one smoke test.

The bus components (`mb_link`, `rs485_port`, ...) do not exist yet. The lint rules below are implemented and self-tested now, against virtual file paths, so later tasks are checked from their first line.

## Files / directories owned
- `firmware/CMakeLists.txt` (lint hook; keep project name `daikin_matter_thermostat`)
- `firmware/sdkconfig.defaults` (new)
- `firmware/main/Kconfig.projbuild` (new)
- `firmware/main/CMakeLists.txt` (warnings, write-gate guard)
- `firmware/components/README.md` (new: per-component rules: `-Wall -Wextra -Werror`, host tests in `test_host/test_*.c`)
- `firmware/tools/check_bus_access.py` (new)
- `firmware/test/host/` (new: `CMakeLists.txt`, `sdkconfig.defaults`, `main/CMakeLists.txt`, `main/test_main.c`, `main/test_smoke.c`)
- `firmware/README.md` (new: build, lint, host-test commands)

Do not change `main/main.c` behaviour (owned by 006). No new dependencies.

## Wiki pages relied on
- `concepts/firmware-architecture`, sections "2. Components and threading", "3. Bus-safety rules" (Timing D16, Read errors and health D21, Compile-time gate and bus-access lint D10/D20), "4. Hardware wiring" (GPIO), "6. Decisions" (D19 address range conflict), "7. Task impact"
- `concepts/dev-environment` (IDF v6.0.2, registry-only rule, `export.sh`)
- `concepts/modbus-rs485-link` (9600 8N1), `concepts/device-addressing` (address 1-255, default 1)

## Interfaces

### Kconfig (menu "Daikin bus (phase 1 read probe)")
| Symbol | Type | Default | Range | Note (put the source in the help text) |
|---|---|---|---|---|
| `CONFIG_DMT_BUS_UART_PORT` | int | 1 | 1-2 | UART0 stays the console |
| `CONFIG_DMT_BUS_TX_GPIO` | int | 5 | 0-48 | placeholder, confirmed in 008 |
| `CONFIG_DMT_BUS_RX_GPIO` | int | 4 | 0-48 | placeholder, confirmed in 008 |
| `CONFIG_DMT_BUS_DE_GPIO` | int | 6 | 0-48 | UART RTS signal on this GPIO drives MAX485 DE + /RE (tied), 10 kOhm pull-down (Q1, D8); placeholder |
| `CONFIG_DMT_BUS_BAUD` | int | 9600 | 1200-115200 | wiki `modbus-rs485-link`; changeable only because probe item 1 is unverified |
| `CONFIG_DMT_BUS_SLAVE_ADDR` | int | 1 | 1-247 | wiki reg 200 default 1; console RAM override (Q6); upper bound see note |
| `CONFIG_DMT_BUS_GAP_MS` | int | 100 | 50-1000 | min idle between our transactions (Q3) |
| `CONFIG_DMT_BUS_RESP_TIMEOUT_MS` | int | 500 | 200-2000 | response timeout (Q3) |
| `CONFIG_DMT_POLL_FAST_S` | int | 5 | 2-60 | fast group period (Q3) |
| `CONFIG_DMT_POLL_SLOW_S` | int | 60 | 10-3600 | slow group period (Q3) |
| `CONFIG_DMT_BUS_READ_RETRIES` | int | 1 | 0-1 | read retries per transaction (Q3, cap) |
| `CONFIG_DMT_BUS_OFFLINE_AFTER` | int | 5 | 2-20 | consecutive failures -> OFFLINE (Q3) |
| `CONFIG_DMT_BUS_OFFLINE_PROBE_S` | int | 30 | 10-600 | OFFLINE probe period, register 000 (Q3) |
| `CONFIG_DMT_BUS_BOOT_SILENCE_MS` | int | 5000 | 1000-60000 | boot silence check (Q4) |
| `CONFIG_DMT_BUS_WRITE` | bool | n | - | phase 3 write gate; help: "Not implemented before phase 3. Selecting it fails the build." |

8N1 is fixed, not configurable. The ranges are safety caps, not tuning hints; GPIO defaults 5/4/6 are placeholders (both user-approved 2026-11-09). Address range 1-247: D19 / D21 (Modbus spec, external); the manual's 1-255 is a recorded conflict (architecture section 6). Do not widen.

`firmware/main/CMakeLists.txt`: `if(CONFIG_DMT_BUS_WRITE) message(FATAL_ERROR "CONFIG_DMT_BUS_WRITE: phase 3 is not specified yet (v0.3)") endif()`.

### sdkconfig.defaults
`CONFIG_UART_ISR_IN_IRAM=y` (kept from v0.2; answers the wiki section-8 note; user-approved 2026-11-09); `CONFIG_DMT_BUS_WRITE=n`. Verify the IRAM symbol name in IDF v6.0.2 (`esp_driver_uart` Kconfig); if it differs, use the real one and report it.

### Bus-access lint (`tools/check_bus_access.py`)
Python 3 stdlib only. Scans `*.c *.h *.cpp` under `firmware/main/` and `firmware/components/`; skips `managed_components/`, `build*/`, `test/`, and every `test_host/` directory. Comments and strings are not exempt. Output on violation: `path:line: R<n>: <message>`, exit 1. Paths below are relative to `firmware/`.

Rules implement architecture section 3 "Compile-time gate and bus-access lint (D10, D20)"; where a rule is stricter than the wiki wording it says so (the stricter R1, R4, R5 are user-approved 2026-11-09). `WRITE_FILE` = `components/mb_link/src/mb_link_write.c` (does not exist in phase 1; compiled only with the gate on, phase 3).

| Rule | Check |
|---|---|
| R1 | `\bmbc_master_\w*` only in `components/mb_link/src/` (stricter: not in its public header); there only `mbc_master_create_serial`, `mbc_master_start`, `mbc_master_stop`, `mbc_master_delete`, `mbc_master_send_request`. Any other `mbc_master_*` name (incl. `mbc_master_set_parameter`) anywhere fails. |
| R2 | `\b(mbc_slave_\w*|mbs_\w*|mbc_tcp_\w*)` anywhere fails; `#include` of `esp_modbus_slave.h` anywhere fails. |
| R3 | `#include` of `esp_modbus_master.h` or `esp_modbus_common.h` only in `components/mb_link/src/`. |
| R4 | `\b(uart_write_bytes\w*|uart_tx_chars|uart_set_rts|uart_set_dtr|uart_set_loop_back|uart_set_line_inverse|_uart_set_pin\w*)\b` anywhere fails (stricter than the wiki, which allows them in `mb_link`/`rs485_port`: esp-modbus transmits internally, our code never needs them). |
| R5 | `\b(uart_set_pin|uart_set_mode|uart_driver_install|uart_driver_delete|uart_param_config|uart_read_bytes)\b` only in `components/rs485_port/` (stricter: not in `mb_link`). Each `uart_set_mode(` call's 2nd argument must be literally `UART_MODE_RS485_HALF_DUPLEX` or `UART_MODE_UART` (argument parsing may join lines up to the closing parenthesis). |
| R6 | `CONFIG_DMT_BUS_(TX|DE)_GPIO` only in `components/rs485_port/` and `main/`. |
| R7 | `\bmb_link_write\w*` only in `WRITE_FILE` and `components/mb_link/include/mb_link_write.h` (neither exists in phase 1). |
| R8 | In `components/` outside `WRITE_FILE`: `\bMB_FUNC_WRITE\w*` fails; in `components/mb_link/` outside `WRITE_FILE` every assignment to a `command` field (`\.command\s*=` or `command\s*=`) must have the literal value `0x03` or `MB_FUNC_READ_HOLDING_REGISTER` (FC06 constant only in `WRITE_FILE`). |

`--self-test`: built-in `(virtual_path, snippet)` cases, at least one violating case per rule and per allowed-name exception, plus good cases (e.g. `mbc_master_send_request` in `components/mb_link/src/mb_link.c`; `uart_set_mode(p, UART_MODE_RS485_HALF_DUPLEX)` in `components/rs485_port/src/rs485_port.c`; `.command = 0x03` in `components/mb_link/src/mb_link.c`; `mb_link_write` and `.command = 0x06` in `WRITE_FILE`). Exit 0 only if every bad case reports exactly its rule and every good case passes.

Build hook in `firmware/CMakeLists.txt` after `project()`: one custom target `bus_access_lint` that runs `${PYTHON} tools/check_bus_access.py --self-test` and then `${PYTHON} tools/check_bus_access.py`, made a dependency of `${CMAKE_PROJECT_NAME}.elf`. Either failing fails `idf.py build`.

### Warnings
`main`: `target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)`. Every later component does the same (stated in `components/README.md`).

### Host-test project (`firmware/test/host/`)
- IDF project for target `linux`; `EXTRA_COMPONENT_DIRS` = `../../components`; `set(COMPONENTS main)`.
- `main/CMakeLists.txt`: compiles `main/test_*.c` and, via `file(GLOB ...)`, `../../../components/*/test_host/test_*.c`; `REQUIRES unity` (later tasks append their component). Document the pattern in `firmware/README.md`.
- `test_main.c`: runs all tests and `exit()`s non-zero if any failed. Use `TEST_CASE` + `unity_run_all_tests()` if it works on `linux` in IDF v6.0.2, else explicit `RUN_TEST` lists; record which in `firmware/README.md`.
- Compile definition `FIXTURE_DIR` = absolute path of `firmware/test/fixtures` (directory created by 011).

## Acceptance checks
1. `. ~/esp/esp-idf-v6.0.2/export.sh && cd firmware && idf.py set-target esp32s3 && idf.py build` passes, zero warnings in `main`.
2. `python firmware/tools/check_bus_access.py --self-test` exits 0; `python firmware/tools/check_bus_access.py` exits 0 on the tree.
3. Temporary edits, each reverted, each output reported: (a) `uart_write_bytes(1, "x", 1);` in `main/main.c` -> `idf.py build` fails with R4; (b) breaking one self-test case expectation -> `idf.py build` fails.
4. `idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;<tmpfile with CONFIG_DMT_BUS_WRITE=y>" reconfigure` (or menuconfig) fails with the write-gate message; revert.
5. Host tests: in `firmware/test/host/`, `idf.py --preview set-target linux` (or the v6.0.2 equivalent; record it), `idf.py build`, run `build/*.elf`: exit 0; a temporarily failing assertion gives non-zero (revert).
6. `idf.py menuconfig` shows the menu with all 15 symbols and defaults above.
7. No new dependencies; nothing under `build/`, `managed_components/`, `sdkconfig`, `dependencies.lock` staged.

## Test strategy
- Lint rules: unit-tested by `--self-test` (pure Python, host), run on every build.
- Host runner: smoke test plus deliberate-failure check.
- No hardware.

## Recommended implementer model
sonnet: IDF CMake custom-target wiring, the linux host project and eight precise lint rules with path exceptions have pitfalls a small model tends to get wrong.

## Out of scope
- Any UART, Modbus, Matter, Wi-Fi, NVS code; `main.c` behaviour.
- Adding `espressif/esp-modbus` (002) or `espressif/esp_matter` (phase 2).
- The ELF check (002). CI. clang-tidy (report whether `esp-clang` is installed; do not install it).
