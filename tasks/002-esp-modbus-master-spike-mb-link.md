# 002 - esp-modbus `==2.1.4` master spike, `rs485_port`, `mb_link` read path, ELF check

Phase: 1. Status: spec ready (v0.3, 2026-11-08). Depends on: 001, 009 (read whitelist). 009/010 may run in parallel with this task.
Review: **bus-touching**. `firmware-reviewer` reviews this task independently, including the spike report.

## Goal
1. Add `espressif/esp-modbus` **`==2.1.4`** (Q9) as the only Modbus layer: serial master, RTU, used only through `mbc_master_send_request` with FC03 (D7).
2. `rs485_port` (HW): pins, RS485 half-duplex with DE via RTS, receive-hold, silence check (D17). Called only by `mb_link`.
3. `mb_link` (HW): the only user of the esp-modbus master and the only hardware interface above it; init/deinit, silence check, blocking classified `mb_link_read`.
4. Answer the spike questions S1-S12 with file:line evidence (architecture section 7, row 002).
5. `tools/check_elf_symbols.py` post-build check (D20).

**Write path in phase 1 (decision, user-approved 2026-11-09):** no write source exists. `mb_link_write.c` / `mb_link_write.h` are **not created**; lint R7/R8 allow them only in those files; the ELF check fails if `mb_link_write` is in the image; `CONFIG_DMT_BUS_WRITE=y` already fails the build (001). Phase 3 adds the file under the gate with its own spec.

## Bus-safety rules (binding; architecture section 3)
- FC03 only; `command` field literally `0x03` (lint R8). Inside `mb_link_read`, **before any bus access**: `addr` in 1..247 (never 0/broadcast, D21) and `daikin_regs_read_allowed(reg, qty)` (qty 1..4); otherwise `MBL_REFUSED` without touching the bus.
- One transaction at a time; esp-modbus internal retries zero (retry policy is in `poll_sched`, D21). If they cannot be disabled: stop (escalation).
- Transceiver held in receive (DE GPIO driven low -> DE and /RE low) whenever esp-modbus is not running; 10 kOhm pull-down covers reset/boot (D8).
- Create, start, stop, delete transmit nothing (S4 proves it from source).
- Only `bus_task` (006) calls `mb_link_*`; only `mb_link` calls `rs485_port_*`.

## Files / directories owned
- `firmware/components/rs485_port/` (new): `CMakeLists.txt` (`REQUIRES esp_driver_uart esp_driver_gpio`), `include/rs485_port.h`, `src/rs485_port.c`
- `firmware/components/mb_link/` (new): `CMakeLists.txt` (`REQUIRES daikin_regs rs485_port`; public because `mb_link.h` uses `rs485_port_cfg_t`), `idf_component.yml`, `include/mb_link.h`, `src/mb_link.c`
- `firmware/tools/check_elf_symbols.py` (new) and its post-build hook in `firmware/CMakeLists.txt`
- `firmware/sdkconfig.defaults` (append only esp-modbus options found in S6/S11)
- `firmware/main/CMakeLists.txt` (`REQUIRES mb_link` so it is in the target build; no code in `main.c`)
- `docs/spikes/002-esp-modbus.md` (new: spike answers with evidence; input for `wiki-keeper`)

All components `-Wall -Wextra -Werror`.

## Wiki pages relied on
- `concepts/firmware-architecture`, sections "2. Components and threading" (Modbus layer D7, escalation), "3. Bus-safety rules", "4. Hardware wiring", "6. Decisions", "7. Task impact", "8. Open items and risks"
- `concepts/modbus-rs485-link` (9600 8N1, FC03/FC06 single register), `concepts/device-addressing`
- `entities/max485` (DE high = transmit, /RE low = receive, datasheet p7/p10), `concepts/dev-environment` (registry-only)

## Interfaces
```c
/* rs485_port.h (used only by mb_link) */
typedef struct { int uart_port, tx_gpio, rx_gpio, de_gpio; uint32_t baud; } rs485_port_cfg_t;
esp_err_t rs485_port_hold_receive(const rs485_port_cfg_t *c);   /* DE GPIO as plain output, level 0 */
/* Precondition: no UART driver on the port. Plain RX-only UART (UART_MODE_UART, TX/RTS/CTS = UART_PIN_NO_CHANGE,
   TX buffer 0); counts received bytes plus frame/parity/break/overflow events for ms; deletes the driver; holds receive. */
esp_err_t rs485_port_silence_check(const rs485_port_cfg_t *c, uint32_t ms, uint32_t *activity);
esp_err_t rs485_port_attach(const rs485_port_cfg_t *c);   /* after esp-modbus installed its driver: uart_set_pin(tx, rx, de as RTS, NO_CHANGE), uart_set_mode(UART_MODE_RS485_HALF_DUPLEX); order per S5 */
esp_err_t rs485_port_release(const rs485_port_cfg_t *c);  /* after esp-modbus deleted its driver: hold receive */

/* mb_link.h */
typedef enum { MBL_OK, MBL_TIMEOUT, MBL_EXCEPTION, MBL_BAD_FRAME, MBL_ERROR, MBL_REFUSED } mb_link_res_t;
typedef struct { rs485_port_cfg_t port; uint32_t resp_timeout_ms; } mb_link_config_t;
esp_err_t mb_link_hold_receive(const mb_link_config_t *cfg);                          /* first call in app_main */
esp_err_t mb_link_silence_check(const mb_link_config_t *cfg, uint32_t ms, uint32_t *activity); /* only while not initialised */
esp_err_t mb_link_init(const mb_link_config_t *cfg);    /* create_serial (RTU, 8N1, baud, timeout), attach, start */
esp_err_t mb_link_deinit(void);                         /* stop, delete, release; safe if not initialised */
/* Blocking, <= resp_timeout_ms + frame time. exc_code = Modbus exception code, 0 if unknown. */
mb_link_res_t mb_link_read(uint8_t addr, uint16_t reg, uint16_t qty, uint16_t *val, uint8_t *exc_code);
```
`mb_link.h` includes `esp_err.h`, `rs485_port.h` and `<stdint.h>` only, never esp-modbus headers (lint R1/R3). `idf_component.yml` of `mb_link`: `espressif/esp-modbus: version: "==2.1.4"`.

