# 002 - esp-modbus pin, `rtu_crc` adapter over `mb_crc16`, build/link guards, spike

Phase: P1. Status: spec ready. Depends on: 001.

Why this comes before the decoder (003): the user decided (D7a = B) that the sniffer reuses esp-modbus's private `mb_crc16`. Whether and how that CRC can run in host tests decides how 003 is tested, so it is settled first. The esp-modbus build spike from the v0.2 outline is merged into this task.

## Goal
1. Add `espressif/esp-modbus` pinned to **exactly `==2.1.4`** (latest on the registry on 2026-11-07, external) as a dependency of a new component `rtu_sniff`, in this task holding only the CRC adapter.
2. Implement `rtu_crc16()` / `rtu_crc_frame_ok()` as a thin wrapper over `mb_crc16`, with guards that fail the build loudly if the symbol disappears, moves or changes signature.
3. Make the same CRC (the real esp-modbus source, not a re-implementation) run in host tests on the IDF `linux` target, following the order H0 then H1 below, or stop and report.
4. Spike findings for P3 (no committed code for them): esp-modbus 2.1.4 builds on IDF v6.0.2 / esp32s3, the v2 master API, UART ownership, RS485 half-duplex direction signal.

## Zero-TX rules (P1/P2, binding)
- Hardware: MAX485-M DE and /RE hardwired to GND, DI unconnected.
- Bus UART: `UART_MODE_UART` only, RX pin only, single `uart_set_pin` in `rs485_rx` with TX/RTS/CTS = `UART_PIN_NO_CHANGE`.
- Forbidden outside `components/bus_tx/`: `mbc_master_*`, `mbc_slave_*`, `uart_write_bytes*`, `uart_tx_chars`, `uart_set_mode`, `uart_set_rts`, `uart_set_dtr`, `uart_set_loop_back`, `_uart_set_pin4/6`, includes of `esp_modbus_master.h` / `esp_modbus_slave.h` / `esp_modbus_common.h`. Enforced by `tools/check_zero_tx.py` on every build.
- **esp-modbus must not be created or started in P1/P2: no controller instance.** It is linked only for `mb_crc16`. This task adds an ELF check proving it.
- The spike's master-API compile test (item 4) is built in the session scratchpad only, never committed, never flashed.

## Files / directories owned
- `firmware/components/rtu_sniff/CMakeLists.txt` (new; 003 later adds its decoder sources)
- `firmware/components/rtu_sniff/idf_component.yml` (new)
- `firmware/components/rtu_sniff/include/rtu_crc.h` (new)
- `firmware/components/rtu_sniff/src/rtu_crc.c` (new; the only file allowed to include `mbcrc.h`)
- `firmware/components/rtu_sniff/test_host/test_rtu_crc.c` (new)
- `firmware/components/rtu_sniff/host_shim/mb_common.h` (new, only if H1 is used)
- `firmware/tools/check_elf_symbols.py` (new) and its hook in `firmware/CMakeLists.txt` (post-build)
- `firmware/test/host/main/CMakeLists.txt` (add `rtu_sniff` to `REQUIRES`)
- `firmware/main/CMakeLists.txt` (add `REQUIRES rtu_sniff` so the component is in the target build; no code in `main.c`)

## Wiki pages relied on
- `concepts/firmware-architecture` (sections "esp-modbus usage", "Zero-TX by construction", "Test strategy")
- `concepts/dev-environment` (registry-only rule, IDF v6.0.2)
- `concepts/modbus-rs485-link` (Modbus RTU, FC 03/06)

