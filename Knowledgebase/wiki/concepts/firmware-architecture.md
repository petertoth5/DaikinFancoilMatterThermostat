---
title: Firmware architecture v0.3 (approved 2026-11-08, user)
type: concept
sources: ["project-brief.md", "EKWHCTRL0.EKWHCTRL1.EKRTCTRL1_Installation manual_N420384A_English.pdf", "EKWHCTRL1,EKRTCTRL1,EKWHCTRL0_Installation manual_N420384A ModBus RTU_English.pdf", "MAX485-M_EN_10035508.pdf", "20261107_192127.jpg"]
updated: 2026-11-09
---

# Firmware architecture (approved v0.3)

**Status:** v0.3 written by `architect`, **approved by the user on 2026-11-08** by answering Q1–Q10 all with the architect's recommendation (user decision, session 2026-11-08, not in raw). Supersedes v0.2 (approved 2026-11-07, see "Superseded v0.2" at the end). **Addendum 2026-11-09:** stage-1 detail decisions by the user (session 2026-11-09, not in raw) in section 6 "Stage-1 detail decisions"; sections 2, 3, 4 and 8 adjusted where the v0.3 text contradicted them; later 2026-11-09 answers (boot-window commands, `mb silence`, `mb log` default on) and lint rules R1–R8 from `tasks/001-skeleton-kconfig-lint.md` added. The architecture as a whole, every D-number below and every default value are user decisions of that session unless tagged otherwise.

Tags: *(external)* = Modbus spec, ESP registry, esp-modbus / IDF / esp_matter / CHIP sources, RS485 standard; not in raw. *(inferred)* = design conclusion, not stated in any source. Register facts cite [[concepts/register-map]], [[concepts/flag-registers]], [[concepts/web-restriction-flags]], [[concepts/device-addressing]] (all from [[sources/n420384a-english]] p3–p7, values identical in [[sources/n420384a-modbus-rtu]]). Context: [[concepts/project-goal]], [[concepts/bus-topology]], [[entities/ekwhctrl1]], [[concepts/dev-environment]]. Task specs: `tasks/README.md`.

Q1–Q10 as answered (user, 2026-11-08): Q1 DE + /RE tied to RTS with 10 kΩ pull-down (D8); Q2 233 = 0 with STAT unavailable -> last explicit season, default Heat, console flag "season auto, not mirrored" (D15); Q3 timing defaults accepted (D16); Q4 boot silence check yes (D17); Q5 247 read-only permanently (D18); Q6 address from Kconfig default 1 + RAM console override, no scan (D19); Q7 write caps accepted (D22); Q8 gate exception: stage-1 (probe) specs released before the on-site bus-topology items are answered, stage-2/3 specs only after the probe log (D2); Q9 esp-modbus stays `==2.1.4`; Q10 Thermostat + Fan mapping (D3/D4) re-approved.

## 1. Phases and exit criteria

Tap point and role (D1): the node connects to one A and one B of the EKWHCTRL1 **"A A B B -"** ModBus RTU port and is the **sole Modbus master** there; the **"+ A B -"** fancoil link is never touched (hard rule) ([[concepts/bus-topology]], [[entities/ekwhctrl1]], [[sources/n420384a-english]] p3). Framework rules: ESP-IDF v6.0.2, esp32s3, registry-only components ([[sources/project-brief]], [[concepts/dev-environment]]).

### Phase 1 – on-site read probe
No Matter, no Wi-Fi, FC03 only, write code compiled out (`CONFIG_DMT_BUS_WRITE=n`, section 3). Polling starts automatically after a clean 5 s boot silence check (user, 2026-11-09, section 6). Results go into a **measurement log** -> `Knowledgebase/raw/` -> `/capture`. Exit criteria:

