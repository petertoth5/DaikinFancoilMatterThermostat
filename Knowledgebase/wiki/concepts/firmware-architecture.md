---
title: Firmware architecture (approved 2026-11-07, user)
type: concept
sources: ["project-brief.md", "EKWHCTRL0.EKWHCTRL1.EKRTCTRL1_Installation manual_N420384A_English.pdf", "EKWHCTRL1,EKRTCTRL1,EKWHCTRL0_Installation manual_N420384A ModBus RTU_English.pdf", "MAX485-M_EN_10035508.pdf", "20261107_192127.jpg"]
updated: 2026-11-07
---

# Firmware architecture (accepted v0.2)

Written by `architect` (v0.2), reviewed by `firmware-reviewer` (v0.1 review: 1 blocker, 8 major, 8 minor, all integrated), **accepted by the user on 2026-11-07**. The architecture as a whole is a user decision (2026-11-07, not in raw). Register facts cite [[concepts/register-map]], [[concepts/flag-registers]], [[concepts/web-restriction-flags]] (from [[sources/n420384a-english]] p4/p5). Tags: *(external)* = ESP registry / GitHub / IDF / CHIP / Modbus spec, not in raw; *(inferred)* = design conclusion. Task specs: `tasks/README.md` (P1: 001–008). Context: [[concepts/project-goal]], [[concepts/bus-topology]], [[concepts/dev-environment]].

**Superseded:** v0.1 had a Kconfig + NVS runtime TX enable (RESUME.md, 2026-11-07); v0.2 keeps only one compile-time gate (reviewer simplicity cut, accepted by the user).

## Scope and phases (D2)
- **P1** listen-only sniffer + diagnostics; no TX hardware, no TX code. Answers [[concepts/bus-topology]] open items 1 (master), 1b (protocol), 4 (STAT/OUT), 5 (A/B mapping) and measures the timing for P3. Items 3/3a: bring-up checklist (task 008 -> `docs/p1-bringup/`).
- **P2** Matter read-only mirror, Thermostat endpoint first, then Fan (D3); still zero TX.
- **P3** guarded TX, master only on Matter-originated commands; specified only after the P1 data.
- Out of scope: scheduling; OTA and certification.

## Hard rules
- ESP-IDF v6.0.2, target esp32s3; registry-only dependencies ([[sources/project-brief]], [[concepts/dev-environment]]).
- The existing EKWHCTRL1 <-> INN-FR-B32 traffic on "+ A B -" ([[entities/ekwhctrl1]], [[entities/inn-fr-b32]]) must not be disturbed. D1: listen on "+ A B -" first, choose the TX tap/path after P1 (bus-topology item 6 stays open until then).
- Zero TX by construction in P1/P2 (below).

## Components
- `espressif/esp-modbus` **==2.1.4** exact *(external, registry 2026-11-07)*: P1 only its private `mb_crc16` (D7a = B); P3 its serial master for the time-shared write (D7b = U2).
- `espressif/esp_matter` ^1.6.1 (P2) *(external)*.
- IDF built-ins: esp_driver_uart, console, esp_timer, freertos, nvs_flash (P2/P3), unity.
- esp-modbus cannot sniff (no raw-frame callback; drops frames outside its own transaction) *(external, v2.1.4 source)*, so `rtu_sniff` + `bus_monitor` stay custom (D7).

## Layers (`firmware/components/`)
| Component | Kind | Phase | Role |
|---|---|---|---|
| `rtu_sniff` (+ `rtu_crc` adapter over `mb_crc16`) | pure | P1 | frame split/decode |
| `bus_monitor` (+ `daikin_regs` table) | pure | P1 | pairing, register mirror, stats |
| `bus_fmt` | pure | P1 | dump formatting |
| `rs485_rx` | HW | P1 | RX-only UART driver with stop/start |
| `bus_task` (+ console) | glue | P1 | single owner of the bus UART |
| `mapping` | pure | P2 | register <-> Matter conversion |
| `matter_bridge` | esp_matter | P2 | endpoints; rejects writes in P2 |
| `bus_tx` | HW | P3 only | the only place allowed to call the esp-modbus master / UART TX |

## Threading
- `bus_task` prio 20, pinned to core 1, single owner of the bus UART: `rs485_rx_read` (100 ms) -> `rtu_sniff_feed` -> `bus_monitor_on_frame` (mutex) -> tick.
- `bus_dump` task prio 2 drains a queue; drops and counts when full.
- Console REPL on UART0 / USB-Serial-JTAG, never the bus UART (`#error` + runtime check).
- P2: `matter_bridge` takes snapshots (mutex copy) and updates attributes in the Matter context; `bus_task` never takes the CHIP lock.
- `CONFIG_UART_ISR_IN_IRAM`.