External facts used (from the esp-modbus v2.1.4 tag on GitHub and the registry page, fetched 2026-11-07; verify them in the downloaded component, do not trust them blindly):
- `modbus/mb_transports/rtu/mbcrc.h`: `uint16_t mb_crc16(uint8_t *frame_ptr, uint16_t len_buf);`, no includes, only `#pragma once`.
- `modbus/mb_transports/rtu/mbcrc.c`: includes only `mb_common.h`, uses only `uint8_t`/`uint16_t`/`int`, does not write through `frame_ptr`; returns `crc_hi << 8 | crc_lo`.
- `mb_transports/rtu` is in `priv_include_dirs`; the component `REQUIRES esp_driver_uart`, `PRIV_REQUIRES esp_timer esp_event esp_netif vfs` (IDF > 5.2); no `linux` handling in its `CMakeLists.txt`.
- `mb_common.h` includes `sdkconfig.h`, `esp_log.h`, FreeRTOS-dependent port headers.

## Interfaces
`include/rtu_crc.h` (pure C, no IDF headers):
```c
#include <stdbool.h>
#include <stdint.h>
/* CRC-16/MODBUS as computed by esp-modbus mb_crc16(); on the wire the low byte is sent first. */
uint16_t rtu_crc16(const uint8_t *data, uint16_t len);
/* true if len >= 4 and the last two bytes equal rtu_crc16(frame, len - 2), low byte first. */
bool rtu_crc_frame_ok(const uint8_t *frame, uint16_t len);
```
`src/rtu_crc.c`:
- `#include <stdint.h>` then `#include "mbcrc.h"` (the header has no includes of its own).
- Signature guard: `_Static_assert(__builtin_types_compatible_p(__typeof__(mb_crc16), uint16_t (uint8_t *, uint16_t)), "esp-modbus mb_crc16 signature changed: re-check D7a");`
- Cast away `const` only in this file, with a comment that v2.1.4 does not write through the pointer.
- Include path to the private header: obtained in `CMakeLists.txt` from the esp-modbus component directory (`idf_component_get_property(... espressif__esp-modbus COMPONENT_DIR)` or equivalent), appended with `modbus/mb_transports/rtu`, added as a **PRIVATE** include of `rtu_sniff` only. A missing directory must be a CMake `FATAL_ERROR` naming D7a.
- Version guard: `CMakeLists.txt` reads the esp-modbus `idf_component.yml` and fails with `FATAL_ERROR` unless its version is `2.1.4`.

`idf_component.yml` of `rtu_sniff`:
```yaml
dependencies:
  espressif/esp-modbus:
    version: "==2.1.4"   # exact: mb_crc16 is private API (D7a)
```
Add a `rules:` entry excluding `linux` only if H0 fails and H1 is used.

Host CRC (decide in this order, report evidence for each step):
- **H0**: build the host-test project with `rtu_sniff` requiring esp-modbus on `linux` as is. If it configures, compiles and links (only `mbcrc.o` is needed at link time), use it: no shim. Expected to fail (inferred from its requirements), but try it.
- **H1** (if H0 fails): on `linux`, `rtu_sniff`'s `CMakeLists.txt` does not require esp-modbus; it compiles `mbcrc.c` from the firmware project's managed copy `firmware/managed_components/espressif__esp-modbus/modbus/mb_transports/rtu/mbcrc.c` together with `host_shim/mb_common.h` (content: `#pragma once` and `#include <stdint.h>`, plus a comment pointing to D7a). The same version guard applies to that copy. If the directory is missing: `FATAL_ERROR "run 'idf.py reconfigure' in firmware/ first (D7a host CRC)"`. The esp-modbus dependency gets `rules: - if: "target != linux"` (check the exact rule syntax supported by idf-component-manager 3.1.2).
- **If neither works cleanly: stop, do not invent a third way.** Report the errors; the fallback is a user decision (see "Escalation").

ELF check (`tools/check_elf_symbols.py`, post-build hook on the app ELF, target builds only):
- Runs the toolchain `nm` on the ELF; fails the build if any defined symbol matches `^(mbc_|mbm_|mbs_)` or equals `mb_port_ser_create` (controller / serial-port code of esp-modbus present in the image).
- Fails if `mb_crc16` is **absent** (proves the adapter links the real symbol).
- Note: `uart_write_bytes` may be present in the image through the IDF console; do not fail on it at ELF level (the source lint covers our code).

