# 001 - Firmware skeleton, Kconfig, zero-TX lint, host-test runner

Phase: P1 (listen-only). Status: spec ready. Depends on: nothing.

## Goal
Turn the existing `firmware/` skeleton into the P1 project layout every later task builds on:
1. `firmware/components/` directory for project components (empty apart from what this task needs).
2. Project Kconfig menu with the P1 bus parameters.
3. `sdkconfig.defaults` with the P1 settings (UART ISR in IRAM).
4. The **zero-TX lint** (`tools/check_zero_tx.py`) with a self-test, wired into the build so a violation fails `idf.py build`.
5. Warnings-as-errors for our own code.
6. A host-test runner project for the IDF `linux` target with Unity and one trivial passing test, so later tasks only add test files.

## Zero-TX rules (P1/P2, binding for this and every bus-related task)
- Hardware: MAX485-M DE and /RE hardwired to GND, DI unconnected; the transceiver can never drive the bus.
- Bus UART: `UART_MODE_UART` only (never call `uart_set_mode`); RX pin only. Exactly one `uart_set_pin` call in the code base, in `components/rs485_rx/`, with TX, RTS and CTS = `UART_PIN_NO_CHANGE`. TX buffer size 0.
- Forbidden outside `components/bus_tx/` (which does not exist in P1/P2): `mbc_master_*`, `mbc_slave_*`, `uart_write_bytes*`, `uart_tx_chars`, `uart_set_mode`, and (architect addition, flagged to the user) `uart_set_rts`, `uart_set_dtr`, `uart_set_loop_back`, `_uart_set_pin4`, `_uart_set_pin6`; `#include` of `esp_modbus_master.h`, `esp_modbus_slave.h`, `esp_modbus_common.h`.
- `mbcrc.h` may be included only by `components/rtu_sniff/src/rtu_crc.c`.
- esp-modbus is linked only for `mb_crc16`; no controller is created in P1/P2 (task 002 adds the ELF check).

## Files / directories owned
- `firmware/CMakeLists.txt` (add the lint hook; keep project name `daikin_matter_thermostat`)
- `firmware/sdkconfig.defaults` (new)
- `firmware/main/Kconfig.projbuild` (new)
- `firmware/main/CMakeLists.txt` (only to add `-Wall -Wextra -Werror` for `main`)
- `firmware/components/` (create; add a short `README.md` saying each component gets `-Wall -Wextra -Werror` and host tests in `test_host/`)
- `firmware/tools/check_zero_tx.py` (new)
- `firmware/test/host/` (new host-test project: `CMakeLists.txt`, `sdkconfig.defaults`, `main/CMakeLists.txt`, `main/test_main.c`, `main/test_smoke.c`)
- `firmware/README.md` (new: build, lint and host-test commands)

Do not touch `main/main.c` (owned by 006) except if the build requires it, and do not add any dependency.

## Wiki pages relied on
- `concepts/firmware-architecture` (approved 2026-11-07; sections "Zero-TX by construction", "Component layout", "Test strategy")
- `concepts/dev-environment` (IDF v6.0.2, registry-only rule, `export.sh`)
- `concepts/modbus-rs485-link` (9600 8N1)

## Interfaces
Kconfig symbols (menu "Daikin bus (P1 listen-only)"), consumed by 005/006:

| Symbol | Type | Default | Range | Source / note |
|---|---|---|---|---|
| `CONFIG_DMT_BUS_UART_PORT` | int | 1 | 1-2 | UART0 stays the console |
| `CONFIG_DMT_BUS_RX_GPIO` | int | 4 | 0-48 | placeholder, the user confirms the wiring in the bring-up checklist (008) |
| `CONFIG_DMT_BUS_BAUD` | int | 9600 | 1200-115200 | wiki `modbus-rs485-link` (9600 8N1); adjustable only because open item 1b is unverified |
| `CONFIG_DMT_BUS_RX_IDLE_CHARS` | int | 3 | 2-3 | UART RX timeout in character times; must be > 1.5 and < 3.5 (Modbus RTU framing, external) |
| `CONFIG_DMT_BUS_RESP_TIMEOUT_MS` | int | 1000 | 50-5000 | monitor pairing window only (no bus effect) |

Help text of each symbol cites its source. 8N1 is not configurable.

`sdkconfig.defaults`: `CONFIG_UART_ISR_IN_IRAM=y` (D2). Verify the exact symbol name in IDF v6.0.2 (`idf.py menuconfig` / `Kconfig` of `esp_driver_uart`); if it differs, use the real name and report it.