## esp-modbus CRC use (D7a)
- `rtu_sniff` validates CRC via `mb_crc16` (private header `modbus/mb_transports/rtu/mbcrc.h`, in esp-modbus `priv_include_dirs`) through `rtu_crc.c`, the only file allowed to include it *(external)*.
- Guards: CMake `FATAL_ERROR` if the header dir is missing or the version is not 2.1.4; `_Static_assert` on the signature `uint16_t(uint8_t*, uint16_t)`; link error if the symbol is gone; post-build ELF check; golden vector "123456789" -> 0x4B37 *(external, CRC catalogue)*. Private header chosen over a self-declared prototype, which would link silently after a signature change *(inferred)*.
- Host tests: H0 esp-modbus on the linux target (expected to fail, *inferred*); else H1 compile only `mbcrc.c` from `managed_components` with a stdint shim; if both fail: **user decision** F1 (host reference CRC + target golden test, recommended) or F2 (decoder tests on target only).
- P1/P2: no esp-modbus controller is created; the ELF check fails if `mbc_*`/`mbm_*`/`mbs_*`/`mb_port_ser_create` are present.
- P3: `mbc_master_create_serial/start/send_request/delete`; esp-modbus installs/deletes the UART driver itself but calls neither `uart_set_pin` nor `uart_set_mode` (`bus_tx` does) *(external)*.

## Zero TX in P1/P2
1. MAX485-M DE and /RE hardwired to GND, DI unconnected (pins: [[sources/max485-datasheet]], header [[sources/max485-module-photo]]).
2. `UART_MODE_UART`, never `uart_set_mode`; RX pin only; TX buffer 0; no flow control.
3. `tools/check_zero_tx.py` on every build forbids, outside `components/bus_tx/`: `mbc_master_*`, `mbc_slave_*`, `uart_write_bytes*`, `uart_tx_chars`, `uart_set_mode`; plus architect additions **pending user OK**: `uart_set_rts`, `uart_set_dtr`, `uart_set_loop_back`, `_uart_set_pin4/6`, esp-modbus controller headers.
4. esp-modbus linked for `mb_crc16` only, ELF-checked.
5. Pre-connection measurements per the bring-up checklist (task 008).

## P1 receive path
- `rs485_rx`: 9600 8N1 ([[concepts/modbus-rs485-link]]), `uart_set_rx_timeout` = `CONFIG_DMT_BUS_RX_IDLE_CHARS` (default 3), always_rx_timeout; chunks `{t_us, data, idle_after, discontinuity_before}`; clean stop/start (U2 precondition).
- `rtu_sniff`: splits on idle and by frame shape with CRC: FC03 req 8 B, FC03 resp 5+N (byte count), FC06 8 B, exception 5 B; tolerates ≤2 leading garbage bytes; kinds FC03_REQ / FC03_RESP / FC06 / EXCEPTION / OTHER_FC / CRC_ERROR / OVERSIZE.
- Timing: one character = 10 bits / baud = 1042 µs; accuracy bounded by task latency *(inferred)*; jitter measured in P1 (Wi-Fi off) and P2 (Wi-Fi on); keep a 2× margin.
- `bus_monitor`: FC03 pairing by address/byte count within a timeout; FC06 request + identical echo = confirmed write; counters for exceptions, orphans, unanswered; register mirror per (addr, reg) with value/source/age/counts; stats incl. the idle gap after each transaction per request signature (P3 input), FC06 turnaround, addresses seen, undocumented registers; verdict = CRC-valid frame ratio; inverted-idle flag. Limitation: a master retry of the same FC06 is indistinguishable from an echo.
- Console (read-only): `bus status|regs|timing|addrs|dump chunks|frames on|off|reset|rx restart`. Dump = fixture format `RX <t_us> <idle> <disc> <hex>`, `FR <t_end_us> <kind> <hex>`, header `# dmt-capture v1`.

## P1 measurement goals
Who polls what and how often; valid 9600 8N1; STAT 104 / OUT 009 seen (D15 data source); A/B mapping; which registers the panel writes per user action (233 values, fan-order keypress test, 008 vs 231 use); idle-gap histogram; module A-B resistance, bus voltages, termination; `rx restart`. Logs/dumps go to `Knowledgebase/raw/` for `/capture`.