1. Panel slave address confirmed: by the user on the panel (standby, hold AUTO 5 s, wait 10 s without changing; procedure from [[concepts/device-addressing]], p6) and by a bus reply.
2. A/B polarity: valid replies at 9600 8N1 ([[concepts/modbus-rs485-link]]).
3. Silence check saw no foreign traffic on "A A B B -" (tests the inferred "panel is slave", [[entities/ekwhctrl1]]).
4. Table of **every** documented register, starred and unstarred (p4, [[concepts/register-map]]): OK with raw/scaled value, or exception + code, or timeout.
5. Does FC03 with quantity > 1 work (e.g. 200..203 qty 4)? Not stated in the manual ([[concepts/modbus-rs485-link]]).
6. Response time min/median/max over ≥ 1000 transactions.
7. ≥ 1 h polling at the proposed rate (section 3), error rate < 1 %, no panel anomaly (display, icons, error codes, local keys).
8. Local keypress correlation: setpoint up/down (231 vs 008), each fan mode (order of 201 bits 0–2), season (233 values 3/5), standby (201 bit 7).
9. Does the panel show anything when polled (e.g. WEB icon, 247 flag 0, [[concepts/web-restriction-flags]])?
10. Panel behaviour when polling stops > 300 s and resumes (display, 105 bit 0, 104 bit 12 if readable; [[concepts/flag-registers]]).
11. Measurements: A–A and B–B continuity, A–B resistance at the panel port with mains off, "-" vs node GND voltage, module U1 / supply / RO level ([[entities/max485]]), whether "-" is shared with CP.

The user verifies on site items 1, 2, 7, 8, 9, 10, 11 and that "+ A B -" was untouched.

### Phase 2 – Matter read-only mirror
Thermostat endpoint first, then Fan (D3); Matter writes rejected. Exit: 24 h without panel anomaly; values match the panel display; bus unplugged -> attributes go stale/null within the threshold (section 5) and recover.

### Phase 3 – Matter writes
Built with `CONFIG_DMT_BUS_WRITE=y`. Exit: every whitelisted write works with readback; a local change after our write wins on the next poll; rate limit and latch trigger; log audit shows no write to 200 or 247.

Out of scope: scheduling, OTA, certification.

## 2. Components and threading

### Components (`firmware/components/`)
| Component | Kind | Phase | Role |
|---|---|---|---|
| `daikin_regs` | pure | P1 | register table from the wiki ([[concepts/register-map]]): number, name, mlt, range, R/W, starred, poll group, write whitelist; raw <-> scaled conversion, range check |
| `reg_model` | pure | P1 | per-register value, age, availability `unknown` / `ok` / `unsupported(exc)` / `timeout`; snapshot under mutex |
| `poll_sched` | pure | P1 | next transaction from time, groups, availability, backoff; health state machine (D21); fake clock |
| `mb_link` | HW | P1 | **only** user of the esp-modbus master; `mb_link_read(addr, reg, qty, out, *status)`; `mb_link_write` only with `CONFIG_DMT_BUS_WRITE`; rejects addr 0 and anything outside 1..247 *(external, Modbus spec)*; hard-coded write whitelist as last line of defence |
| `rs485_port` | HW | P1 | pins, RS485 half-duplex DE via RTS, boot silence check (D17); called by `mb_link` |
| `bus_task` + probe console | glue | P1 | single owner of `mb_link`; poll loop; write queue (P3); commands `mb status`, `mb regs`, `mb read <reg> [qty]`, `mb probe`, `mb poll on\|off`, `mb silence <s>`, `mb stats`, `mb addr <1-247>`, `mb log on\|off` (last two: user, 2026-11-09; `mb log` transaction lines default **on** from power-up, user, 2026-11-09) |
| `mapping` | pure | P2 | register <-> Matter conversion (section 5) |
| `matter_bridge` | esp_matter | P2 | endpoints; rejects writes in PRE_UPDATE *(external)*; in P3 turns them into write requests |
| `write_guard` | pure | P3 | Matter intent -> FC06 operations incl. RMW of 201, limits, debounce/caps, readback check, latch (section 3) |

Seam: `mb_link.h` is the only hardware interface. Host tests (D12): the scripted fake slave plugs in at the `poll_sched` link-function seam, because `mb_link` is not buildable for the linux target (user, 2026-11-09; supersedes the v0.3 wording "fake slave behind `mb_link.h`").

### Modbus layer (D7)
esp-modbus master via `mbc_master_send_request` (raw requests, no parameter descriptors) is the only Modbus layer. Reasons: approved, in the registry, provides CRC/framing and RS485 DE via RTS *(external)*, no own stack. **Escalation in task 002:** if timeout, exception (with code) and CRC error cannot be distinguished, go back to the architect; alternative is an own minimal RTU master.

Dependencies: `espressif/esp-modbus` **==2.1.4** (Q9), `espressif/esp_matter` ^1.6.1 (P2) *(external, registry)*; IDF built-ins `esp_driver_uart`, `esp_driver_gpio`, `console`, `esp_timer`, `freertos`, `nvs_flash` (Matter only), `unity`.