## Acceptance checks
1. Target build (`idf.py build`, esp32s3) passes; `dependencies.lock` (local, ignored) shows `espressif/esp-modbus` `2.1.4`; report the `idf.py size-components` line for esp-modbus.
2. ELF check passes: no controller symbols, `mb_crc16` present. Report the `nm | grep -E 'mb_crc16|mbc_'` output.
3. Guards proven, each by a temporary local edit that is reverted and whose failing output is reported: (a) change the guard's expected type to `uint16_t (const uint8_t *, uint16_t)` -> `_Static_assert` fails; (b) point the private include path to a wrong directory -> `FATAL_ERROR`; (c) change the expected version string -> `FATAL_ERROR`.
4. Host tests (H0 or H1) build and pass with exit code 0.
5. Lint passes (`rtu_crc.c` is the only includer of `mbcrc.h`).
6. Source inspection of the downloaded esp-modbus 2.1.4, report each with file:line: no `__attribute__((constructor))`, no linker fragment (`.lf`) or `WHOLE_ARCHIVE`/`-u` forcing controller code into the image, no code that runs without an explicit `mbc_*_create*` call.
7. Spike items, reported only (not committed): in a throwaway project in the session scratchpad, a file calling `mbc_master_create_serial`, `mbc_master_start`, `mbc_master_send_request`, `mbc_master_delete` with `UART_MODE_RS485_HALF_DUPLEX` configured by the application compiles and links for esp32s3 with IDF v6.0.2. Record: exact prototypes and `mb_communication_info_t` fields used; that `mb_port_ser_create` installs and `mb_port_ser_delete` deletes the UART driver (needed for the U2 time-share in P3); that esp-modbus itself does not call `uart_set_pin`/`uart_set_mode` (the application does); which signal drives the transceiver DE in `UART_MODE_RS485_HALF_DUPLEX` on esp32s3 in IDF v6.0.2 (RTS or DTR; the v6.0.2 `uart.h` says DTR is hardware-controlled in RS485 mode) with the IDF hal/ll file:line; whether `uart_get_collision_flag` is usable while esp-modbus owns the port.

## Test strategy
Host unit tests in `test_host/test_rtu_crc.c` (Unity), all must pass on `linux`:
- Check value: `rtu_crc16("123456789", 9) == 0x4B37` (CRC-16/MODBUS catalogue check value, external). If it fails, **stop and report**; do not change the expectation (it would mean the return byte order differs from the assumption in the architecture).
- At least two complete Modbus RTU frames from an independent published reference (e.g. the Modbus over Serial Line specification examples or another documented source; cite it in the test comment) pass `rtu_crc_frame_ok`; cross-check each with a second CRC-16/MODBUS tool during authoring and say which.
- Flipping any single bit of a valid frame makes `rtu_crc_frame_ok` false; swapping the two CRC bytes makes it false.
- `len` 0..3 -> `rtu_crc_frame_ok` false; `rtu_crc16(data, 0) == 0xFFFF`.
- The same test file builds for esp32s3 (not run: no board attached).

## Escalation (needs the user, do not decide)
If H0 and H1 both fail, report it. The architect's proposed minimal fallback for the user's decision: **F1** host tests of the decoder use a test-only reference CRC-16/MODBUS (bitwise, in `test_host/`, never linked into the firmware), and the golden-vector test of the real `mb_crc16` runs only on target (once a board is attached); the build-time guards stay. **F2** decoder tests run only on target. Architect recommends F1.

## Recommended implementer model
sonnet: CMake component-property and managed-component path handling, `rules:` syntax and the H0/H1 investigation need judgement and careful reporting; not mechanical.

## Out of scope
- Any decoder logic (003), UART code (005).
- Committing any code that calls esp-modbus controller APIs.
- Changing the esp-modbus Kconfig defaults beyond what is needed to build.
- P3 design decisions based on the spike (report findings only).