## P2 Thermostat mapping (EP1, device type 0x0301)
Features Heat + Cool, **no Auto**; ControlSequenceOfOperation 4 CoolingAndHeating *(external, CHIP data_model 1.4)*.
- **LocalTemperature** (nullable) <- 000 T1 ×10; null until observed or when stale (threshold N × observed poll interval; N after P1).
- **OccupiedHeatingSetpoint / OccupiedCoolingSetpoint** (non-nullable *(external)*) <-> 231 SP ×10, both mirror 231; seeded 2000 marked "default, not observed"; writes (P3) refused until 231 has been observed; round to 0.1 °C and republish both. 008 = diagnostic only; P1 decides which of 231/008 the panel uses.
- **Limits:** clamp to the intersection of 202 SPL / 203 SPH, 245/246 SPL_W/SPH_W when 247 bit 5 is set, and absolute 5.0–40.0 (AbsMin/AbsMax 500/4000); respect 247 bit 4; slave-side enforcement unverified ([[concepts/web-restriction-flags]]).
- **SystemMode:** 201 bit 7 Stby = 1 -> Off; 233 = 3 -> Heat, 5 -> Cool *(inferred from "inv"/"est", confirm in P1)*; 233 = 0 -> Heat if STAT bit 1, Cool if STAT bit 0 (D15 = b). Write (P3): Off -> RMW 201 bit 7 = 1; Heat/Cool -> clear bit 7 if needed; write 233 only if it differs from the actual mode; never "confirm" under 233 = 0.
- **D15 risk:** STAT 104 is INN-FR-B32-only ([[concepts/flag-registers]]) and its availability on this link is open; without STAT, 233 = 0 has no data source; both/neither bit set is undefined; revisit after P1. Cool with hot water -> fan stops (STAT F.V. bits) = expected behaviour.
- **Not mapped:** 008 (diagnostic), 009 OUT, 104 STAT (except SystemMode under 233 = 0), 105 ALR_STAT, 015 MOT_SET (removed), PRG bit 4 LOCK, 247.

## P2 Fan (EP2, device type 0x002B, Fan Control; later milestone)
- **FanMode** <-> 201 bits 0–2: 000 Auto, 010 Night -> Low, 001 Silent -> Medium, 011 Maximum -> High (order *inferred* from the MVV limits, confirm in P1); 100–111 -> unknown: keep the last value, raise a diagnostic, refuse fan writes.
- Fan Off write rejected (standby only via SystemMode); every FanModeSequence contains Off *(external)*, so controllers may still offer it. FanModeSequence const 2 (OffLowMedHighAuto).
- **PercentSetting** (nullable) derived: Low 33 / Med 66 / High 100 / Auto null; writes map to the nearest mode, 0 rejected. **PercentCurrent** derived from FanMode, not from 015; value under Auto/unknown decided in the fan-milestone spec.
- FanMode, PercentSetting, PercentCurrent, FanModeSequence are mandatory *(external)*.

## P3 outline (detail after P1)
- **TX enable:** one compile-time gate `CONFIG_APP_BUS_TX` (default n) adds `bus_tx` + the esp-modbus master; no NVS/console runtime enable. Kill switch latches until reboot + an explicit user action (detail in P3).
- **U2 sequence:** wait for an idle window -> `rs485_rx_stop` -> esp-modbus create + `uart_set_pin` (TX, RX, DE) + `UART_MODE_RS485_HALF_DUPLEX` (not COLLISION_DETECT: esp-modbus would accept its own FC06 echo as the reply, *external*, source) + start -> one FC06 -> one FC03 readback -> delete -> DE forced low and verified -> `rs485_rx_start`. Blind during the transaction; success = esp-modbus OK + equal readback; bounded blind time.
- **Window sizing:** from the observed FC06 turnaround (P1), else ≥250 ms or refuse; panel keypress writes are asynchronous, so the minimum observed gap is optimistic *(inferred)*; TX idle minimums use Modbus-nominal 11-bit characters (t3.5 ≈ 4.0 ms, t1.5 ≈ 1.7 ms, *external*); esp-modbus dispatch latency is part of the budget; idle check right before TX is best effort (TOCTOU); collision avoidance only via window + idle check (answers bus-topology item 2 in outline).
- **Pacing/retries:** ≥1 s between own transactions, ≤1 per master cycle; ≤2 retries, each in a fresh window, only after a negative readback; kill switch latches on repeated failures or post-TX anomalies in master traffic (CRC errors, exceptions, unanswered above baseline).
- **Refuse TX if:** verdict bad; panel absent (k × measured cycle; absence only ever means "refuse", the node never polls in place of the master); window not learned; target address not pinned (NVS + console confirmation, not auto-learned); limits (202/203/247, 245/246 if 247 bit 5) unknown before a setpoint write.
- **PRG 201 RMW** (reviewer M3): value fresh (age < 1 master cycle) or an FC03 right before the window; abort if a master write/change of 201 was sniffed in between; readback; residual race documented; P3 blocker if P1 shows the master writes 201 routinely.
- No own NVS/flash writes during TX; scope-check DE release and dispatch latency in HIL; check that create/start sends nothing; our writes may reset the 300 s Com-err watchdog (STAT bit 12, [[concepts/flag-registers]]) and mask a lost panel *(inferred)* -> HIL checklist.
- **P3 hardware:** DI -> UART TX GPIO, DE -> RTS GPIO with 10 kΩ pull-down, /RE stays on GND; from P3 on, zero-TX-by-hardware no longer holds. Open: RTS vs DTR as DE signal in RS485 mode on esp32s3 / IDF v6.0.2 (task 002 spike reports), TX tap point (D1).