### Threading
- `bus_task`: priority ~10, pinned to core 1 *(inferred)*; the only caller of `mb_link`. esp-modbus task priorities stay at their Kconfig defaults (the task-002 spike records them).
- `matter_bridge` takes `reg_model` snapshots and updates attributes in the Matter context; `bus_task` never takes the CHIP lock.
- P3: write intents go through a queue of depth 4; newest intent per register wins.
- `CONFIG_UART_ISR_IN_IRAM=y` kept (user, 2026-11-09).
- **Console rule (kept from v0.2):** console REPL on USB-Serial-JTAG / UART0, never on the bus UART (compile-time `#error` + runtime check).

## 3. Bus-safety rules

The manual states no timing, polling interval, response time or exception codes ([[entities/ekwhctrl1]], [[concepts/bus-topology]]); all defaults below are therefore *(inferred)* design values (D16), to be re-tuned from the probe log.

### Timing (D16)
- One transaction at a time; gap ≥ 100 ms after each.
- Reference: t3.5 ≈ 4.0 ms at 9600 baud with 11-bit characters *(external, Modbus serial-line spec)*.
- Response timeout 500 ms in P1; after the probe: max(3 × observed max, 100 ms).
- Kconfig ranges as safety caps (user, 2026-11-09): gap 50–1000 ms, timeout 200–2000 ms, retries 0–1, fast period ≥ 2 s.

### Polling (D6)
FC03 quantity 1 unless the probe proves qty > 1 works (probe item 5).
- **Fast group, 5 s:** 000, 008, 201, 231, 233 (+ 104 if it answers).
- **Slow group, 60 s:** 105, 202, 203, 245, 246, 247 (+ 009, 001 if they answer).
- ≈ 16 ms per transaction, < 2 % bus use *(inferred)*.
- Starred registers (001, 009, 104, … "* Only for INN-FR-B32 board", [[concepts/register-map]]) may be unavailable via the panel *(inferred)*; they are optional.

### Read errors and health (D21)
- 1 retry on timeout/CRC error, none on an exception.
- Exception 02 -> register `unsupported` until reboot, not polled again.
- Other exception codes (user, 2026-11-09): no retry, register backs off to 60 s, not counted toward OFFLINE.
- 1–4 consecutive failures -> **DEGRADED** (user, 2026-11-09); 5 consecutive failures -> **OFFLINE**: poll only 000 every 30 s; first success -> **ONLINE**. States ONLINE / DEGRADED / OFFLINE with backoff.
- Stale = 3 × group period: 15 s fast, 180 s slow (user, 2026-11-09).
- Never broadcast; addresses 1..247 only *(external, Modbus spec)*.

### Fallbacks
- Console `mb poll off` / `mb poll on`. Polling starts automatically after a clean boot silence check (user, 2026-11-09). `mb probe` and `mb read` also work while polling is stopped, without retry and without changing link health (user, 2026-11-09).
- Boot silence check (D17): RX-only for 5 s at boot (60 s on demand via `mb silence <s>`) **before** esp-modbus starts; any received byte -> refuse to start the master / transmit, latched until reboot.
  - Console commands typed during the 5 s boot window are executed before the first poll, so `mb poll off` / `mb addr N` there stop or redirect the first frame (user, 2026-11-09).
  - After a passed on-demand `mb silence <s>` check, polling stays off until `mb poll on` (deliberately different from boot) (user, 2026-11-09).
- Physical fallback: unplug A/B. Panel behaviour without polling is unknown (probe item 10).

### Writes (P3 only)
- Matter-originated only, and only if the value differs from a fresh poll.
- Debounce 2 s per register; caps 1 write per 5 s per register, 60/h, 500/day total (RAM counters); above a cap the Matter request is rejected (D22). Caps exist because write endurance is not stated and parameters are possibly in EEPROM *(inferred)*.
- FC03 readback after every FC06; ≤ 1 retry, only after a timeout whose readback shows the write did not land.
- 3 consecutive write failures latch writes off until reboot; reads continue.
- **Whitelist (D9):** 231; 201 bits 0–2 and 7 via RMW; 233.
  - 201 RMW: FC03 201 right before, change only bits 0–2/7, preserve bit 4 LOCK, undocumented bits 3/5/6 and reserved bits 8–15 ("System Flags reserved, do not change the status", p5, [[concepts/flag-registers]]), FC06, readback. Residual race with a local keypress (tens of ms) accepted and documented.