## Spike questions (answer each in `docs/spikes/002-esp-modbus.md`, file:line evidence)
- S1 esp-modbus 2.1.4 builds for esp32s3 on IDF v6.0.2 next to our `-Werror` components (its own warnings do not fail us).
- S2 Prototypes and structs used: `mbc_master_create_serial`, `mbc_master_start`, `mbc_master_stop` (exists?), `mbc_master_delete`, `mbc_master_send_request`, `mb_param_request_t`, `mb_communication_info_t` (port, RTU mode, baud, parity, data/stop bits, response timeout).
- S3 **Error distinguishability:** what distinguishes (a) timeout, (b) slave exception **and its code**, (c) CRC/malformed reply, (d) local error. Give the mapping to `mb_link_res_t`.
- S4 Create/start/stop/delete transmit nothing (no probe frame, no break).
- S5 UART ownership: esp-modbus installs/deletes the driver; required order of `uart_set_pin` / `uart_set_mode` relative to create/start; which signal drives DE in `UART_MODE_RS485_HALF_DUPLEX` on esp32s3 (RTS or DTR) and its level idle vs. transmitting (IDF hal/ll file:line). Expected: RTS, low idle, high while transmitting.
- S6 Internal retries, t3.5 / inter-frame handling at 9600; how to set retries to 0 and the response timeout to `CONFIG_DMT_BUS_RESP_TIMEOUT_MS`; esp-modbus task priorities (record defaults, do not change).
- S7 Handling of slave addresses 0 and > 247.
- S8 FC03 qty 2..4 through `send_request`: buffer handling.
- S9 Blocking semantics and thread-safety of `send_request` from one task.
- S10 delete fully releases UART and heap so that silence check -> init -> deinit -> silence check -> init works repeatedly (source review; heap check on target in 008).
- S11 Can esp-modbus write handlers (FC05/06/15/16/23) be compiled out via its Kconfig? If yes: set it in `sdkconfig.defaults` and add the handler symbols to the ELF check. If no: list the write-capable symbols in the image (reported to the user: write capability exists in the library but is unreachable from our code).
- S12 No slave or TCP code in the image when only the serial master is used.

## Escalation (stop and report to the architect; the user decides)
- **S3: timeout, exception (with code) and CRC error not distinguishable** -> stop before finishing `mb_link_read` (architecture section 2, Modbus layer: "go back to the architect; alternative is an own minimal RTU master"). Report the evidence; candidate options: E1 classify by elapsed time (reply before timeout = answered, `exc_code = 0`), E2 own minimal RTU master over `rs485_port`. Not decided in this task.
- S5 other than "RTS, low idle", internal retries not disableable (S6), slave/TCP symbols unavoidable (S12): stop and report.

## ELF check (`tools/check_elf_symbols.py <elf>`, post-build, target builds)
Toolchain `nm`; fails if a defined symbol matches `^(mbc_slave_|mbs_|mbc_tcp_|mb_link_write)` or an S11 handler name; fails if `mbc_master_send_request` or `mb_link_read` is absent (proves the expected path is linked). `uart_write_bytes` may be present via console/esp-modbus; not checked at ELF level (lint R4 covers our code).

## Acceptance checks
1. `cd firmware && idf.py build` (esp32s3) passes; lint and ELF check pass; report the `idf.py size-components` line for esp-modbus and `nm` lines matching `mbc_|mbs_`.
2. Guard proofs, each a reverted temporary edit with the failing output reported: (a) `.command = 0x06` in `mb_link.c` -> lint R8; (b) `uart_set_mode` in `mb_link.c` -> lint R5; (c) a dummy `void mb_link_write(void){}` in `mb_link.c`, built with the lint target skipped by hand -> ELF check fails.
3. Code inspection checklist in the report (file:line): address and whitelist checks precede the bus call; DE GPIO held low before the silence check and after deinit; silence check uses `UART_MODE_UART` with TX/RTS `UART_PIN_NO_CHANGE`; no `mbc_master_*` outside `mb_link/src/`.
4. `docs/spikes/002-esp-modbus.md` answers S1-S12 with evidence.
5. No dependency other than `espressif/esp-modbus` `2.1.4` added (local `dependencies.lock`, not committed).

## Test strategy
- No host tests: both components are thin wrappers over IDF/esp-modbus and do not build for `linux`. Their pure inputs (whitelist) are host-tested in 009; the caller logic in 010/011 through the `ps_link_fn` seam, which has `mb_link_read` semantics.
- Target behaviour (silence check, reads, DE level, heap over init/deinit cycles) is verified on site through 008.

## Recommended implementer model
sonnet: source-level investigation of esp-modbus and IDF RS485 internals with evidence, plus safety-relevant driver code; not mechanical.

## Out of scope
- Any write code (phase 3); polling, console (006/010); Matter.
- esp-modbus Kconfig changes beyond S6/S11.
- Workarounds for an unexpected DE signal (inverter, `uart_set_line_inverse`): escalate instead.