## GPIO
Avoid 19/20 (USB), 43/44 (UART0), 26–32 (flash), 33–37 (octal PSRAM, R8), strapping 0/3/45/46 *(external; exact reserved set unconfirmed, no module datasheet in raw, see [[entities/esp32-s3-wroom-1]])*. RX GPIO via Kconfig (`CONFIG_DMT_BUS_RX_GPIO`); default 4 is a placeholder.

## Commissioning (D11)
esp_matter BLE + Wi-Fi, dev DAC, test VID/PID 0xFFF1/0x8000, QR/pairing code on the console, BOOT long-press factory reset in the Matter app (P2).

## Decisions (user decision 2026-11-07, not in raw)
| # | Decision |
|---|---|
| D1 | Listen on "+ A B -" first; TX path after P1 |
| D2 | Phases P1/P2/P3; P1 hardware rules: DE and /RE to GND, plain UART, no TX code in P1/P2, measure "+"/"-", fix the RO 5 V level and ground ([[entities/max485]]), measure module A-B, no bias/termination in P1, IRAM ISR; P1 specs released as a gate exception |
| D3 | Thermostat + Fan endpoints, no fan Off; Thermostat first |
| D4 | Attribute mapping as above (with the reviewer fixes) |
| D5 | Conflict policy: bus is truth, last writer wins; optimistic accept, revert if overridden by the master within ~2 cycles; never fight the panel |
| D6 | Unseen values null / "default, not observed"; no polling; FC03 only right before a write (P3) |
| D7 | Registry esp-modbus instead of a custom layer; sniffer/monitor stay custom (esp-modbus cannot sniff) |
| D7a | B: esp-modbus private `mb_crc16` (user's choice against the recommendation); exact pin + guards |
| D7b | U2: single time-shared UART (user's choice against the recommendation); blind during the transaction |
| D8 | P3: IDF RS485 HALF_DUPLEX with RTS -> DE + pull-down; /RE to GND |
| D9 | Write whitelist: 231, 201 bits 0–2 and 7 (other bits preserved), 233 |
| D10 | TX safety: one compile-time gate, ≤2 retries, windows from observed data, ≥1 s pacing, latching kill switch, refuse rules |
| D11 | Commissioning as above |
| D12 | Host tests: IDF linux target + Unity + replay fixtures from P1 captures |
| D13 | Hardware: RO level shift/divider (or a verified 3.3 V U1), DE pull-down, /RE to GND, GPIO via Kconfig |
| D14 | chip-tool allowed for tests |
| D15 | b: no Auto; Heat/Cool from STAT under 233 = 0; revisit after P1 |

## Open (user)
- OK for the extra zero-TX lint names (rule 3).
- F1/F2 if task 002 cannot get `mb_crc16` into host tests.
- RX GPIO default 4 = placeholder.
- Hardware: termination, "+"/"-" voltages, U1/supply/RO level, A/B mapping ([[concepts/bus-topology]] items 3, 3a, 5).

## External sources (not in raw)
esp-modbus registry page + GitHub v2.1.4 source (`CMakeLists.txt`, `mbcrc.h/.c`, `mb_common.h`, `port_serial.c`, `esp_modbus_master.h`, `rtu_master.c`, `mb_master.c`); IDF v6.0.2 `uart.h`, esp32s3 `uart_ll.h`; esp_matter registry page; CHIP data_model 1.4 `Thermostat.xml` / `FanControl.xml`; Modbus serial-line spec; CRC catalogue.