- **Never written:** 200 (ADR), 247 (WEB), 202/203, 245/246, 209, 210–234 (MVV), 221/222, 242–244 ([[concepts/register-map]]).
- **247 read-only permanently (D18):** flags persist over power loss and flags 1/6 can lock the user out ([[concepts/web-restriction-flags]], p6). Read only for the setpoint limits (bit 4; bit 5 -> 245/246).
- **Setpoint clamp:** [202, 203] ∩ [245, 246] if 247 bit 5 ∩ [5.0, 40.0] °C; if any limit is unread or stale, the write is refused (p4: "The limit values must be respected!", [[concepts/register-map]]). Whether the panel itself enforces these limits is unknown, so the node enforces them.

### Compile-time gate and bus-access lint (D10, D20)
- `CONFIG_DMT_BUS_WRITE` default **n**; when off, `mb_link_write` and `write_guard` are not compiled. Phase 1 (user, 2026-11-09): no write source file exists at all; enabling `CONFIG_DMT_BUS_WRITE` fails the build until phase 3.
- `tools/check_bus_access.py` (replaces the v0.2 zero-TX lint):
  - rules R1–R8 as specified in the approved spec `tasks/001-skeleton-kconfig-lint.md` ("Bus-access lint"); stricter than the v0.3 text where marked (user, 2026-11-09). Summary:
  - R1: `mbc_master_*` only in `components/mb_link/src/` (not its public header), and there only `mbc_master_create_serial` / `_start` / `_stop` / `_delete` / `_send_request`; any other name (incl. `mbc_master_set_parameter`) fails everywhere;
  - R2: `mbc_slave_*`, `mbs_*`, `mbc_tcp_*` and `#include` of `esp_modbus_slave.h` nowhere;
  - R3: `#include` of `esp_modbus_master.h` / `esp_modbus_common.h` only in `components/mb_link/src/`;
  - R4: `uart_write_bytes*`, `uart_tx_chars`, `uart_set_rts`, `uart_set_dtr`, `uart_set_loop_back`, `uart_set_line_inverse`, `_uart_set_pin*` banned everywhere;
  - R5: `uart_set_pin`, `uart_set_mode`, `uart_driver_install`, `uart_driver_delete`, `uart_param_config`, `uart_read_bytes` only in `components/rs485_port/`; `uart_set_mode` 2nd argument literally `UART_MODE_RS485_HALF_DUPLEX` or `UART_MODE_UART`;
  - R6: `CONFIG_DMT_BUS_TX_GPIO` / `CONFIG_DMT_BUS_DE_GPIO` only in `components/rs485_port/` and `main/`;
  - R7: `mb_link_write*` only in `mb_link/src/mb_link_write.c` and `mb_link/include/mb_link_write.h` (neither exists in phase 1);
  - R8: `MB_FUNC_WRITE*` nowhere in `components/` outside the write file; in `mb_link/` every `command` assignment is `0x03` / `MB_FUNC_READ_HOLDING_REGISTER` (FC06 only in the write file, compiled only with the gate on, phase 3);
  - `--self-test` and the scan run on every `idf.py build`; comments and strings are not exempt; `test/` and `test_host/` are skipped.
  - (v0.3 text was: `mbc_master_*` and headers in `components/mb_link/`; the UART calls incl. `uart_set_loop_back` allowed in `mb_link/` and `rs485_port/`; superseded by R1–R8.)
  - post-build ELF check fails if `mb_link_write` is present with the gate off.
- The v0.2 open point "user OK for extra zero-TX lint names" is moot.

### NVS wear
Mirrored Matter attributes without the NONVOLATILE flag; the esp_matter defaults for thermostat attributes are to be checked *(external)*; the node persists nothing else.

## 4. Hardware wiring

