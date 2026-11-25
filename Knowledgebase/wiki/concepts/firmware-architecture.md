---
title: Firmware architecture v0.3 (approved 2026-11-08, user)
type: concept
sources: ["project-brief.md", "EKWHCTRL0.EKWHCTRL1.EKRTCTRL1_Installation manual_N420384A_English.pdf", "EKWHCTRL1,EKRTCTRL1,EKWHCTRL0_Installation manual_N420384A ModBus RTU_English.pdf", "MAX485-M_EN_10035508.pdf", "20261107_192127.jpg"]
updated: 2026-11-25
---

# Firmware architecture (approved v0.3)

**Status:** v0.3 written by `architect`, **approved by the user on 2026-11-08** (Q1–Q10 all answered with the architect's recommendation; user decision, session 2026-11-08, not in raw). Supersedes v0.2 (2026-11-07). Later user decisions (sessions 2026-11-09 and 2026-11-24, not in raw) are in section 6 and are applied in sections 1–5. The architecture as a whole, every D-number and every default value are user decisions unless tagged otherwise. **Slimmed 2026-11-24** (user decision S2, session 2026-11-24): progress narratives, verification output, review-round details, superseded texts, closed questions and the superseded v0.2 table moved verbatim to [[concepts/firmware-history]]. Header addenda history: [[concepts/firmware-history]] §1; pre-slimming wording of the sections kept here: §10.

Tags: *(external)* = Modbus spec, ESP registry, esp-modbus / IDF / esp_matter / CHIP sources, RS485 standard; not in raw. *(project evidence)* = project document `docs/spikes/002-esp-modbus.md` (task-002 spike report, 2026-11-24, source reading of esp-modbus 2.1.4 and IDF v6.0.2, no hardware attached), verified by `firmware-reviewer` and the orchestrator; not a raw source; file:line references are into esp-modbus (`EM/` = `firmware/managed_components/espressif__esp-modbus/modbus/`, `EMR/` = its repo root) or IDF (`IDF/` = `~/esp/esp-idf-v6.0.2/components/`) as in that report. *(inferred)* = design conclusion, not stated in any source. *Session facts* = verified tool output reported in the session, not in raw. Register facts cite [[concepts/register-map]], [[concepts/flag-registers]], [[concepts/web-restriction-flags]], [[concepts/device-addressing]] (all from [[sources/n420384a-english]] p3–p7, values identical in [[sources/n420384a-modbus-rtu]]). Context: [[concepts/project-goal]], [[concepts/bus-topology]], [[entities/ekwhctrl1]], [[concepts/dev-environment]]. Task specs: `tasks/README.md`.

Q1–Q10 as answered (user, 2026-11-08): Q1 DE + /RE tied to RTS with 10 kΩ pull-down (D8); Q2 233 = 0 with STAT unavailable -> last explicit season, default Heat, console flag "season auto, not mirrored" (D15); Q3 timing defaults accepted (D16); Q4 boot silence check yes (D17); Q5 247 read-only permanently (D18); Q6 address from Kconfig default 1 + RAM console override, no scan (D19); Q7 write caps accepted (D22); Q8 gate exception: stage-1 (probe) specs before the on-site items, stage-2/3 specs only after the probe log (D2; **partly superseded 2026-11-24** by the phase-2a gate change, section 6 "Working mode and gate"); Q9 esp-modbus `==2.1.4` (**superseded 2026-11-24**: dependency removed with E2); Q10 Thermostat + Fan mapping (D3/D4) re-approved.

## 1. Phases and exit criteria

Tap point and role (D1): the node connects to one A and one B of the EKWHCTRL1 **"A A B B -"** ModBus RTU port and is the **sole Modbus master** there; the **"+ A B -"** fancoil link is never touched (hard rule) ([[concepts/bus-topology]], [[entities/ekwhctrl1]], [[sources/n420384a-english]] p3). Framework rules: ESP-IDF v6.0.2, esp32s3, registry-only components ([[sources/project-brief]], [[concepts/dev-environment]]).

### Phase 1 – on-site read probe
No Matter, no Wi-Fi, FC03 only, write code compiled out (`CONFIG_DMT_BUS_WRITE=n`, section 3). Polling starts automatically after a clean 5 s boot silence check (user, 2026-11-09, section 6). Results go into a **measurement log** -> `Knowledgebase/raw/` -> `/capture`. Exit criteria:

1. Panel slave address confirmed: by the user on the panel (standby, hold AUTO 5 s, wait 10 s without changing; procedure from [[concepts/device-addressing]], p6) and by a bus reply.
2. A/B polarity: valid replies at 9600 8N1 ([[concepts/modbus-rs485-link]]).
3. Silence check saw no foreign traffic on "A A B B -" (tests the inferred "panel is slave", [[entities/ekwhctrl1]]).
4. Table of **every** documented register, starred and unstarred (p4, [[concepts/register-map]]): OK with raw/scaled value, or exception + code, or timeout.
5. Does FC03 with quantity > 1 work (e.g. 200..203 qty 4)? Not stated in the manual ([[concepts/modbus-rs485-link]]).
6. Response time min/median/max over ≥ 1000 transactions. Answered by the whole-transaction time `dur_ms` plus a derived upper bound for the slave turnaround of about `dur_ms` - 34 ms, logged "derived, not measured"; pure turnaround "unknown, no instrument"; sanity check: OK qty-1 `dur_ms` ≥ about 36 ms (F2, J8, user decision, session 2026-11-24; earlier bound `dur_ms` - 25 ms and derivation: [[concepts/firmware-history]] §2, §6).
7. ≥ 1 h polling at the proposed rate (section 3), error rate < 1 %, no panel anomaly (display, icons, error codes, local keys).
8. Local keypress correlation: setpoint up/down (231 vs 008), each fan mode (order of 201 bits 0–2), season (233 values 3/5), standby (201 bit 7).
9. Does the panel show anything when polled (e.g. WEB icon, 247 flag 0, [[concepts/web-restriction-flags]])?
10. Panel behaviour when polling stops > 300 s and resumes (display, 105 bit 0, 104 bit 12 if readable; [[concepts/flag-registers]]).
11. Measurements: A–A and B–B continuity, A–B resistance at the panel port with mains off, "-" vs node GND voltage, module U1 / supply / RO level ([[entities/max485]]), whether "-" is shared with CP.

The user verifies on site items 1, 2, 7, 8, 9, 10, 11 and that "+ A B -" was untouched.

### Phase 2 – Matter read-only mirror
Thermostat endpoint first, then Fan (D3); Matter writes rejected. Exit: 24 h without panel anomaly; values match the panel display; bus unplugged -> attributes go stale/null within the threshold (section 5) and recover.
- **Phase 2a (user decision, session 2026-11-24; plan approved, section 6 "Phase-2a plan"):** separate IDF project `firmware/matter/` (own manifest with `espressif/esp_matter` ^1.6.1, own `sdkconfig.defaults` and partition table for 16 MB flash (16 MB inferred from N16R8), BLE commissioning per D11); reuses `daikin_regs` and `reg_model` via `EXTRA_COMPONENT_DIRS`; **no bus code** (no `bus_task`, `mb_link`, `rs485_port`, `mb_rtu`), DE not driven (pull-down holds receive). Thermostat EP1 read-only from a simulated `sim_source`; Matter writes rejected; Fan EP2 waits for 2b. The phase-1 probe project stays byte-for-byte unchanged.
  - Acceptance controllers: chip-tool (reference), Home Assistant (Core + separate `python-matter-server`), Apple Home (cross-check); SmartThings on site later (2b/3) ([[concepts/dev-environment]]).
- **Phase 2b:** the real Modbus <-> Matter mapping and anything touching the bus, only after the probe log is in `raw/` and captured.

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
| `mb_rtu` | pure | P1 | own minimal RTU framing (E2): CRC16, FC03 request builder, reply classifier OK / EXCEPTION + code / BAD_FRAME / TIMEOUT; host-tested; the only place with Modbus function-code bytes (FC03 only in phase 1) |
| `mb_link` | HW | P1 | `mb_link_read(addr, reg, qty, out, *status)` (API unchanged by E2), built on `mb_rtu` + `rs485_port`; `mb_link_write` only with `CONFIG_DMT_BUS_WRITE`; rejects addr 0 and anything outside 1..247 *(external, Modbus spec)*; hard-coded write whitelist as last line of defence; `mb_link_get_counters` (gap bytes, port faults, max inter-byte interval; copy of the `rs485_port` counters) shown in `mb status`; `mb_link_counters_t` carries a `measured` flag (true under E2; false, zeros and `link counters: n/a` under E1b) (user, 2026-11-24, spec 014) |
| `rs485_port` | HW | P1 | owns the IDF UART (E2) in RS485 half-duplex mode, DE via RTS; pins; boot silence check (D17); the only place allowed to call `uart_write_bytes` (section 3) |
| `bus_task` + probe console | glue | P1 | single owner of `mb_link`; poll loop; write queue (P3); commands `mb status`, `mb regs`, `mb read <reg> [qty]`, `mb probe`, `mb poll on\|off`, `mb silence <s>`, `mb stats`, `mb addr <1-247>`, `mb log on\|off` (`mb log` default **on**; user, 2026-11-09) |
| `mapping` | pure | P2 | register <-> Matter conversion (section 5) |
| `matter_bridge` | esp_matter | P2 | endpoints; rejects writes in PRE_UPDATE *(external)*; in P3 turns them into write requests |
| `write_guard` | pure | P3 | Matter intent -> FC06 operations incl. RMW of 201, limits, debounce/caps, readback check, latch (section 3) |

Seam: `mb_link.h` is the only hardware interface. Host tests (D12): the scripted fake slave plugs in at the `poll_sched` link-function seam, because `mb_link` is not buildable for the linux target (user, 2026-11-09).

### Modbus layer (E2, user, 2026-11-24; supersedes D7)
**Own minimal FC03-only RTU master, no esp-modbus** (user decision, session 2026-11-24, not in raw): `mb_rtu` (pure, host-tested; four outcomes OK / EXCEPTION + code / BAD_FRAME / TIMEOUT, as D21 needs); `rs485_port` owns the UART in `UART_MODE_RS485_HALF_DUPLEX`, DE via RTS (section 4); `mb_link` API unchanged. Reason: esp-modbus 2.1.4 cannot distinguish a slave exception from a bad reply and drops the exception code (spike S3) *(project evidence)*. Rejected: E1a, E3. **Fallback E1b (not built):** esp-modbus `==2.1.4` with every invalid reply = BAD_FRAME, exc 0, retry once, counts toward OFFLINE; only if E2 proves too hard to debug on site; revert is a **separate user decision** and replaces only `mb_link` internals; losses: exception-02 unsupported detection (D21), exception codes (U2/U3/U5), probe items 4/5 weakened. Details and the superseded D7 text: section 6 "Task 002 spike decisions", [[concepts/firmware-history]] §3, §5.

Dependencies: `espressif/esp_matter` ^1.6.1 (P2) *(external, registry)*; IDF built-ins `esp_driver_uart`, `esp_driver_gpio`, `console`, `esp_timer`, `freertos`, `nvs_flash` (Matter only), `unity`. esp-modbus removed with E2. Registry-only rule: own project code adds no dependency, so it is compliant (user-confirmed interpretation, session 2026-11-24, [[concepts/dev-environment]]).

### Threading
- `bus_task`: priority ~10, pinned to core 1 *(inferred)*; the only caller of `mb_link`; 4 KB stack (to verify on site, section 8).
- `matter_bridge` takes `reg_model` snapshots and updates attributes in the Matter context; `bus_task` never takes the CHIP lock.
- P3: write intents go through a queue of depth 4; newest intent per register wins.
- `CONFIG_UART_ISR_IN_IRAM=y` kept (user, 2026-11-09); load-bearing for DE (section 3).
- **Console rule:** console REPL on USB-Serial-JTAG / UART0, never on the bus UART (compile-time `#error` + runtime check before `bus_task` starts, T4).

## 3. Bus-safety rules

The manual states no timing, polling interval, response time or exception codes ([[entities/ekwhctrl1]], [[concepts/bus-topology]]); all defaults below are therefore *(inferred)* design values (D16), to be re-tuned from the probe log.

### Timing (D16)
- One transaction at a time; gap ≥ 100 ms after each (end-to-start; enforced in the host fake slave since task 011, G1).
- Reference: t3.5 ≈ 4.0 ms at 9600 baud with 11-bit characters *(external, Modbus serial-line spec)*.
- **Gap bytes** (bytes received before a request): counted in the `rs485_port` counters and discarded; the request is still sent (user, 2026-11-24).
- `CONFIG_FREERTOS_HZ` stays at **100 Hz** (1000 Hz rejected, user, 2026-11-24); end-of-frame silence window **about 20–30 ms** (`rs485_port.c:413`: (ceil(t3.5 / tick) + 1) × tick = 20 ms from the last byte, plus up to one tick; session facts) is a known, accepted limit. **Conflict (recorded, resolved by the user 2026-11-24):** earlier spec/wiki text said 10–20 ms; code stays, texts corrected (H5; [[concepts/firmware-history]] §4, §6).
- **`CONFIG_UART_ISR_IN_IRAM=y` is load-bearing for DE:** DE is released in the UART TX_DONE ISR (section 4); a flash/NVS stall could otherwise hold DE high. Build guard M2 (`#error` if off).
- **Counters are approximate (M3):** `gap_bytes` is a lower bound, `max_byte_interval_us` approximate; quote only as bounds in the probe log.
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
- Exception 02 -> register `unsupported` until reboot, not polled again; applies to POLL, RETRY, SWEEP and MANUAL qty-1 reads (U3); **not HEALTH**: HEALTH exc 02 on 000 = failed read (U5).
- Other exception codes (user, 2026-11-09): no retry, back off to 60 s, not counted toward OFFLINE; recorded like a failed read, not unsupported (U2).
- 1–4 consecutive failures -> **DEGRADED**; 5 -> **OFFLINE**: automatic traffic only 000 every 30 s (manual `mb read` still allowed, U1); first success -> **ONLINE**.
- Stale = 3 × group period: 15 s fast, 180 s slow (user, 2026-11-09).
- A reply carrying exception code **0x00** is classified **BAD_FRAME** (user, 2026-11-24).
- The sweep (`mb probe`) is suspended while OFFLINE and cancelled by `mb poll off` / `mb silence` / `mb addr` (`poll_sched.c` rule 8; H3, session facts).
- Never broadcast; addresses 1..247 only *(external, Modbus spec)*.

### Fallbacks
- Console `mb poll off` / `mb poll on`. Polling starts automatically after a clean boot silence check (user, 2026-11-09). `mb probe` and `mb read` also work while polling is stopped, without retry and without changing link health. `mb read` also works while OFFLINE (U1). `mb poll on` is a no-op unless STOPPED; `mb addr N` resets counter, unsupported marks and pending work (U4). `mb silence` cancels a pending manual read (T3).
- Boot silence check (D17): RX-only for 5 s at boot (60 s on demand via `mb silence <s>`, 0 ms rejected, M4) **before** the master transmits; any received byte -> refuse to transmit, latched until reboot; no read before a clean check (006-c); failed hold-receive at boot is fatal (006-b).
  - Console commands typed during the 5 s boot window are executed before the first poll (user, 2026-11-09).
  - After a passed on-demand `mb silence <s>`, polling stays off until `mb poll on` (user, 2026-11-09).
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
- `CONFIG_DMT_BUS_WRITE` default **n**; when off, `mb_link_write` and `write_guard` are not compiled. Phase 1: no write source file exists at all; enabling the gate fails the build until phase 3 (user, 2026-11-09).
- `tools/check_bus_access.py` runs `--self-test` and the tree scan on every `idf.py build`; comments and strings are not exempt; `test/` and `test_host/` are skipped. Base rule set R1–R8 from `tasks/001-skeleton-kconfig-lint.md`, rewritten for E2 by task 013 (L1–L9, A1–A12). Rules in force (session facts; exact text in `firmware/tools/check_bus_access.py` and spec 013):
  - No esp-modbus anywhere (no `mbc_*` / `mbm_*` / `mbs_*` names, no esp-modbus headers incl. comment, digraph and `#import` forms; manifests match `esp[-_]modbus`) (R1–R3).
  - R4: `uart_write_bytes` only in `components/rs485_port/src/`; banned everywhere incl. `rs485_port`: `uart_tx_chars`, `uart_set_rts`, `uart_set_dtr`, `uart_set_loop_back`, `uart_set_line_inverse`, `_uart_set_pin*`, `uart_ll_*`, `uart_hal_*`, `esp_rom_uart_tx_one_char`, `uart_tx_one_char`, `esp_rom_output_*`, `esp_rom_uart_*`, `esp_rom_install_*` (no blanket `esp_rom_*` ban), raw `UARTn.` / `UARTn->` access, `REG_WRITE` / `WRITE_PERI_REG` / `REG_READ` / `READ_PERI_REG` on `UART_FIFO*_REG` (200-character window), and the `uart_struct.h` / `uart_ll.h` / `uart_hal.h` / `uart_reg.h` includes.
  - R5: UART setup calls only in `components/rs485_port/` (`uart_set_pin`, `uart_set_mode` with 2nd argument literally `UART_MODE_RS485_HALF_DUPLEX` or `UART_MODE_UART`, `uart_driver_install`, `uart_driver_delete`, `uart_param_config`, `uart_read_bytes`, plus `uart_set_hw_flow_ctrl`, `uart_set_baudrate`, `uart_set_parity`, `uart_set_word_length`, `uart_set_stop_bits`, `uart_set_tx_idle_num`, `uart_enable_tx_intr`, `uart_flush`, `uart_intr_config`, `uart_isr_register`); `uart_flush_input` allowed.
  - R6: `CONFIG_DMT_BUS_TX_GPIO` / `CONFIG_DMT_BUS_DE_GPIO` only in `components/rs485_port/` and `main/`. R7: `mb_link_write*` only in `mb_link/src/mb_link_write.c` / `mb_link/include/mb_link_write.h` (neither exists in phase 1).
  - R8: function-code bytes only in `mb_rtu`, only 0x03 in phase 1; `++` / `--` on function-code fields banned; deterministic comment regex. In `mb_link/` and `rs485_port/` names `fc`, `func`, `command` and strings such as `"fc=%d"` fail R8; FC-valued hex constants must be decimal or named (L9).
  - Documented limits (L8, A9–A11, A14): parenthesis nesting; FC arithmetic not evaluated; escapes not decoded; CMake/Kconfig not scanned; the single `uart_write_bytes` site checked by inspection; nested-bracket prefix `++a[b[1]].fc`; quadratic R5 helper.
- Post-build ELF check `tools/check_elf_symbols.py` (POST_BUILD, `xtensa-esp32s3-elf-nm`): forbids all `mbc_` / `mbm_` / `mbs_` symbols and `mb_link_write`; **fails** on empty `nm` output or missing `app_main` (L5); required symbols `mb_link_read`, `mb_rtu_build_fc03` since task 006. **Correction (session fact, verified by the orchestrator in `firmware/tools/check_elf_symbols.py:22`, local inspection, 2026-11-24):** the pattern is only `^(mbc_|mbm_|mbs_|mb_link_write)`; "no Wi-Fi / lwip / Matter linked" in the probe image was a manual `build-verifier` acceptance item, **not enforced by tooling**.
- Phase-2a Matter image: a **separate new** check script in `firmware/matter/` forbids `mb_link_` / `rs485_port_` / `mb_rtu_` / `bus_task_` / `mbc_` symbols; no phase-1 file touched (user decision, session 2026-11-24).
- Superseded task-001 R1–R8 text and v0.3 lint wording: [[concepts/firmware-history]] §4.

### NVS wear
Mirrored Matter attributes without the NONVOLATILE flag; the esp_matter defaults for thermostat attributes are to be checked *(external)*; the node persists nothing else. **2026-11-25:** esp_matter 1.6.1 creates OccupiedHeating/CoolingSetpoint, SystemMode and ControlSequenceOfOperation as NONVOLATILE and the public API cannot clear it (task 015 finding, session fact, *external*, not in raw); handled by decision 2a-10 (section 6).

## 4. Hardware wiring

- Use one A and one B of "A A B B -". Doubled terminals = bridged in/out *(inferred, [[entities/ekwhctrl1]])*; continuity check with mains off (probe item 11).
- Site power (user, 2026-11-09): normal USB power; the architect's proposal (laptop on battery / USB isolator until "-" is measured) was declined. Whether the panel's low-voltage side is isolated from mains is not stated in the wiki (section 8).
- Node GND to "-" only after measuring "-" and A/B against node GND and finding them within −7..+12 V common mode *(external, RS485 standard)*. "-" is possibly shared with the CP presence contact *(inferred, [[entities/ekwhctrl1]], p3)*.
- The CP terminal and "+ A B -" are never touched.
- ESP TX -> DI: 3.3 V is fine, V_IH ≥ 2.0 V ([[sources/max485-datasheet]] p2).
- RO -> level shift / divider -> ESP RX, because RO is a 5 V output (D13, [[entities/max485]]), unless U1 turns out to be a 3.3 V part.
- **DE and /RE tied together to the RTS GPIO, 10 kΩ pull-down** (D8, Q1). Header RO/RE/DE/DI with separate DE and RE pins ([[sources/max485-module-photo]]); tying DE and /RE is the datasheet's send/receive switching (inferred from [[sources/max485-datasheet]] p10).
- **DE signal (task-002 spike S5, *project evidence*, source reading, no hardware):** in `UART_MODE_RS485_HALF_DUPLEX` on esp32s3 / IDF v6.0.2 **RTS** drives DE: **low while idle, high during TX** (`IDF/esp_hal_uart/esp32s3/include/hal/uart_ll.h:773-783`; `IDF/esp_driver_uart/src/uart.c:1191` raises it before the TX FIFO is filled, `:1506` drops it in the TX_DONE ISR) ([`docs/spikes/002-esp-modbus.md`](../../../docs/spikes/002-esp-modbus.md) S5).
- **Pin order (DE safety, user-approved 2026-11-24):** `uart_set_mode(UART_MODE_RS485_HALF_DUPLEX)` **first**, then `uart_set_pin` (TX, RX, DE as RTS). Reason: `sw_rts` resets to 0 (`IDF/soc/esp32s3/register/soc/uart_struct.h:517`) and the RTS pin follows its inverse, so routing RTS first drives DE **high** in between *(project evidence, S5)*. The spec 002 interface comment had the reverse order.
- **DE re-drive after driver delete (DE safety, user-approved 2026-11-24):** `uart_driver_delete` -> `uart_release_pin` disables the RTS output (`uart.c:2116`); DE then floats on the 10 kΩ pull-down. `rs485_port` writes the DE GPIO level **0 before** `gpio_output_enable` (`IDF/esp_driver_gpio/src/gpio.c:228`) *(project evidence, S5)*. Both DE rules are in the code-inspection checklists.
- **DE timing facts (task 014 review, `firmware-reviewer`, project evidence from the local IDF v6.0.2 source, not verified on hardware):** DE is high only during our own 8-byte request; RTS asserted before the TX FIFO fill (`uart.c:1191`), released in the TX_DONE ISR (`uart.c:1506`); half-duplex mode sets `sw_rts` = 1, i.e. RTS low (`uart_ll.h:773-783`); mode before pins; DE driven low before output enable (`gpio.c:228-229`); no reply byte is lost to the ISR RX-FIFO reset (`uart.c:1505`), because the receiver is off while DE is high. DE release depends on the ISR (section 3).
- **GPIO:** RX/TX/DE via Kconfig, placeholder defaults TX 5 / RX 4 / DE 6 (user, 2026-11-09). Avoid 0/3/45/46 (strapping), 19/20 (USB), 26–37 (flash 26–32, octal PSRAM 33–37 on R8), 43/44 (UART0) *(external; exact reserved set unconfirmed, no module datasheet in raw, see [[entities/esp32-s3-wroom-1]])*.
- **Termination (open):** measure A–B at the panel port (mains off, node disconnected). ≈ 120 Ω -> fit the user's separate 120 Ω at the node end; high/open -> start without and judge by the phase-1 error rate (a short 2-node run at 9600 has only minor reflections, *inferred*). Bias / fail-safe idle (module R5/R6/R8 unknown, [[sources/max485-module-photo]]) is assessed together with it. The MAX485 datasheet terminates both bus ends in the characteristic impedance ([[sources/max485-datasheet]] p13).

## 5. Matter mapping

D3/D4 re-approved (Q10). Details kept from v0.2 unless marked v0.3. Phase 2a feeds EP1 only, read-only, from a simulated `sim_source` (section 6 "Phase-2a plan"); the mapping is provisional there (233 values, 231 vs 008, starred registers wait for the probe); the real bus mapping is phase 2b (section 1).

### EP1 Thermostat (device type 0x0301)
Features Heat + Cool, **no Auto**; ControlSequenceOfOperation 4 CoolingAndHeating *(external, CHIP data_model 1.4)*.
- **LocalTemperature** (nullable) <- 000 T1 × 10; null while unseen or stale (stale = 3 × fast period = 15 s).
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

All rows are user decisions (session of the stated date, not in raw) unless tagged otherwise. Full original text of the 2026-11-24 task subsections, with verification output and review details: [[concepts/firmware-history]] §5, §6.

### Core decisions D1–D22 (user, 2026-11-08; "kept" = 2026-11-07, re-confirmed)

| # | Status | Decision |
|---|---|---|
| D1 | changed | Tap "A A B B -", node is the sole master; "+ A B -" never touched (hard rule) |
| D2 | changed | Phases 1 probe / 2 read-only mirror / 3 writes; phase-1 specs before the on-site items, phase-2/3 specs only after the probe log (Q8). **Amended 2026-11-24:** phase 2a (bus-independent) may start now (section 6 "Working mode and gate") |
| D3 | kept | Thermostat EP1 + Fan EP2, no fan Off; Thermostat first |
| D4 | changed | v0.2 mapping + degradation for starred registers; limits from 202/203/245/246 (section 5) |
| D5 | kept | Bus is truth, last writer wins, never re-assert |
| D6 | changed | Periodic FC03 polling in two groups, 5 s / 60 s; qty > 1 only if the probe proves it |
| D7 | **superseded 2026-11-24** | (was: esp-modbus master is the only Modbus layer; sniffer/monitor dropped). Now E2: own FC03-only RTU master, no esp-modbus; sniffer/monitor stay dropped |
| D7a | retired | (was: private esp-modbus `mb_crc16` for the sniffer) |
| D7b | retired | (was: time-shared UART); `rs485_port` owns the UART (E2) |
| D8 | changed | RS485 half-duplex, RTS -> DE + /RE tied, 10 kΩ pull-down (Q1) |
| D9 | kept | Write whitelist 231, 201 bits 0–2/7 via RMW, 233; never 200 |
| D10 | changed | Single compile-time gate `CONFIG_DMT_BUS_WRITE`; debounce, caps, readback, ≤ 1 retry, latch |
| D11 | kept | Commissioning BLE + Wi-Fi, dev DAC, 0xFFF1/0x8000, BOOT long-press factory reset |
| D12 | changed | Host tests against a fake `mb_link` slave; probe logs become fixtures |
| D13 | kept+ | RO level shift, DE pull-down, all bus GPIO via Kconfig |
| D14 | kept | chip-tool allowed for tests |
| D15 | changed | 233 = 0 with STAT unavailable -> last explicit season (default Heat) + console flag; if STAT answers, v0.2 D15 = b (Q2) |
| D16 | new | Timing defaults, section 3 (Q3) |
| D17 | new | Boot silence check: RX-only 5 s at boot (60 s on demand) before the master transmits; any byte -> refuse TX, latched (Q4) |
| D18 | new | 247 read-only permanently (Q5) |
| D19 | new | Slave address from Kconfig (default 1) + RAM-only console override in P1, no scan (Q6). See conflict below |
| D20 | new | Bus-access lint + ELF check (section 3) |
| D21 | new | Health ONLINE / DEGRADED / OFFLINE with backoff; never broadcast; addresses 1..247 only |
| D22 | new | Write caps / debounce values, section 3 (Q7) |

Superseded v0.2 decisions and their v0.3 replacements: [[concepts/firmware-history]] §9.

### Working mode and gate (user, 2026-11-24)

User decisions, session 2026-11-24, not in raw. Goal: faster development, lower token use.

| # | Decision |
|---|---|
| S1 | The wiki is updated **once per task, at acceptance** (decisions and verified facts only). Step-by-step progress goes to `RESUME.md` and git, not the wiki. `RESUME.md` is still updated after every step |
| S2 | Slim the wiki: progress diary moved from this page to [[concepts/firmware-history]] (done 2026-11-24) |
| S3 | Full independent `firmware-reviewer` only for **bus-touching** code (bus timing, polling, writes, UART/DE). Other code (Matter, Wi-Fi, UI) gets `build-verifier` plus **one review at the end of the feature**. Text-only fixes get no re-review loop |
| S4 | Fewer, bigger tasks with shorter specs (one per feature); `firmware-dev` on sonnet by default, haiku for small edits |
| S5 | The orchestrator batches open questions **once per task** and takes the recommended defaults for minor items |
| Gate | **Phase 2a may start now:** bus-independent part only (Matter skeleton: esp_matter, Wi-Fi commissioning, Thermostat endpoint fed by a fake/simulated register model; no new bus code, no writes). The real Modbus <-> Matter mapping and anything touching the bus (phase 2b) still wait for the probe log in `raw/` + `/capture`. Supersedes the earlier rule "stage 2/3 specs only after the probe log is in raw/ and captured" (D2, Q8) **for the bus-independent part only**. Phase-2a plan **approved** 2026-11-24 (next subsection) |

### Phase-2a plan (user decisions, session 2026-11-24; approved)

| # | Decision |
|---|---|
| 2a-1 | Separate IDF project `firmware/matter/`: own manifest (`espressif/esp_matter` ^1.6.1), own `sdkconfig.defaults`, own partitions for 16 MB flash (16 MB *inferred* from N16R8); BLE commissioning, test DAC, VID 0xFFF1 / PID 0x8000 (D11) |
| 2a-2 | Reuses `daikin_regs` and `reg_model` via `EXTRA_COMPONENT_DIRS`; **no bus code** (`bus_task`, `mb_link`, `rs485_port`, `mb_rtu` absent); DE not driven, pull-down holds receive. Phase-1 probe project unchanged byte for byte |
| 2a-3 | Scope: Thermostat EP1 (0x0301) read-only: LocalTemperature, both occupied setpoints, setpoint limits, SystemMode Off/Heat/Cool. Fan EP2 waits for 2b. Matter writes rejected |
| 2a-4 | Values from a simulated `sim_source` feeding a `reg_model`; mapping provisional (233 values, 231 vs 008, starred registers wait for the probe) |
| 2a-5 | Acceptance controllers: chip-tool (reference), Home Assistant (Core + separate `python-matter-server`), Apple Home (cross-check). SmartThings acceptance on site later (2b/3), incl. test-device registration |
| 2a-6 | Bus guard: separate new check script in the Matter project forbidding `mb_link_` / `rs485_port_` / `mb_rtu_` / `bus_task_` / `mbc_` symbols; no phase-1 file touched |
| 2a-7 | Tasks 2a-A "Matter skeleton" (step 0 stop: esp_matter must resolve and build on IDF v6.0.2 from the registry only, no build-time fetch of non-registry code; else escalate) and 2a-B "simulated model + provisional read-only mapping + sim console" (ids pending, specs by the `architect`) |
| 2a-8 | Roles: `firmware-dev` sonnet, `build-verifier` per task, one `firmware-reviewer` review at feature end (S3) |
| 2a-9 | Q-HW2 (flash/NVS DE run) belongs to phase 2b: the 2a image has no bus code |
| 2a-10 | (2026-11-25, "Accept + limit") Keep the esp_matter NONVOLATILE default for the setpoints, SystemMode and ControlSequenceOfOperation (section 3 "NVS wear"); task 016 updates attributes **only on change** and uses `attribute::set_deferred_persistence`; the `architect` adds a flash-wear estimate to spec 016. Consequence *(inferred)*: one NVS write per value change (deferred); at boot the stored value overrides the initial value until the first update |

### Stage-1 detail decisions (user, 2026-11-09)

Answers to the architect's stage-1 detail questions (session 2026-11-09, not in raw).

| Topic | Decision (architect proposal if declined) |
|---|---|
| Polling after boot | Starts **automatically** after a clean 5 s boot silence check (D17); `mb poll off/on` remain; `mb probe` and `mb read` work while stopped, no retry, no health change (declined: wait for a console command) |
| Site power | Normal USB power; node GND to "-" only after measuring (declined: battery laptop / USB isolator) |
| Console | `mb addr <1-247>` (RAM-only, D19), `mb log on\|off` |
| Boot window commands | Commands typed during the 5 s boot check run before the first poll |
| `mb silence <s>` | Polling stays **off** after a passed on-demand check until `mb poll on` |
| `mb log` default | **On** from power-up (proposal was off) |
| Health | DEGRADED = 1–4 consecutive failures, OFFLINE at 5 (D21) |
| Other exceptions | Codes other than 02: no retry, back off to 60 s, not counted toward OFFLINE |
| Stale | 3 × group period: 15 s fast, 180 s slow |
| Bus-access lint | Stricter than v0.3 (D20); rules R1–R8 per `tasks/001-skeleton-kconfig-lint.md` (now as rewritten by 013, section 3) |
| Write code | No write file in phase 1; enabling `CONFIG_DMT_BUS_WRITE` fails the build until phase 3 (D10) |
| Host fake slave | At the `poll_sched` link-function seam; `mb_link` not buildable for linux (D12) |
| Kconfig ranges | gap 50–1000 ms, timeout 200–2000 ms, retries 0–1, fast period ≥ 2 s (D16) |
| GPIO defaults | TX 5 / RX 4 / DE 6, placeholders (D13) |
| UART ISR | `CONFIG_UART_ISR_IN_IRAM=y` kept |
| New risk | An unbiased idle line may falsely trip the silence check -> stop, back to the architect (section 8) |

### Task 010 detail decisions (user, 2026-11-24)

Answers to spec ambiguities in `tasks/010-reg-model-poll-sched.md`; U1–U4 recommended options, U5–U6 architect proposals confirmed by the user. Spec 010 amended by the architect 2026-11-24.

| # | Decision |
|---|---|
| U1 | `mb read` (MANUAL) allowed while OFFLINE: single read, no retry, gap respected, no effect on health or the HEALTH schedule; "only HEALTH reads" applies to automatic traffic |
| U2 | Exception codes other than 02 do **not** mark unsupported in `reg_model`; recorded like a failed read (last value kept, ages out, code recorded) |
| U3 | Exception 02 on a RETRY read also marks unsupported; HEALTH excluded |
| U4 | `mb poll on` / `poll_sched_start` no-op unless STOPPED (a DEGRADED/OFFLINE link keeps its counter and 30 s HEALTH backoff); `mb addr` (`set_addr`) resets counter/unsupported/pending work and makes all registers due |
| U5 | HEALTH read of 000 answering exc 02 = failed read (`reg_model_set_exception`), not unsupported |
| U6 | Completion for an old address after `mb addr` updates only statistics and `last_end`; `bus_task` calls `set_addr` only between `poll_step` calls |

### Task 002 spike decisions (user, 2026-11-24)

Evidence: [`docs/spikes/002-esp-modbus.md`](../../../docs/spikes/002-esp-modbus.md) *(project evidence, not raw)*. Key spike fact **S3:** `mbc_master_send_request` returns only an `esp_err_t`; slave exception, CRC/malformed reply and wrong byte count all give `ESP_ERR_INVALID_RESPONSE`, the exception code is dropped (`EM/mb_objects/mb_master.c:111-118`, `:646-650`). Other spike facts (S5, S6, S11, S12, registry version): [[concepts/firmware-history]] §5.

| # | Decision |
|---|---|
| E2 | No esp-modbus; own FC03-only RTU master (`mb_rtu` + `rs485_port` under `mb_link`, section 2). Rejected: E1a (merged error = exception), E3 (parse the debug log). Re-confirmed after a pros/cons review |
| E1b | Kept as a documented, unbuilt fallback behind the `mb_link.h` seam (section 2); reverting is a separate user decision |
| D7 / Q9 | esp-modbus as only layer and its `==2.1.4` pin **reversed**; dependency removed |
| Registry rule | Own project code is compliant (user-confirmed interpretation, [[concepts/dev-environment]]) |
| Lint / ELF | No esp-modbus; `uart_write_bytes` only in `rs485_port/src/`; FC bytes only in `mb_rtu`, FC03 only; ELF forbids `mbc_`/`mbm_`/`mbs_` and `mb_link_write` (section 3) |
| Task split | 002 split into 012 (`mb_rtu`), 013 (lint/ELF), 014 (`rs485_port` + `mb_link`); 002 retired |
| Gap bytes | Counted and discarded; the request is still sent |
| Counters | `mb_link_get_counters` (gap bytes, port faults, max inter-byte interval) in `mb status`; used by 008 D.8a |
| Seam types | `mb_link.h` has its own `mb_link_port_cfg_t` and `mb_link_counters_t { bool measured; uint32_t gap_bytes, faults, max_byte_interval_us; }`, does not include `rs485_port.h`; `measured` true under E2, false (values 0, `link counters: n/a`) under E1b |
| `mb_link` host test | Fake-`rs485_port` test rejected; refusal-before-port-call proven by code inspection |
| Exc code 0x00 | -> BAD_FRAME |
| Rejected extras | `CONFIG_FREERTOS_HZ=1000` (stay 100 Hz); bench slave-simulator task |
| Agent definitions | `architect.md` / `orchestrator.md` corrected to "sole master on the EKWHCTRL1 'A A B B -' port" (D1) |
| Task order | 012 -> 013 -> 014 -> 006 -> 008; 011 any time; all on sonnet; independent review for 012/013/014 |
| DE safety | `uart_set_mode` before `uart_set_pin`; after driver delete, DE level 0 before output enable; in the inspection checklists (section 4) |

**Discrepancy (recorded, not resolved; moot after E2):** the spike report says there is no side channel for the exception code (S3 consequence 3, citing a debug log of the *sent* frame, `mb_master.c:518-522`); the review names a debug log of the *received* frames (`mb_master.c:610-611`) as the only, fragile side channel (basis of rejected E3). The spike cites `esp_modbus_common.h:91-102`, the review ~`:80-125` (not contradictory).

### Task 013 decisions (lint / ELF; user, 2026-11-24)

| # | Decision |
|---|---|
| L1 | Detect `#include`, `#import`, digraph `%:include`, also with comments between `#` and the directive |
| L2 | Deterministic comment-stripping regex, no catastrophic backtracking |
| L3 | `++` / `--` banned on function-code fields |
| L4 | Manifests match `esp[-_]modbus` |
| L5 | ELF check fails on empty `nm` output and on missing `app_main` |
| L6 | Low-level TX paths and the extra UART setup calls banned outside `rs485_port` (list in section 3 R4/R5) |
| L7 | Octal/binary literal bans **not adopted** (decimal cannot be banned either) |
| L8 | Documented limits: parenthesis nesting; FC arithmetic not evaluated; escapes not decoded; CMake/Kconfig not scanned; `uart_write_bytes` site checked by inspection in 014 |
| L9 | In `mb_link/` and `rs485_port/` names `fc`/`func`/`command` and strings like `"fc=%d"` fail R8; FC hex constants decimal or named |
| A1 | Ban ROM/register TX routes everywhere (R4): `esp_rom_(output\|uart)_\w*`, `UART[0-2]->`, `REG_WRITE`/`WRITE_PERI_REG` on `UART_FIFO\w*_REG`, `soc/uart_reg.h` (done, `9ca4517`) |
| A6 | Also ban `esp_rom_install_*` |
| A7 | Also ban `REG_READ` / `READ_PERI_REG` on `UART_FIFO\w*_REG` |
| A8 | No blanket `esp_rom_*` ban |
| A9 | Nested-bracket prefix `++a[b[1]].fc` documented as a limit |
| A10 | "Matching is linear" docstring corrected; no cap on the gap length |
| A11 | Quadratic R5 `_second_argument` documented |
| A12 | Self-test per-case timing guard 1 s -> 5 s |
| A2/A13 | 013 and its follow-up accepted "as is"; A4 host-run gap closed by A13 |

### Task 014 decisions (`rs485_port` + `mb_link`; user, 2026-11-24)

| # | Decision |
|---|---|
| M1 | IRAM interrupt flag on `uart_driver_install` (removes the IDF "flag updated" line) |
| M2 | `#error` if `CONFIG_UART_ISR_IN_IRAM` is off |
| M3 | `gap_bytes` documented as lower bound, `max_byte_interval_us` as approximate |
| M4 | Silence check rejects 0 ms (and NULL) |
| 006-a | Mute the IDF `uart` log tag to WARN |
| 006-b | Failed hold-receive at boot is fatal |
| 006-c | No read before a clean silence check |
| 008-a | DE checks on site at boot, open, per transaction, with flash/NVS activity, silence check, close; without an instrument the six DE timing items are "not measured", flash/NVS run deferred to phase 2 (Q-HW1/Q-HW2) |
| 008-b | (Measure slave turnaround on site) **replaced by F2** |
| 008-c | No answering slave gives TIMEOUT (checked on site) |
| 008-d | End-of-frame wait (about 20–30 ms, was stated 10–20 ms) accepted as a known limit |
| K5 | DE/ISR facts remain project evidence from the IDF source; on-board checks in 008 |

### Task 006 decisions (`bus_task`, probe console, `app_main`; user, 2026-11-24)

| # | Decision |
|---|---|
| T1 | Boot hint derived from the real state; never "silence ok" while latched (`silence_running` flag, `# silence check: started <s> s` line) |
| T2 | Status snapshot published at boot and around `mb silence` |
| T3 | `mb silence` cancels a pending manual read (`poll_sched_cancel_manual`; spec 010 amended) |
| T4 | Runtime check that the console UART is not the bus UART, before `bus_task` starts; on failure nothing starts |
| T5 | "faults since boot" wording; one `# mb_link: port fault ...` line on a fault |
| T6 | Priority comment on `log_out` |
| T7 | Reviewer's 7 on-site checks added to spec 008 (stack high-water after boot and after `mb silence 1`, abort path, wait out `mb silence`, pending read cancelled, no "queue free spaces" line, latch lines, console port) |

The session labels these fixes M1–M6; in this wiki they are T1–T6 (T7 = spec 008 checks).

### Spec 008 / task 008 decisions (on-site checklist; user, 2026-11-24)

| # | Decision |
|---|---|
| Q-HW1 | No oscilloscope or logic analyser on bench or site: 008 B.6 DE timing and D.8b turnaround logged "not measured" |
| Q-HW2 | Defer the DE-with-flash/NVS run to phase-2 bring-up; no test command in 006 |
| F1 | Heap/leak check after UART delete/re-install (B.5): "not checked (no heap output in the 006 image)" in phase 1 |
| F2 | Exit item 6 answered by `dur_ms` + derived turnaround bound ("derived, not measured"); bound about `dur_ms` - 34 ms after J8 |
| F3 | Hard stop: averaged DE level near 3.3 V during `mb probe` on the bench -> record and stop, back to the architect |
| F4 | Phase-2 DE re-check method without an instrument: decide later (section 8) |
| — | Indirect evidence (multimeter static/averaged DE level, OK replies + counters) recorded, **not counted as a pass** |
| J1 | Every transmitting step tagged `[TRANSMITS]`; UART close/re-open steps tagged no-TX with a "DE glitch possible, not measured" note |
| J2 | B.6.b: run from STOPPED, read during the ~20 s sweep, no `mb poll off`; "unplug USB at once" applies only to the B.6.b hard stop |
| J3 | B.5: start `mb probe` from STOPPED; stop if `faults since boot` changes or a `# mb_link: ... failed` line appears |
| J4 | D.2 address change: always `mb poll off` -> `mb addr` -> `mb poll on` |
| J5 | C.4: skip if unsure about the isolation |
| J6 | D.3: OFFLINE before or during `mb probe` -> go to D.4, no further probing |
| J7 | New stop: `link counters: n/a` still shown after the first `H <t> <health>` console line after boot -> record and stop |
| J8 | Timing figures corrected everywhere: about 20–30 ms wait, bound about `dur_ms` - 34 ms, OK qty-1 `dur_ms` ≥ about 36 ms |
| P4 | Minors fixed: B.4.3 tagged "[TRANSMITS] (polling still running until step 4)"; D.9 tag note names only `mb poll on` |

### Task 011 decisions (fake-slave host tests; user, 2026-11-24)

| # | Decision |
|---|---|
| Major | Fix the D16 gap-coverage gap: the fake fails any request starting < 100 ms after the previous end; `assert_gaps_respected` at the end of every scenario; proven by mutations (G1, G2) |
| Deviations | All 8 implementer deviations accepted by the reviewer (`fake_tag_last()`, unconfigured registers -> exc 02, scenario 2/3 assertion forms, unstarred-fresh reading, replay latency = `dur_ms` incl. TIMEOUT, SWEEP fixtures with dummy times, `mkstemp` under `/tmp`) |
| G6 | Scenarios 11a/11b and the in-fake gap rule are recorded **extra scope beyond spec 011**, not a deviation; spec 011 not amended |
| m2–m7 | Minors stay open (section 8) |

### Recorded conflict (D19)

**Conflict (D19, recorded, not resolved):** the manual gives the address range 1–255 for reg 200 ([[concepts/device-addressing]], p4) while the Modbus spec reserves 248–255 *(external)*; the node accepts only 1..247. Also not stated: which port the address applies to, and "Address 001 – Leave free!" (p7) vs default 1 ([[concepts/device-addressing]]).

## 7. Task status

Session facts and user decisions (session 2026-11-24, not in raw). Full progress narrative, verification output and the old task table: [[concepts/firmware-history]] §7.

| Task | Content | Status | Commit |
|---|---|---|---|
| 001 | skeleton, Kconfig, bus-access lint | done 2026-11-09 | session-end commit 2026-11-09 |
| 009 | `daikin_regs` | done | pushed (see `log.md`) |
| 010 | `reg_model` + `poll_sched` + `txn_log` | done 2026-11-24 | `53e2c49` |
| 002 | esp-modbus spike | retired (stopped at S3, replaced by 012/013/014) | spike report in `3310030` |
| 012 | `mb_rtu` codec | done 2026-11-24 | `568ec8a` |
| 013 | lint/ELF without esp-modbus + follow-up | done 2026-11-24 | `fc5b50e`, `19a3e94`, `9ca4517` |
| 014 | `rs485_port` + `mb_link` | done 2026-11-24 | `987fbd1` |
| 006 | `bus_task`, probe console, `app_main` (first flashable image) | done 2026-11-24, not run on hardware | `b8ad00d` |
| 011 | fake-slave + e2e host tests (host 139 / 0 / 0) | done 2026-11-24 | `17825cb` |
| 008 | phase-1 probe checklist + log template | done 2026-11-24 | `4be04e3` (specs + wiki `13b0591`) |
| 003/004/005/007 | — | retired (007 replaced by 011) | — |
| 015 (2a-A) | Matter skeleton (`firmware/matter/`, esp_matter, BLE commissioning) | implemented + build-verified 2026-11-25; review at feature end (after 016); on-device D1–D7 pending | not committed |
| 016 (2a-B) | simulated model + provisional read-only EP1 mapping + sim console | spec amended by the `architect` (2a-10) | — |

**Phase 1 complete.** Next: user bench run (checklist section B), on-site C/D, probe log -> `raw/` + `/capture` -> phase 2b. Phase 2a (plan approved 2026-11-24, section 6 "Phase-2a plan") runs in parallel.

**Task 015 verified facts** (orchestrator + independent `build-verifier`, session 2026-11-25, not in raw; local inspection): step 0 passed: esp_matter 1.6.1 resolves registry-only on IDF v6.0.2; `dependencies.lock` = esp_matter 1.6.1, cjson 1.7.19~2, esp_delta_ota 1.1.4, esp_encrypted_img 2.7.0, esp_secure_cert_mgr 2.9.3, mdns 1.15.1 (all components.espressif.com) + idf 6.0.2; no network fetch during build, configure works offline. Build `idf.py reconfigure` + `ninja -C build -j2`; clean build about 26 min, no OOM. Image `daikin_matter_skeleton.bin` 1,728,528 B = 41.2 % of the 4 MB factory partition (`partitions.csv`: nvs 0xC000 @0x9000, phy_init 4 KB, factory 4 MB @0x20000; 16 MB flash). 0 warnings in own code (`-Wall -Wextra -Werror`); 4 third-party `std::is_trivial` deprecation warnings in connectedhomeip headers, left as is. Bus guard `tools/check_no_bus.py`: 38 self-test cases, source scan + POST_BUILD ELF `nm` check, deliberate-failure proof (linked `mb_link_dummy`) failed the build. Phase-1 tree unchanged, phase-1 build passes. Deviation from spec: `app_main` and `thermostat_ep` are C++ (esp_matter API), `reset_button` C; factory reset = GPIO0 held 5 s.

## 8. Open items and risks

Closed items and the full earlier wording: [[concepts/firmware-history]] §8.

- Open: phase-2 DE re-check method without scope/analyser (F4); settle before phase-2b bus work.
- Residual risk: DE timing (incl. ISR-in-IRAM, M2) unmeasured in phase 1; evidence limited to console counters and timestamps; re-check at phase-2 bring-up *(inferred)*.
- Risk: flash/NVS stall (Matter) could delay DE release if the TX_DONE ISR is not in IRAM; mitigated by M2; flash/NVS DE run deferred to **phase 2b** (Q-HW2): the 2a image has no bus code (user decision, session 2026-11-24).
- Closed 2026-11-25 (task 015, section 7): CHIP build RAM (`-j2`, no OOM); image size (1.73 MB, 4 MB factory partition); registry-only build on IDF v6.0.2 (step 0 passed).
- Open (015): on-device D1–D7 (QR code print, BLE commissioning, attribute reads, write rejection, rejoin after reboot, factory reset, HA + Apple Home); N16R8 flash / octal PSRAM settings *inferred* until boot on hardware; phase-1 host tests (139/0/0) not re-run because `libbsd-dev` is missing (user action).
- Hard stops: B.6.b averaged DE near 3.3 V (F3, J2); B.5 fault change (J3); `link counters: n/a` after first H line (J7); D.3 OFFLINE -> D.4 (J6).
- Risk: B.6.b reading during an OFFLINE or cancelled sweep is a false pass (H3); mitigated by J2.
- To verify on site: `bus_task` 4 KB stack (high-water after boot and after `mb silence 1`, T7); too small -> new user decision.
- Open: probe image not yet run on hardware.
- Open 008 docs minors (excluded by the user): B.4.2 "started" line wording; `mb probe` asynchronous / `mb regs` after the last SWEEP; median 10 ms bucket upper edge and "answered = OK + EXC"; latency line text; 5 s boot window with USB power; C.5 "in D.3" should be D.2. S1–S9 items of spec 008 not listed individually in the session facts.
- Open 011 minors m2–m7: m2 scenario 10a never runs `poll_step` before 5000 ms; m3 untagged request defaults to kind 0; m4 temp files leak under `/tmp` on failure; m5 `FAKE_REPLAY_MAX` 512 may be exceeded by the real 008 log and replay keys on (addr, reg, qty) only (check before using the log as a fixture); m6 no explicit "no RETRY after RETRY" assertion; m7 11b wait loop has no iteration cap (test robustness only, *inferred*).
- Open 013 doc minors (A14): FIFO-rule bypass with `;` in a comment/string; R5 `_second_argument` > 60 s on 5000 × `uart_set_mode(`; optional self-test for unterminated `/*`. The reviewer's 013 bypass table and the 014 M3 reason text are not in the wiki.
- Open hygiene minor: host build prints 4 CMake `component_validation` warnings (test `main` globs component `test_host` sources; cosmetic).
- "Panel is slave" is inferred ([[entities/ekwhctrl1]]); silence-check traffic or no answer -> stop, back to the architect.
- Risk (user-accepted 2026-11-09): unbiased idle line may falsely trip the boot silence check -> stop, back to the architect *(inferred mechanism)*.
- Starred registers may be unavailable (no water temperature, no STAT, no OUT) ([[concepts/register-map]]).
- Not stated in the manual: port of the address, "Address 001 – leave free" vs default 1, timing, exception codes, write endurance, panel behaviour without polling (300 s timeout probably irrelevant, *inferred*, [[concepts/flag-registers]]).
- Shared "-" with CP: to measure. Isolation of "A A B B -" from mains not stated in the wiki (node on USB power, section 4).
- Termination / bias of the "A A B B -" segment; MAX485-M U1 / supply / RO level ([[entities/max485]], [[concepts/bus-topology]] items 3, 3a).
- FC03 qty > 1 unknown.
- Whether the panel enforces 202/203/245/246 is unknown -> the node enforces them.
- Risk (E2): own code in the TX path (`mb_rtu`, `rs485_port`); mitigated by review, inspection checklists and host tests *(inferred)*.
- Risk (E2): inter-byte timing of the answering slave on "A A B B -" unknown; own frame-end detection depends on it (about 20–30 ms window); checked by 008 D.8a *(inferred)*.
- Risk (M3): `gap_bytes` / `max_byte_interval_us` approximate; quote only as bounds.
- Fallback E1b remains available; revert is a separate user decision (section 2).
- RMW race on 201 accepted.
- How Matter ecosystems render the Fan endpoint *(external)*.
- Not addressed in v0.3 (ask the architect/user): SystemMode under 233 = 0 when STAT answers but both or neither of bits 0/1 are set.