Lint (`tools/check_zero_tx.py`):
- Scans `firmware/main/` and `firmware/components/` (`*.c`, `*.h`, `*.cpp`), skips `components/bus_tx/`, `managed_components/`, `build*/`, `test/`.
- Fails (exit 1, file:line and rule printed) on: any forbidden identifier listed above (regex `\b(mbc_master_\w*|mbc_slave_\w*|uart_write_bytes\w*|uart_tx_chars|uart_set_mode|uart_set_rts|uart_set_dtr|uart_set_loop_back|_uart_set_pin4|_uart_set_pin6)\b`); forbidden includes; `mbcrc.h` included anywhere except `components/rtu_sniff/src/rtu_crc.c`; any `uart_set_pin(` outside `components/rs485_rx/`; more than one `uart_set_pin(` call; a `uart_set_pin(` call whose 2nd, 4th and 5th argument are not literally `UART_PIN_NO_CHANGE` or which has other than 5 arguments (argument parsing may join continuation lines up to the closing parenthesis).
- Comments are not exempt (simpler and stricter): the forbidden names must not appear at all.
- `--self-test`: runs the rules against built-in good and bad snippets (one bad snippet per rule, plus a valid `uart_set_pin(port, UART_PIN_NO_CHANGE, rx, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE)`) and exits 0 only if every bad snippet is caught and every good one passes.
- Python 3 standard library only, runnable with the IDF Python.

Build hook: `firmware/CMakeLists.txt` adds a custom target that runs `${PYTHON} tools/check_zero_tx.py` and is a dependency of the app build (for example `add_custom_target(zero_tx_lint ALL ...)` plus `add_dependencies(${CMAKE_PROJECT_NAME}.elf zero_tx_lint)` after `project()`); a violation must make `idf.py build` fail.

Host-test project (`firmware/test/host/`):
- IDF project for target `linux`; `EXTRA_COMPONENT_DIRS` = `../../components`; `set(COMPONENTS main)` so only `main` and what it requires are built.
- `main/CMakeLists.txt` collects `test_*.c` in `main/` and, with `file(GLOB ...)`, every `../../../components/*/test_host/test_*.c`; `REQUIRES unity` plus (added by later tasks) the components under test. Document in `firmware/README.md` how later tasks add a component to `REQUIRES`.
- `test_main.c`: runs all registered Unity tests and calls `exit()` with a non-zero status if any test failed (an agent must be able to rely on the exit code). Use `TEST_CASE` registration with `unity_run_all_tests()` if it works on `linux` in IDF v6.0.2; otherwise use explicit `RUN_TEST` lists and document the pattern. Record which one you used in `firmware/README.md`.
- `test_smoke.c`: one passing test.
- Compile definition `FIXTURE_DIR` = absolute path of `firmware/test/fixtures` (directory created by 007; the definition can exist now).

## Acceptance checks
1. `. ~/esp/esp-idf-v6.0.2/export.sh && cd firmware && idf.py set-target esp32s3 && idf.py build` passes with zero warnings in `main`.
2. `python firmware/tools/check_zero_tx.py --self-test` exits 0; `python firmware/tools/check_zero_tx.py` exits 0 on the tree.
3. Temporarily adding `uart_write_bytes(1, "x", 1);` to `main/main.c` makes `idf.py build` fail with the lint message (revert afterwards; report the output).
4. Host tests: in `firmware/test/host/`, `idf.py --preview set-target linux` (or the v6.0.2 equivalent; record it), `idf.py build`, run the produced ELF: smoke test passes, exit code 0; a temporarily failing assertion gives a non-zero exit code (revert afterwards).
5. `idf.py menuconfig` shows the "Daikin bus (P1 listen-only)" menu with the five symbols and defaults above.
6. `build-verifier` dependency check: no new dependencies; nothing under `build/`, `managed_components/`, `sdkconfig` staged.

## Test strategy
- Unit-tested: the lint rules via `--self-test` (pure Python, host).
- Host-runner proven by the smoke test and the deliberate-failure check.
- No hardware.

## Recommended implementer model
sonnet: CMake/IDF build-system wiring (custom target dependency, linux host project, Unity runner choice) has pitfalls that a small model tends to get wrong; the lint has several precise rules.

## Out of scope
- Any UART, Modbus, Matter, Wi-Fi or NVS code; `main.c` behaviour.
- Adding `espressif/esp-modbus` (task 002) or `espressif/esp_matter` (P2).
- clang-tidy / `idf.py clang-check` configuration (mention in the report whether `esp-clang` is installed; do not install it).
- CI configuration.