- Use one A and one B of "A A B B -". Doubled terminals = bridged in/out *(inferred, [[entities/ekwhctrl1]])*; continuity check with mains off (probe item 11).
- Site power (user, 2026-11-09): the node runs on normal USB power; the architect's proposal (laptop on battery / USB isolator until "-" is measured) was declined. Unchanged rule below. Whether the panel's low-voltage side is isolated from mains is not stated in the wiki (section 8).
- Node GND to "-" only after measuring "-" and A/B against node GND and finding them within −7..+12 V common mode *(external, RS485 standard)*. "-" is possibly shared with the CP presence contact *(inferred, [[entities/ekwhctrl1]], p3)*.
- The CP terminal and "+ A B -" are never touched.
- ESP TX -> DI: 3.3 V is fine, V_IH ≥ 2.0 V ([[sources/max485-datasheet]] p2).
- RO -> level shift / divider -> ESP RX, because RO is a 5 V output (D13, [[entities/max485]]), unless U1 turns out to be a 3.3 V part.
- **DE and /RE tied together to the RTS GPIO, 10 kΩ pull-down** (D8, Q1). Header RO/RE/DE/DI with separate DE and RE pins ([[sources/max485-module-photo]]); tying DE and /RE is the datasheet's send/receive switching (inferred from [[sources/max485-datasheet]] p10).
- Which signal drives DE in `UART_MODE_RS485_HALF_DUPLEX` on esp32s3 / IDF v6.0.2 (RTS or DTR) is verified in the task-002 spike *(external, open)*.
- **GPIO (kept from v0.2):** RX/TX/DE GPIO via Kconfig, defaults are placeholders: TX 5 / RX 4 / DE 6 (user, 2026-11-09). Avoid 0/3/45/46 (strapping), 19/20 (USB), 26–37 (flash 26–32, octal PSRAM 33–37 on R8), 43/44 (UART0) *(external; exact reserved set unconfirmed, no module datasheet in raw, see [[entities/esp32-s3-wroom-1]])*.
- **Termination (open):** measure A–B at the panel port (mains off, node disconnected). ≈ 120 Ω -> fit the user's separate 120 Ω at the node end; high/open -> start without and judge by the phase-1 error rate (a short 2-node run at 9600 has only minor reflections, *inferred*). Bias / fail-safe idle (module R5/R6/R8 unknown, [[sources/max485-module-photo]]) is assessed together with it. The MAX485 datasheet terminates both bus ends in the characteristic impedance ([[sources/max485-datasheet]] p13).

## 5. Matter mapping

D3/D4 re-approved (Q10). Details kept from v0.2 unless marked v0.3.

### EP1 Thermostat (device type 0x0301)
Features Heat + Cool, **no Auto**; ControlSequenceOfOperation 4 CoolingAndHeating *(external, CHIP data_model 1.4)*.
- **LocalTemperature** (nullable) <- 000 T1 × 10; null while unseen or stale. v0.3: stale = 3 × fast period = 15 s.
- **OccupiedHeatingSetpoint / OccupiedCoolingSetpoint** (non-nullable *(external)*) <-> 231 SP × 10, both mirror 231 and each other; seeded 2000 marked "default, not observed"; writes (P3) refused until 231 has been observed; round to 0.1 °C and republish both. P3 handles attribute writes and the SetpointRaiseLower command *(external)*. 008 = diagnostic only (231 is the write register, 008 the read-only real setpoint, p6, [[concepts/register-map]]); probe item 8 shows which one the panel moves.
- **Limits:** Min/Max Heat/Cool SetpointLimit from 202 SPL / 203 SPH, and 245 SPL_W / 246 SPH_W when 247 bit 5 is set; AbsMin/AbsMax 500/4000; respect 247 bit 4 ([[concepts/web-restriction-flags]]). Write clamp: section 3.
- **SystemMode:** 201 bit 7 Stby = 1 -> Off; 233 = 3 -> Heat, 5 -> Cool *(inferred from "inv"/"est", confirmed by probe item 8)*; 233 = 0 -> D15:
  - STAT 104 answers: Heat if STAT bit 1, Cool if STAT bit 0 (v0.2 D15 = b, unchanged) ([[concepts/flag-registers]]).
  - STAT unavailable (v0.3): report the last explicit season (default Heat) and raise the console flag "season auto, not mirrored".
  - Write (P3): Off -> RMW 201 bit 7 = 1; Heat/Cool -> clear bit 7 if needed; write 233 only if it differs from the actual mode; never "confirm" under 233 = 0.
  - Cool with hot water -> fan stops (STAT F.V. bits) = expected behaviour.

### EP2 Fan Control (device type 0x002B)
- **FanMode** <-> 201 bits 0–2: 000 Auto, 010 Night -> Low, 001 Silent -> Medium, 011 Maximum -> High (order *inferred* from the MVV limits, confirmed by probe item 8; bit values [[concepts/flag-registers]] p5); 100–111 -> unknown: keep the last value, raise a diagnostic, refuse fan writes.
- **No fan Off:** Fan Off write rejected (standby only via SystemMode); every FanModeSequence contains Off *(external)*, so controllers may still offer it. FanModeSequence const 2 (OffLowMedHighAuto).
- **PercentSetting** (nullable) derived: Low 33 / Med 66 / High 100 / Auto null; writes map to the nearest mode, 0 rejected. **PercentCurrent** derived from FanMode, not from 015; value under Auto/unknown decided in the fan-milestone spec.
- FanMode, PercentSetting, PercentCurrent, FanModeSequence are mandatory *(external)*.

### Not mapped / degradation
- 104 STAT, 009 OUT, 001 T2, 015 MOT_SET not mapped unless the probe shows they answer (except STAT for SystemMode under 233 = 0). 105 ALR_STAT diagnostic on the console. PRG bit 4 LOCK and 247 are not mapped.
- Starred registers are optional: the mapping degrades without failing when they are unavailable.

### Commissioning (D11, kept)
esp_matter BLE + Wi-Fi, dev DAC, test VID/PID 0xFFF1/0x8000, QR/pairing code on the console, BOOT long-press factory reset in the Matter app (P2). chip-tool is allowed for tests (D14).

## 6. Decisions

User decisions 2026-11-08 (session, not in raw) unless stated "kept" (then originally 2026-11-07, re-confirmed by the v0.3 approval).

| # | Status | Decision |
|---|---|---|
| D1 | changed | Tap "A A B B -", node is the sole master; "+ A B -" never touched (hard rule) |
| D2 | changed | Phases 1 probe / 2 read-only mirror / 3 writes; gate exception: phase-1 specs released before the on-site items are answered, phase-2/3 specs only after the probe log (Q8) |
| D3 | kept | Thermostat EP1 + Fan EP2, no fan Off; Thermostat first |
| D4 | changed | v0.2 mapping + degradation for starred registers; limits from 202/203/245/246 (section 5) |
| D5 | kept | Bus is truth, last writer wins, never re-assert |
| D6 | changed | Periodic FC03 polling in two groups, 5 s / 60 s; qty > 1 only if the probe proves it |
| D7 | changed | esp-modbus master via `mbc_master_send_request` is the only Modbus layer; sniffer/monitor dropped |
| D7a | retired | (was: private esp-modbus `mb_crc16` for the sniffer) |
| D7b | retired | (was: time-shared UART); esp-modbus owns the UART after the silence check |
| D8 | changed | RS485 half-duplex, RTS -> DE + /RE tied, 10 kΩ pull-down (Q1) |
| D9 | kept | Write whitelist 231, 201 bits 0–2/7 via RMW, 233; never 200 |
| D10 | changed | Single compile-time gate `CONFIG_DMT_BUS_WRITE`; debounce, caps, readback, ≤ 1 retry, latch |
| D11 | kept | Commissioning BLE + Wi-Fi, dev DAC, 0xFFF1/0x8000, BOOT long-press factory reset |
| D12 | changed | Host tests against a fake `mb_link` slave; probe logs become fixtures |
| D13 | kept+ | RO level shift, DE pull-down, all bus GPIO via Kconfig |
| D14 | kept | chip-tool allowed for tests |
| D15 | changed | 233 = 0 with STAT unavailable -> last explicit season (default Heat) + console flag; if STAT answers, v0.2 D15 = b unchanged (Q2) |
| D16 | new | Timing defaults, section 3 (Q3) |
| D17 | new | Boot silence check: RX-only 5 s at boot (60 s on demand) before esp-modbus starts; any byte -> refuse TX, latched (Q4) |
| D18 | new | 247 read-only permanently (Q5) |
| D19 | new | Slave address from Kconfig (default 1) + RAM-only console override in P1, no scan (Q6). See conflict below |
| D20 | new | Bus-access lint + ELF check (section 3) |
| D21 | new | Health ONLINE / DEGRADED / OFFLINE with backoff; never broadcast; addresses 1..247 only |
| D22 | new | Write caps / debounce values, section 3 (Q7) |

### Stage-1 detail decisions (user, 2026-11-09)

User statements in the session of 2026-11-09, not in raw; answers to the architect's stage-1 detail questions while the phase-1 specs were written.

| Topic | Decision | Architect proposal |
|---|---|---|
| Polling after boot | Starts **automatically** after a clean 5 s boot silence check (D17). `mb poll off/on` remain; `mb probe` and manual `mb read` also work while polling is stopped, without retry and without changing link health | wait for a console command (declined) |
| Site power | Normal USB power. Unchanged: node GND to "-" only after measuring "-" and A/B vs node GND (section 4) | laptop on battery / USB isolator until "-" is measured (declined) |
| Console | `mb addr <1-247>` (RAM-only override, D19), `mb log on\|off` | accepted |
| Boot window commands | Console commands typed during the 5 s boot silence check are executed before the first poll (`mb poll off` / `mb addr N` there stop or redirect the first frame) | approved |
| `mb silence <s>` | On-demand check: polling stays **off** after a passed check until `mb poll on` (deliberately different from boot) | approved |
| `mb log` default | Transaction lines **on** from power-up | off (user chose on) |
| Health | DEGRADED = 1–4 consecutive failures, OFFLINE at 5 (D21) | accepted |
| Other exceptions | Codes other than 02: no retry, back off to 60 s, not counted toward OFFLINE | accepted |
| Stale | 3 × group period: 15 s fast, 180 s slow | accepted |
| Bus-access lint | Stricter than v0.3 (D20): UART TX/RTS/DTR calls banned everywhere; `uart_set_pin` / `uart_set_mode` only in `rs485_port`; `mbc_master_*` only in `mb_link/src/`; full rules R1–R8 in `tasks/001-skeleton-kconfig-lint.md` (section 3) | accepted |
| Write code | No write file at all in phase 1; enabling `CONFIG_DMT_BUS_WRITE` fails the build until phase 3 (D10) | accepted |
| Host fake slave | Plugs in at the `poll_sched` link-function seam; `mb_link` not buildable for linux (D12) | accepted |
| Kconfig ranges | Safety caps: gap 50–1000 ms, timeout 200–2000 ms, retries 0–1, fast period ≥ 2 s (D16) | accepted |
| GPIO defaults | TX 5 / RX 4 / DE 6, placeholders (D13) | accepted |
| UART ISR | `CONFIG_UART_ISR_IN_IRAM=y` kept (resolves the section-8 gap) | accepted |
| New risk | An unbiased idle line may falsely trip the silence check -> stop, back to the architect (section 8) | accepted |

**Conflict (D19, recorded, not resolved):** the manual gives the address range 1–255 for reg 200 ([[concepts/device-addressing]], p4) while the Modbus spec reserves 248–255 *(external)*; the node accepts only 1..247. Also not stated: which port the address applies to, and "Address 001 – Leave free!" (p7) vs default 1 ([[concepts/device-addressing]]).

## 7. Task impact

Status 2026-11-09 (architect specs, `tasks/README.md`): 001, 009, 010, 002, 006, 011, 008 **spec ready**; 003/004/005/007 retired. The 002 spec now lives in `tasks/002-esp-modbus-master-spike-mb-link.md`; the old `tasks/002-esp-modbus-crc-adapter-spike.md` is a pointer, to be removed with `git rm` at commit.

| Task | Change |
|---|---|
| 001 | modify: new Kconfig (UART port, RX/TX/DE GPIO, baud, slave address, timeouts, poll periods, `CONFIG_DMT_BUS_WRITE` = n); lint -> `check_bus_access.py`; host runner unchanged |
| 002 | modify -> esp-modbus master spike + `mb_link` read path + `rs485_port` (CRC adapter dropped). Must establish: DE signal, silent create/start, error distinguishability incl. exception code, internal retries / t3.5, UART ownership and the order of `uart_set_pin` / `uart_set_mode` |
| 003 | retire |
| 004 | retire (ideas go into `reg_model` / `poll_sched`) |
| 005 | retire (RX-only listen survives as the silence check in `rs485_port`) |
| 006 | modify: poll loop + probe console |
| 007 | retire, replaced by 011 |
| 008 | modify: on-site probe checklist + log template, items 1–11 (section 1) |
| 009 | new: `daikin_regs` |
| 010 | new: `reg_model` + `poll_sched` |
| 011 | new: fake-slave host tests |

Order: 001, 009, 010, 002, 006, 011, 008 (009/010 may run in parallel with 002).

## 8. Open items and risks

- "Panel is slave" is inferred ([[entities/ekwhctrl1]]); if the silence check sees traffic or the panel never answers -> stop, back to the architect.
- Starred registers may be unavailable (no water temperature, no STAT, no OUT) ([[concepts/register-map]]).
- Not stated in the manual: port of the address, "Address 001 – leave free" vs default 1, timing, exception codes, write endurance, panel behaviour without polling (the 300 s timeout is probably irrelevant here, *inferred*, [[concepts/flag-registers]]).
- Shared "-" with CP: to measure.
- Whether the panel's low-voltage side ("A A B B -") is isolated from mains is not stated in the wiki; relevant because the node runs on normal USB power (user, 2026-11-09, section 4).
- Risk (user-accepted, 2026-11-09): an unbiased idle line (no fail-safe bias, module R5/R6/R8 unknown) may falsely trip the boot silence check -> stop, back to the architect *(inferred mechanism)*.
- Lint details (esp-modbus header location, `uart_set_loop_back`): resolved 2026-11-09 from the approved spec `tasks/001-skeleton-kconfig-lint.md` R2/R3/R4 (section 3).
- Termination / bias of the "A A B B -" segment; MAX485-M U1 / supply / RO level ([[entities/max485]], [[concepts/bus-topology]] items 3, 3a).
- FC03 qty > 1 unknown.
- Whether the panel enforces 202/203/245/246 is unknown -> the node enforces them.
- esp-modbus facts to verify in the task-002 spike *(external)*: DE signal, silent create/start, error granularity, internal retries, handling of addresses > 247.
- RMW race on 201 accepted.
- How Matter ecosystems render the Fan endpoint *(external)*.
- `CONFIG_UART_ISR_IN_IRAM`: resolved 2026-11-09, kept (section 6).
- Not addressed in the v0.3 text (wiki-keeper note, ask the architect/user): SystemMode under 233 = 0 when STAT answers but both or neither of bits 0/1 are set (undefined in v0.2, no v0.3 rule).

## Superseded v0.2 (2026-11-07)

v0.2 (architect, reviewed by `firmware-reviewer`, approved by the user 2026-11-07, not in raw) assumed a **listen-only sniffer on the "+ A B -" fancoil link**. Full text in git history. Its tap-point assumption was invalidated on 2026-11-08 (user: the node uses "A A B B -", [[concepts/bus-topology]]). v0.1 had a Kconfig + NVS runtime TX enable, already replaced in v0.2 by one compile-time gate.

| v0.2 decision | Replaced by (v0.3) |
|---|---|
| D1 listen on "+ A B -" first, TX path after P1 | D1: tap "A A B B -", sole master, "+ A B -" never touched |
| D2 P1 sniffer with DE and /RE hardwired to GND, DI unconnected, plain UART, no TX code in P1/P2; P1 specs as gate exception | D2: phase 1 read probe (FC03 only), write code compiled out; new gate exception (Q8) |
| D3 Thermostat + Fan, no fan Off | kept |
| D4 mapping | kept + starred-register degradation, limits 202/203/245/246 |
| D5 bus is truth, last writer wins, optimistic accept, revert if overridden by the master within ~2 cycles | kept as "bus is truth, last writer wins, never re-assert"; the node is now the master, local panel changes win on the next poll |
| D6 unseen values null, no polling, FC03 only before a write | D6: periodic FC03 polling 5 s / 60 s (null/"default, not observed" for unseen values kept in section 5) |
| D7 esp-modbus + custom `rtu_sniff` / `bus_monitor` / `bus_fmt` / `rs485_rx` | D7: esp-modbus master only; sniffer components dropped |
| D7a private `mb_crc16` with CMake/static-assert/ELF guards and H0/H1/F1/F2 host-test fallbacks | retired |
| D7b U2 single time-shared UART, blind during the transaction | retired |
| D8 P3 RS485 half-duplex, RTS -> DE + pull-down, /RE to GND | D8: DE + /RE tied to RTS, 10 kΩ pull-down, from phase 1 |
| D9 whitelist 231, 201 bits 0–2/7, 233 | kept (never 200; 247 never, D18) |
| D10 gate `CONFIG_APP_BUS_TX`, ≤ 2 retries, learned idle windows, ≥ 1 s pacing, latching kill switch | D10: gate `CONFIG_DMT_BUS_WRITE`, ≤ 1 retry, caps/debounce (D22), readback, latch |
| D11 commissioning | kept |
| D12 IDF linux target + Unity + replay fixtures from P1 captures | D12: fake `mb_link` slave, probe logs as fixtures |
| D13 RO level shift, DE pull-down, /RE to GND, GPIO via Kconfig | kept+, /RE now tied to DE (D8) |
| D14 chip-tool | kept |
| D15 = b Heat/Cool from STAT under 233 = 0 | kept when STAT answers; otherwise last explicit season (v0.3 D15) |
| Zero-TX lint `check_zero_tx.py` + "pending user OK" extra names; `bus_dump` task; P1 sniffer console `bus ...`; P3 idle-window outline (U2 sequence, window sizing) | replaced by `check_bus_access.py` (D20), probe console `mb ...`; window logic no longer applies (sole master) |
| Note: our writes may reset the 300 s Com-err watchdog (STAT bit 12) | probably not about this port *(inferred, low-medium, [[concepts/flag-registers]])* |
