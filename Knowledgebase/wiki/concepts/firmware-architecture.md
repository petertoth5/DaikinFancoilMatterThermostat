---
title: Firmware architecture v0.3 (approved 2026-11-08, user)
type: concept
sources: ["project-brief.md", "EKWHCTRL0.EKWHCTRL1.EKRTCTRL1_Installation manual_N420384A_English.pdf", "EKWHCTRL1,EKRTCTRL1,EKWHCTRL0_Installation manual_N420384A ModBus RTU_English.pdf", "MAX485-M_EN_10035508.pdf", "20261107_192127.jpg"]
updated: 2026-11-24
---

# Firmware architecture (approved v0.3)

**Status:** v0.3 written by `architect`, **approved by the user on 2026-11-08** by answering Q1–Q10 all with the architect's recommendation (user decision, session 2026-11-08, not in raw). Supersedes v0.2 (approved 2026-11-07, see "Superseded v0.2" at the end). **Addendum 2026-11-09:** stage-1 detail decisions by the user (session 2026-11-09, not in raw) in section 6 "Stage-1 detail decisions"; sections 2, 3, 4 and 8 adjusted where the v0.3 text contradicted them; later 2026-11-09 answers (boot-window commands, `mb silence`, `mb log` default on) and lint rules R1–R8 from `tasks/001-skeleton-kconfig-lint.md` added. **Addendum 2026-11-24:** task-010 detail decisions U1–U6 by the user (session 2026-11-24, not in raw) in section 6 "Task 010 detail decisions"; section 3 adjusted where it contradicted them. Spec `tasks/010-reg-model-poll-sched.md` amended by the architect on 2026-11-24 (U1–U6 and reviewer minors). **Addendum 2026-11-24 (task 002 spike):** the spike stopped at escalation S3; the user chose **E2** (own minimal FC03-only RTU master, no esp-modbus) and approved the lint/ELF and DE-safety changes (user decision, session 2026-11-24, not in raw), see section 6 "Task 002 spike decisions"; sections 2, 3, 4, 7 and 8 adjusted; **D7 and Q9 superseded** (history kept). **Addendum 2026-11-24 (specs 012–014):** E2 confirmed after a pros/cons review with **E1b kept as a documented fallback**, plus the user's answers to the 012–014 spec questions (gap bytes, counters, exception code 0x00, rejected extras) (user decision, session 2026-11-24, not in raw), see section 6 "Task 002 spike decisions"; sections 2, 3, 7 and 8 adjusted. The architecture as a whole, every D-number below and every default value are user decisions of that session unless tagged otherwise.

Tags: *(external)* = Modbus spec, ESP registry, esp-modbus / IDF / esp_matter / CHIP sources, RS485 standard; not in raw. *(project evidence)* = project document `docs/spikes/002-esp-modbus.md` (task-002 spike report, 2026-11-24, source reading of esp-modbus 2.1.4 and IDF v6.0.2, no hardware attached), verified by `firmware-reviewer` and the orchestrator; not a raw source; file:line references are into esp-modbus (`EM/` = `firmware/managed_components/espressif__esp-modbus/modbus/`, `EMR/` = its repo root) or IDF (`IDF/` = `~/esp/esp-idf-v6.0.2/components/`) as in that report. *(inferred)* = design conclusion, not stated in any source. Register facts cite [[concepts/register-map]], [[concepts/flag-registers]], [[concepts/web-restriction-flags]], [[concepts/device-addressing]] (all from [[sources/n420384a-english]] p3–p7, values identical in [[sources/n420384a-modbus-rtu]]). Context: [[concepts/project-goal]], [[concepts/bus-topology]], [[entities/ekwhctrl1]], [[concepts/dev-environment]]. Task specs: `tasks/README.md`.

Q1–Q10 as answered (user, 2026-11-08): Q1 DE + /RE tied to RTS with 10 kΩ pull-down (D8); Q2 233 = 0 with STAT unavailable -> last explicit season, default Heat, console flag "season auto, not mirrored" (D15); Q3 timing defaults accepted (D16); Q4 boot silence check yes (D17); Q5 247 read-only permanently (D18); Q6 address from Kconfig default 1 + RAM console override, no scan (D19); Q7 write caps accepted (D22); Q8 gate exception: stage-1 (probe) specs released before the on-site bus-topology items are answered, stage-2/3 specs only after the probe log (D2); Q9 esp-modbus stays `==2.1.4` (**superseded 2026-11-24**: dependency removed with E2, section 6 "Task 002 spike decisions"); Q10 Thermostat + Fan mapping (D3/D4) re-approved.

## 1. Phases and exit criteria

Tap point and role (D1): the node connects to one A and one B of the EKWHCTRL1 **"A A B B -"** ModBus RTU port and is the **sole Modbus master** there; the **"+ A B -"** fancoil link is never touched (hard rule) ([[concepts/bus-topology]], [[entities/ekwhctrl1]], [[sources/n420384a-english]] p3). Framework rules: ESP-IDF v6.0.2, esp32s3, registry-only components ([[sources/project-brief]], [[concepts/dev-environment]]).

### Phase 1 – on-site read probe
No Matter, no Wi-Fi, FC03 only, write code compiled out (`CONFIG_DMT_BUS_WRITE=n`, section 3). Polling starts automatically after a clean 5 s boot silence check (user, 2026-11-09, section 6). Results go into a **measurement log** -> `Knowledgebase/raw/` -> `/capture`. Exit criteria:

1. Panel slave address confirmed: by the user on the panel (standby, hold AUTO 5 s, wait 10 s without changing; procedure from [[concepts/device-addressing]], p6) and by a bus reply.
2. A/B polarity: valid replies at 9600 8N1 ([[concepts/modbus-rs485-link]]).
3. Silence check saw no foreign traffic on "A A B B -" (tests the inferred "panel is slave", [[entities/ekwhctrl1]]).
4. Table of **every** documented register, starred and unstarred (p4, [[concepts/register-map]]): OK with raw/scaled value, or exception + code, or timeout.
5. Does FC03 with quantity > 1 work (e.g. 200..203 qty 4)? Not stated in the manual ([[concepts/modbus-rs485-link]]).
6. Response time min/median/max over ≥ 1000 transactions. *(2026-11-24, user decision, session 2026-11-24, section 6 "Spec 008 finalisation" F2: answered by the whole-transaction time `dur_ms` plus a derived upper bound for the slave turnaround (about `dur_ms` - 25 ms, logged "derived, not measured"); the pure slave turnaround is "unknown, no instrument".)*
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
| `mb_rtu` | pure | P1 | **new 2026-11-24 (E2, user):** own minimal RTU framing: CRC16, FC03 request builder, reply classifier OK / EXCEPTION + code / BAD_FRAME / TIMEOUT; host-tested; the only place with Modbus function-code bytes (FC03 only in phase 1); implemented and accepted 2026-11-24 (task 012, section 7) |
| `mb_link` | HW | P1 | `mb_link_read(addr, reg, qty, out, *status)` (**API unchanged by E2**); since 2026-11-24 built on `mb_rtu` + `rs485_port` instead of the esp-modbus master (was: "only user of the esp-modbus master", superseded); `mb_link_write` only with `CONFIG_DMT_BUS_WRITE`; rejects addr 0 and anything outside 1..247 *(external, Modbus spec)*; hard-coded write whitelist as last line of defence; `mb_link_get_counters` (gap bytes, port faults, max inter-byte interval; copy of the `rs485_port` counters) shown in `mb status` (user, 2026-11-24, spec `tasks/014-rs485-port-mb-link.md`); the struct `mb_link_counters_t` carries a `measured` flag (true under E2, false with zeros and `link counters: n/a` under the E1b fallback; section 6 "Task 002 spike decisions", row "Seam types and `measured` flag") |
| `rs485_port` | HW | P1 | **owns the IDF UART** (E2, 2026-11-24) in RS485 half-duplex mode, DE via RTS; pins; boot silence check (D17); the only place allowed to call `uart_write_bytes` (section 3); called by `mb_link` |
| `bus_task` + probe console | glue | P1 | single owner of `mb_link`; poll loop; write queue (P3); commands `mb status`, `mb regs`, `mb read <reg> [qty]`, `mb probe`, `mb poll on\|off`, `mb silence <s>`, `mb stats`, `mb addr <1-247>`, `mb log on\|off` (last two: user, 2026-11-09; `mb log` transaction lines default **on** from power-up, user, 2026-11-09) |
| `mapping` | pure | P2 | register <-> Matter conversion (section 5) |
| `matter_bridge` | esp_matter | P2 | endpoints; rejects writes in PRE_UPDATE *(external)*; in P3 turns them into write requests |
| `write_guard` | pure | P3 | Matter intent -> FC06 operations incl. RMW of 201, limits, debounce/caps, readback check, latch (section 3) |

Seam: `mb_link.h` is the only hardware interface. Host tests (D12): the scripted fake slave plugs in at the `poll_sched` link-function seam, because `mb_link` is not buildable for the linux target (user, 2026-11-09; supersedes the v0.3 wording "fake slave behind `mb_link.h`").

### Modbus layer (E2, user, 2026-11-24; supersedes D7)
**Own minimal FC03-only RTU master, no esp-modbus** (user decision, session 2026-11-24, not in raw):
- `mb_rtu` (pure, host-tested): CRC16, FC03 request builder, reply classifier with the four outcomes **OK / EXCEPTION + code / BAD_FRAME / TIMEOUT**, so D21 (no retry on exception, 1 retry on timeout/CRC, exception 02 -> unsupported) gets the split it needs.
- `rs485_port`: owns the IDF UART in `UART_MODE_RS485_HALF_DUPLEX`, DE via RTS (pin order and DE re-drive: section 4).
- `mb_link`: public API unchanged; built on `mb_rtu` + `rs485_port`.
- Reason: the task-002 spike showed that esp-modbus 2.1.4 cannot distinguish a slave exception from a bad reply and drops the exception code (S3, section 6 "Task 002 spike decisions") *(project evidence)*. E2 re-confirmed after a pros/cons review (user, 2026-11-24). Rejected: E1a (classify the merged error as exception), E3 (parse the esp-modbus debug log).
- **Fallback E1b (user, 2026-11-24; not built):** if E2 proves too complicated or hard to debug on site, revert to the esp-modbus `==2.1.4` serial master with every invalid reply classified as BAD_FRAME (exception code 0, retry once, counts toward OFFLINE). Reverting is a **separate user decision**. `mb_link.h` is the single seam, so a revert replaces only `mb_link` internals. Losses under E1b: exception-02 unsupported detection (D21), exception codes (U2/U3/U5), probe items 4/5 weakened. Revert reference: the spike findings (descriptor table, pin order, S11) in `docs/spikes/002-esp-modbus.md` *(project evidence)*. *(Was 2026-11-24 earlier in the session: E1b rejected together with E1a.)*
- Tasks: `tasks/012-mb-rtu-frame-codec.md` (`mb_rtu`), `tasks/013-lint-elf-check-no-esp-modbus.md` (lint/ELF), `tasks/014-rs485-port-mb-link.md` (`rs485_port` + `mb_link`), section 7.

*Superseded D7 text (2026-11-08, kept as history):* "esp-modbus master via `mbc_master_send_request` (raw requests, no parameter descriptors) is the only Modbus layer. Reasons: approved, in the registry, provides CRC/framing and RS485 DE via RTS *(external)*, no own stack. **Escalation in task 002:** if timeout, exception (with code) and CRC error cannot be distinguished, go back to the architect; alternative is an own minimal RTU master." The escalation was hit and the alternative chosen (2026-11-24).

Dependencies: `espressif/esp_matter` ^1.6.1 (P2) *(external, registry)*; IDF built-ins `esp_driver_uart`, `esp_driver_gpio`, `console`, `esp_timer`, `freertos`, `nvs_flash` (Matter only), `unity`. *(Superseded 2026-11-24: `espressif/esp-modbus` **==2.1.4** (Q9), removed with E2.)* Registry-only rule: own project code adds no dependency and removes one, so it is compliant (user-confirmed interpretation, session 2026-11-24, [[concepts/dev-environment]]).

### Threading
- `bus_task`: priority ~10, pinned to core 1 *(inferred)*; the only caller of `mb_link`. *(Superseded 2026-11-24: "esp-modbus task priorities stay at their Kconfig defaults"; with E2 there are no esp-modbus tasks.)*
- `matter_bridge` takes `reg_model` snapshots and updates attributes in the Matter context; `bus_task` never takes the CHIP lock.
- P3: write intents go through a queue of depth 4; newest intent per register wins.
- `CONFIG_UART_ISR_IN_IRAM=y` kept (user, 2026-11-09).
- **Console rule (kept from v0.2):** console REPL on USB-Serial-JTAG / UART0, never on the bus UART (compile-time `#error` + runtime check).

## 3. Bus-safety rules

The manual states no timing, polling interval, response time or exception codes ([[entities/ekwhctrl1]], [[concepts/bus-topology]]); all defaults below are therefore *(inferred)* design values (D16), to be re-tuned from the probe log.

### Timing (D16)
- One transaction at a time; gap ≥ 100 ms after each.
- Reference: t3.5 ≈ 4.0 ms at 9600 baud with 11-bit characters *(external, Modbus serial-line spec)*.
- **Gap bytes** (bytes received before a request, e.g. late replies or foreign traffic): counted in the `rs485_port` counters and discarded; the request is still sent (user, 2026-11-24).
- `CONFIG_FREERTOS_HZ` stays at **100 Hz** (1000 Hz rejected, user, 2026-11-24); the resulting end-of-frame silence window of 10–20 ms is a known, accepted limit (user, 2026-11-24).
- **`CONFIG_UART_ISR_IN_IRAM=y` is load-bearing for DE (user, 2026-11-24, `firmware-reviewer` on task 014):** DE is released in the UART TX_DONE ISR (section 4). If that ISR is not in IRAM, a flash or NVS stall (Matter activity) can delay the release and hold DE high after our request. Build guard M2 (`#error` if the option is off) and section 8 risk.
- **Counters are approximate (user decision, 2026-11-24, M3):** `gap_bytes` is a lower bound and `max_byte_interval_us` is approximate. Do not quote them as exact values in the probe log (section 6 "Task 014 review decisions").
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
- Exception 02 -> register `unsupported` until reboot, not polled again. Applies to POLL, RETRY, SWEEP and MANUAL reads of qty 1 (U3, user, 2026-11-24). **Not HEALTH:** a HEALTH read of 000 answering exception 02 is recorded as a failed read, not as unsupported (U5, architect proposal confirmed by user, 2026-11-24).
- Other exception codes (user, 2026-11-09): no retry, register backs off to 60 s, not counted toward OFFLINE. They do **not** mark the register unsupported in `reg_model`: recorded like a failed read (last value kept, ages out by time, exception code recorded) (U2, user, 2026-11-24).
- 1–4 consecutive failures -> **DEGRADED** (user, 2026-11-09); 5 consecutive failures -> **OFFLINE**: automatic traffic is only 000 every 30 s (manual `mb read` still allowed, U1, user, 2026-11-24); first success -> **ONLINE**. States ONLINE / DEGRADED / OFFLINE with backoff.
- Stale = 3 × group period: 15 s fast, 180 s slow (user, 2026-11-09).
- A reply carrying exception code **0x00** is classified **BAD_FRAME** (user, 2026-11-24).
- Never broadcast; addresses 1..247 only *(external, Modbus spec)*.

### Fallbacks
- Console `mb poll off` / `mb poll on`. Polling starts automatically after a clean boot silence check (user, 2026-11-09). `mb probe` and `mb read` also work while polling is stopped, without retry and without changing link health (user, 2026-11-09). `mb read` also works while OFFLINE: single read, no retry, gap respected, no effect on health or the HEALTH schedule (U1, user, 2026-11-24). `mb poll on` is a no-op unless polling is stopped: a DEGRADED/OFFLINE link keeps its failure counter and 30 s HEALTH backoff; `mb addr N` still resets counter, unsupported marks and pending work and makes all registers due (U4, user, 2026-11-24).
- Boot silence check (D17): RX-only for 5 s at boot (60 s on demand via `mb silence <s>`) **before** the master transmits (v0.3 wording "before esp-modbus starts", reworded 2026-11-24 for E2); any received byte -> refuse to start the master / transmit, latched until reboot.
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
- **Lint/ELF change for E2 (user, 2026-11-24, not in raw; to be implemented by the lint/ELF task, one of the three tasks replacing 002, section 7):**
  - no esp-modbus anywhere (no `mbc_*` / `mbm_*` / `mbs_*` names, no esp-modbus headers);
  - `uart_write_bytes` allowed **only in `components/rs485_port/src/`** (was banned everywhere by R4);
  - Modbus function-code bytes only in `mb_rtu`, and **FC03 only** in phase 1;
  - post-build ELF check forbids **all** `mbc_` / `mbm_` / `mbs_` symbols and `mb_link_write`.
  - Implemented by `tasks/013-lint-elf-check-no-esp-modbus.md`, which must land before 014; it also closes the open lint-self-test minor (compound operators, section 7).
  - **Status 2026-11-24 (session facts, not in raw):** task 013 first pass implemented by `firmware-dev` (sonnet): R1/R3/R4/R8 rewritten for E2, self-test 218 cases; new ELF check `tools/check_elf_symbols.py` (stdlib only, 24 self-test cases) runs as POST_BUILD hook on the ELF with `CMAKE_NM` = `xtensa-esp32s3-elf-nm`; `DMT_ELF_REQUIRED_SYMBOLS` is empty until task 006. The ELF check currently prints `forbidden 0 found; required: none configured`. Verified by `build-verifier` PASS and `firmware-reviewer` APPROVE WITH MINORS (bypass table from the reviewer, not copied here: open question). Follow-up fixes required by the user, see "Task 013 review decisions" in section 6.
  - **ELF check behaviour after the follow-up (user decision 2026-11-24):** the check **fails** on empty `nm` output and when `app_main` is missing (a silent nm failure must not pass as "0 found").
  - Rule text as of the task 013 first pass: R1, R3, R4 and R8 rewritten for E2; R5 touched by the follow-up (**confirmed 2026-11-24:** the ten new UART setup names in R5 are allowed only in `components/rs485_port/`; `uart_flush_input` stays allowed). **Rule numbers as implemented (session facts, 2026-11-24, `build-verifier` and `firmware-reviewer`; not in raw):**
    - R3: esp-modbus headers, incl. comment, digraph and `#import` forms; manifests match `esp[-_]modbus`.
    - R4: UART TX ban; `uart_write_bytes` only in `components/rs485_port/src/`; `uart_ll_*`, `uart_hal_*`, `esp_rom_uart_tx_one_char`, `uart_tx_one_char`, raw `UARTn.` register access and the `uart_struct.h` / `uart_ll.h` / `uart_hal.h` includes are banned everywhere, including `rs485_port`. Added by the 013 follow-up (commit `9ca4517`, section 6 rows A5–A8): `esp_rom_output_*`, `esp_rom_uart_*`, `esp_rom_install_*` (no blanket `esp_rom_*` ban), raw `UARTn.` / `UARTn->` access, `REG_WRITE` / `WRITE_PERI_REG` / `REG_READ` / `READ_PERI_REG` on `UART_FIFO*_REG` (bounded 200-character window, linear) and the `uart_reg.h` include, everywhere including `rs485_port`. Documented limits: section 6 rows A9–A14.
    - R5: UART setup calls, incl. the ten new names, only in `components/rs485_port/`; `uart_flush_input` allowed.
    - R8: function-code bytes only in `mb_rtu`, and only 0x03; `++` / `--` on function-code fields banned; deterministic comment regex.
    - Results: lint self-test 343 cases (0.24 s), tree scan rc 0; ELF self-test 32 cases, fails on empty `nm` output or missing `app_main` (`--nm /bin/true` gives exit 1); esp32s3 fresh build 0 warnings; ELF summary `forbidden 0 found; required: none configured`.
    - ROM/register TX routes (row A1): banned since commit `9ca4517` (section 6, rows A5–A8, A13). Open doc minors: section 6 row A14.
    - The exact rule text is in `firmware/tools/check_bus_access.py` and spec 013, not copied here.
  - **Stricter UART rules and new bans (user decision 2026-11-24; applied by the 013 follow-up (commit `9ca4517`) and accepted 2026-11-24, incl. the ROM/register routes of row A1; exact rule ids as in the bullets above):** low-level TX paths (`uart_ll_*`, `uart_hal_*`, `esp_rom_uart_tx_one_char`, `uart_tx_one_char`, raw `UARTn.` register access, and their headers) and the UART setup calls `uart_set_hw_flow_ctrl`, `uart_set_baudrate`, `uart_set_parity`, `uart_set_word_length`, `uart_set_stop_bits`, `uart_set_tx_idle_num`, `uart_enable_tx_intr`, `uart_flush`, `uart_intr_config`, `uart_isr_register` are banned outside `components/rs485_port/`.
  - The R1–R8 text below is the task 001 rule set; where it differs from the two bullets above, the bullets above apply *(inferred from the decisions; exact rule text is the architect's spec)*.
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
  - post-build ELF check fails if `mb_link_write` is present with the gate off (extended 2026-11-24 for E2: also any `mbc_` / `mbm_` / `mbs_` symbol, see above). Background: with esp-modbus the write handlers could not be compiled out and the ELF check could not forbid them (spike S11, `docs/spikes/002-esp-modbus.md` S11, `EM/mb_objects/include/mb_config.h:134-158`) *(project evidence)*; moot after E2.
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
- **DE signal (resolved 2026-11-24, task-002 spike S5, *project evidence*, source reading, no hardware):** in `UART_MODE_RS485_HALF_DUPLEX` on esp32s3 / IDF v6.0.2 **RTS** drives DE: **low while idle, high during TX** (`IDF/esp_hal_uart/esp32s3/include/hal/uart_ll.h:773-783`; `IDF/esp_driver_uart/src/uart.c:1191` raises it before the TX FIFO is filled, `:1506` drops it in the TX_DONE ISR) ([`docs/spikes/002-esp-modbus.md`](../../../docs/spikes/002-esp-modbus.md) S5). *(Was: "verified in the task-002 spike, open".)* On-board confirmation is still for 008.
- **Pin order (DE safety, user-approved 2026-11-24):** `uart_set_mode(UART_MODE_RS485_HALF_DUPLEX)` **first**, then `uart_set_pin` (TX, RX, DE as RTS). Reason: `sw_rts` resets to 0 (`IDF/soc/esp32s3/register/soc/uart_struct.h:517`) and the RTS pin follows its inverse, so routing RTS before the mode is set drives DE **high** in between *(project evidence, S5)*. The spec 002 interface comment had the reverse order (spike "Findings beyond S1-S12" point 2).
- **DE re-drive after driver delete (DE safety, user-approved 2026-11-24):** `uart_driver_delete` -> `uart_release_pin` disables the RTS output (`uart.c:2116`); DE then floats and only the 10 kΩ pull-down holds it low until re-driven. `rs485_port` must write the DE GPIO level **0 before** `gpio_output_enable` (`IDF/esp_driver_gpio/src/gpio.c:228`) *(project evidence, S5)*.
- Both DE rules are added to the code-inspection checklists (user, 2026-11-24).
- **DE timing facts (task 014 review, `firmware-reviewer`, session 2026-11-24; project evidence from reading the local IDF v6.0.2 source, not verified on hardware):** DE is high only during our own request transmit (8 bytes). RTS is asserted before the TX FIFO is filled (`uart.c:1191`) and released in the TX_DONE ISR (`uart.c:1506`). Half-duplex mode sets `sw_rts` = 1, i.e. RTS low (`uart_ll.h:773-783`). The mode is set before the pins. DE is driven low before the output is enabled (`gpio.c:228-229`). After driver delete, DE floats on the 10 kΩ pull-down until re-driven. No reply byte is lost to the ISR RX-FIFO reset (`uart.c:1505`), because the receiver is off while DE is high. The DE release depends on the ISR, see section 3 (`CONFIG_UART_ISR_IN_IRAM`). On-board confirmation of DE for each transaction: task 008.
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
| D7 | **superseded 2026-11-24** | (was: esp-modbus master via `mbc_master_send_request` is the only Modbus layer; sniffer/monitor dropped). Reversed by the user (E2): own minimal FC03-only RTU master (`mb_rtu` + `rs485_port` under `mb_link`), no esp-modbus; sniffer/monitor stay dropped. See "Task 002 spike decisions" |
| D7a | retired | (was: private esp-modbus `mb_crc16` for the sniffer) |
| D7b | retired | (was: time-shared UART); esp-modbus owns the UART after the silence check (*superseded 2026-11-24:* `rs485_port` owns the UART, E2) |
| D8 | changed | RS485 half-duplex, RTS -> DE + /RE tied, 10 kΩ pull-down (Q1) |
| D9 | kept | Write whitelist 231, 201 bits 0–2/7 via RMW, 233; never 200 |
| D10 | changed | Single compile-time gate `CONFIG_DMT_BUS_WRITE`; debounce, caps, readback, ≤ 1 retry, latch |
| D11 | kept | Commissioning BLE + Wi-Fi, dev DAC, 0xFFF1/0x8000, BOOT long-press factory reset |
| D12 | changed | Host tests against a fake `mb_link` slave; probe logs become fixtures |
| D13 | kept+ | RO level shift, DE pull-down, all bus GPIO via Kconfig |
| D14 | kept | chip-tool allowed for tests |
| D15 | changed | 233 = 0 with STAT unavailable -> last explicit season (default Heat) + console flag; if STAT answers, v0.2 D15 = b unchanged (Q2) |
| D16 | new | Timing defaults, section 3 (Q3) |
| D17 | new | Boot silence check: RX-only 5 s at boot (60 s on demand) before esp-modbus starts (2026-11-24: before the master transmits, E2); any byte -> refuse TX, latched (Q4) |
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

### Task 010 detail decisions (user, 2026-11-24)

User statements in the session of 2026-11-24, not in raw (user decision, session 2026-11-24); answers to spec ambiguities in `tasks/010-reg-model-poll-sched.md` raised by `firmware-dev` during implementation. U1–U4 are the option recommended to the user; U5–U6 are the architect's proposal, confirmed by the user. Spec amendment by the `architect` done on 2026-11-24: `tasks/010-reg-model-poll-sched.md` amended with U1–U6 and the reviewer minors (new `reg_model_set_exception`, txn_log enum bounds check, mixed-kind gap soak test).

| # | Topic | Decision |
|---|---|---|
| U1 | MANUAL reads while OFFLINE | `mb read` (MANUAL) is allowed while OFFLINE: single read, no retry, gap respected, no effect on health or the HEALTH schedule. Spec rule 8 "only HEALTH reads" applies to automatic traffic |
| U2 | Exception codes other than 02 in `reg_model` | Do **not** mark the register unsupported; recorded like a failed read (timeout-like: last value kept, ages out by time; exception code recorded). Only exception 02 sets unsupported, consistent with the scheduler (D21) |
| U3 | Exception 02 on a RETRY read | Also marks the register unsupported (spec rule 6 extended to RETRY); HEALTH stays excluded as written |
| U4 | `mb poll on` / `poll_sched_start` | No-op unless the scheduler is STOPPED: a DEGRADED/OFFLINE link keeps its counter and 30 s HEALTH backoff. `mb addr` (`set_addr`) still resets counter/unsupported/pending work and makes all registers due (new slave address) |
| U5 | HEALTH read of reg 000 answering exception 02 | `reg_model` records it as a failed read (`reg_model_set_exception`, not unsupported), consistent with the scheduler, which never marks 000 unsupported from HEALTH (U3). Architect proposal, user confirmed |
| U6 | Completion for an old address after `mb addr` (rule 12 in spec 010) | Only statistics and the inter-frame gap (`last_end`) are updated; no effect on counter, health, retry, schedule, unsupported marks or `reg_model`. Caller contract: `bus_task` calls `set_addr` only between `poll_step` calls. Architect proposal, user approved |

### Task 002 spike decisions (user, 2026-11-24)

User statements in the session of 2026-11-24, not in raw (user decision, session 2026-11-24); rows E1b to "Task order" are the later answers given while specs 012–014 were written. Evidence: the task-002 spike report [`docs/spikes/002-esp-modbus.md`](../../../docs/spikes/002-esp-modbus.md) (`firmware-dev`, 2026-11-24, source reading of esp-modbus 2.1.4 on IDF v6.0.2 plus one throw-away image build, no hardware), verified by `firmware-reviewer` and the orchestrator. It is **project evidence, not a raw source**; file:line references below are as given in that report and the review (prefixes in the "Tags" paragraph).

**Spike facts (project evidence):**
- **S3 (escalation hit):** `mbc_master_send_request` returns only an `esp_err_t`. A slave exception, a CRC/malformed reply and a CRC-valid reply with a wrong byte count all come back as `ESP_ERR_INVALID_RESPONSE`; the exception code is dropped (`EM/mb_objects/mb_master.c:111-118`, `:646-650`; mapping `MB_ERR_TO_ESP_ERR` in `EMR/modbus/mb_controller/common/include/esp_modbus_common.h` ~`:80-125`). Timeout -> `ESP_ERR_TIMEOUT`. Frames of <= 3 bytes are dropped silently (`EM/mb_ports/serial/port_serial.c:169-173`) and therefore end as a timeout. The only side channel is the debug log of received frames (`mb_master.c:610-611`), judged fragile. The wrong-byte-count case and the debug-log side channel come from the review/verification (caller report), not from the spike text; see "Discrepancy" below.
- esp-modbus **2.1.4 is the latest registry version** (registry page fetched 2026-11-24, *external*), so no newer release to wait for.
- **S5:** RTS drives DE in RS485 half-duplex mode (low idle, high during TX); pin order and re-drive rules in section 4.
- **History, moot after E2:** esp-modbus has no internal retries (S6); 150 ms cooldown after a timeout (`CONFIG_FMB_MASTER_TIMEOUT_COOLDOWN_MS`, S6); frame end detected after 3 character times, not 3.5 (`MB_SERIAL_TOUT = 3`, S6); `mbc_master_start` needs a descriptor table via `mbc_master_set_descriptor`, not allowed by lint R1 ("Findings beyond S1-S12" point 1); write handlers cannot be compiled out (S11); no slave/TCP symbols linked (S12); `ESP_ERROR_CHECK` on `uart_driver_delete` aborts if the delete fails (S5, S10).

**Decisions:**

| # | Topic | Decision |
|---|---|---|
| E2 | S3 resolution | **No esp-modbus.** Own minimal FC03-only RTU master: pure `mb_rtu` component (CRC16, FC03 request builder, reply classifier OK / EXCEPTION + code / BAD_FRAME / TIMEOUT, host-tested); `rs485_port` owns the IDF UART in RS485 half-duplex mode with DE via RTS; `mb_link` API unchanged (section 2). Rejected: **E1a** (classify the merged `ESP_ERR_INVALID_RESPONSE` as exception), **E3** (parse the esp-modbus debug log). *(E1b was listed as rejected at first; changed the same day, see row E1b.)* Re-confirmed 2026-11-24 after a pros/cons review |
| D7 | Modbus layer | "No own Modbus stack, esp-modbus as the only Modbus layer" **reversed** |
| Q9 | esp-modbus pin | `==2.1.4` **reversed**: dependency removed |
| — | Registry-only rule | Own project code is compliant (no dependency added, one removed); user-confirmed interpretation ([[concepts/dev-environment]]) |
| — | Lint / ELF | No esp-modbus anywhere; `uart_write_bytes` only in `components/rs485_port/src/`; Modbus function-code bytes only in `mb_rtu`, FC03 only in phase 1; ELF check forbids all `mbc_` / `mbm_` / `mbs_` symbols and `mb_link_write` (section 3) |
| — | Task split | Task 002 is split into three tasks: (1) pure `mb_rtu`; (2) `rs485_port` + `mb_link` over the UART; (3) lint/ELF update. Written by the `architect` as 012 / 014 / 013 (section 7) |
| E1b | Fallback | **Kept in scope as a documented fallback, not built:** esp-modbus `==2.1.4` serial master, every invalid reply = BAD_FRAME, exc_code 0, retry once, counts toward OFFLINE; used only if E2 proves too complicated or hard to debug on site; reverting is a separate user decision. `mb_link.h` API is the single seam (revert replaces `mb_link` internals only). Loss: exception-02 unsupported detection (D21), exception codes (U2/U3/U5), probe items 4/5 weakened. Revert reference: spike findings (descriptor table, pin order, S11) in `docs/spikes/002-esp-modbus.md` |
| — | Gap bytes | Bytes seen before a request are counted and discarded; the request is still sent |
| — | Counters | `mb_link_get_counters` (gap bytes, port faults, max inter-byte interval) approved; shown in `mb status`; used by task 008 D.8a |
| — | Seam types and `measured` flag | `mb_link.h` has its own `mb_link_port_cfg_t` and `mb_link_counters_t { bool measured; uint32_t gap_bytes, faults, max_byte_interval_us; }` and does not include `rs485_port.h` (seam rule, spec 014). Under E2 `measured = true`. Under the E1b fallback `measured = false`, values 0, and `mb status` prints `link counters: n/a`, so a fallback log is never mistaken for a real 0 measurement. User approved keeping the flag (architect proposal) |
| — | `mb_link` host test | With a fake `rs485_port`: **rejected**; refusal-before-port-call is proven by code inspection instead |
| — | Exception code 0x00 | A reply with exception code 0x00 -> BAD_FRAME |
| — | Rejected extras | `CONFIG_FREERTOS_HZ=1000` (stay at 100 Hz; end-of-frame silence window 10–20 ms is a known limit); a bench slave-simulator task |
| — | Agent definitions | `.claude/agents/architect.md` and `orchestrator.md` corrected by the orchestrator (user-approved) from "second master on the EKWHCTRL1 <-> INN-FR-B32 bus" to "sole master on the EKWHCTRL1 'A A B B -' port", matching D1 |
| — | Task order | 012 -> 013 -> 014 -> 006 -> 008; 011 any time; all implemented by `firmware-dev` on sonnet; 012, 013 and 014 each get an independent `firmware-reviewer` review. 002 is a retired note (section 7) |
| — | DE safety (independent of E2) | `uart_set_mode(RS485 half-duplex)` before `uart_set_pin`; after driver delete, write DE level 0, then enable the output; both added to the code-inspection checklists (section 4) |

**Discrepancy (recorded, not resolved):** the spike report says there is **no side channel** for the exception code (`docs/spikes/002-esp-modbus.md` S3 consequence 3; it cites only a debug log of the *sent* frame, `mb_master.c:518-522`), while the review/verification names a debug log of the *received* frames (`mb_master.c:610-611`) as the only, fragile side channel (basis of the rejected option E3). Likewise, the spike's mapping table cites `esp_modbus_common.h:91-102`, the review gives ~`:80-125` (wider range, not contradictory). Moot for the design after E2.

### Task 013 review decisions (user, 2026-11-24)

User decisions in the session of 2026-11-24, not in raw (user decision, session 2026-11-24), taken after the task 013 first pass and the `firmware-reviewer` review (APPROVE WITH MINORS, section 7). Scope: lint and ELF check only (section 3). Spec 013 is amended by the architect, then `firmware-dev` implements.

| # | Topic | Decision |
|---|---|---|
| L1 | Include detection | Must detect `#include`, `#import` and the digraph `%:include`, also when comments sit between `#` and the directive (comments are not exempt from the scan, section 3) |
| L2 | Comment handling in the scanner | Deterministic comment-stripping regex with no catastrophic backtracking |
| L3 | Increment / decrement on function-code fields | `++` and `--` banned on function-code fields (`fc`-type fields, section 3 R8 scope) |
| L4 | Manifest match | Manifests match `esp[-_]modbus` (covers `esp-modbus` and `esp_modbus` spellings) |
| L5 | ELF check, empty output | ELF check **fails** on empty `nm` output and when `app_main` is missing |
| L6 | Stricter UART rules | Low-level TX paths (`uart_ll_*`, `uart_hal_*`, `esp_rom_uart_tx_one_char`, `uart_tx_one_char`, raw `UARTn.` registers, their headers) and the UART setup calls `uart_set_hw_flow_ctrl`, `uart_set_baudrate`, `uart_set_parity`, `uart_set_word_length`, `uart_set_stop_bits`, `uart_set_tx_idle_num`, `uart_enable_tx_intr`, `uart_flush`, `uart_intr_config`, `uart_isr_register` banned outside `components/rs485_port/` (section 3) |
| L7 | Octal / binary literal bans | **Not adopted.** Decimal literals cannot be banned either; the rule would not close the gap |
| L8 | Documented remaining limits | Written into the lint documentation and the 013 spec: (a) parenthesis nesting limit; (b) arithmetic on FC constants (e.g. `0x03 + 0x10`) not evaluated; (c) char and string escapes not decoded; (d) CMake and Kconfig files are not scanned; (e) the single `uart_write_bytes` call site in `rs485_port` is checked by inspection in task 014, not by the lint |
| L9 | Consequence for task 014 | In `mb_link/` and `rs485_port/`, names `fc`, `func`, `command` and strings or comments such as `"fc=%d"` fail R8. Hex constants that are FC values (e.g. `0x10`) must be written in decimal or as a named constant. Task 014 must follow this |

### Task 013 acceptance (user, 2026-11-24)

User decisions in the session of 2026-11-24, not in raw (user decision, session 2026-11-24). Task 013 follow-up (L1–L9) verified by `build-verifier` PASS and `firmware-reviewer` APPROVE WITH MINORS, all earlier findings closed (section 7).

| # | Topic | Decision |
|---|---|---|
| A1 | ROM / register TX routes | **Done** (user decision 2026-11-24; implemented in commit `9ca4517`, rows A5–A8). Ban everywhere (R4): `esp_rom_output_tx_one_char`, `esp_rom_uart_putc` (pattern `esp_rom_(output\|uart)_\w*` for TX names), `UART[0-2]->` register access, `REG_WRITE` / `WRITE_PERI_REG` on `UART_FIFO\w*_REG`, and the `soc/uart_reg.h` include. Implemented by the follow-up (row A5), before task 014 starts; verified, row A13 |
| A2 | Acceptance of 013 | **Accepted "as is", 2026-11-24** (user). Host-test re-run was not done, see A4. Not committed (commit on request) |
| A3 | Open minors (accepted as is) | (1) prefix `++a[b[1]].fc` with nested brackets not detected: document as a limit. (3) The docstring claim "matching is linear" is false for unterminated or repeated `/*` (contrived, ~40 s at 20000 repeats): correct the docstring or cap the gap length. (4) R5 `_second_argument` is quadratic on 3000 × `uart_set_mode(` (pre-existing): mention it. (5) The self-test 1.0 s per-case timing guard can fail on a loaded machine: consider 5 s. Caller's plan (2026-11-24): minors 1, 3, 4, 5 are fixed in the same lint follow-up as A1, before 014. **Status: done in `9ca4517`** (rows A9–A12) |
| A4 | Host-test re-run | **Not run** in the 013 `build-verifier` pass (blocked by the permission classifier). User accepted 013 without it: no C code changed in 013; `firmware-dev` reported 118 / 0 / 0. **Closed** in the follow-up pass (row A13): fresh host build and run 118 / 0 / 0. |
| A5 | 013 follow-up | **Done 2026-11-24** (user, session 2026-11-24, not in raw). Scope: the ROM/register TX ban (A1), extended by A6 and A7 (A8 limits the scope), and minors A9–A12. Implemented by `firmware-dev` (sonnet), committed and pushed as `9ca4517` (reported by the caller). Verified and accepted: row A13 |
| A6 | Q-a: `esp_rom_install_*` | **Yes** (user, 2026-11-24). Also ban `esp_rom_install_*` (e.g. `esp_rom_install_uart_printf` can redirect output to a UART) |
| A7 | Q-b: register reads | **Yes** (user, 2026-11-24). Also ban `REG_READ` / `READ_PERI_REG` on `UART_FIFO\w*_REG` |
| A8 | Q-c: blanket ROM ban | **No** (user, 2026-11-24). No blanket `esp_rom_*` ban; only the names listed in A1, A6 |
| A9 | Minor (1): nested brackets | Documented limit: the prefix `++a[b[1]].fc` (nested brackets) is not detected (A3 minor 1) |
| A10 | Minor (3): docstring | The "matching is linear" docstring is corrected (A3 minor 3). **No cap** on the gap length (user, 2026-11-24) |
| A11 | Minor (4): R5 helper | Documented: `_second_argument` is quadratic (3000 × `uart_set_mode(`) (A3 minor 4) |
| A12 | Minor (5): timing guard | The self-test per-case timing guard goes from 1 s to **5 s** (A3 minor 5) |
| A13 | Follow-up verified and accepted | **Accepted "as is", 2026-11-24** (user), commit `9ca4517` (committed and pushed, reported by the caller). `build-verifier` PASS: fresh esp32s3 build 0 warnings; lint self-test 372 cases (0.3 s); ELF `forbidden 0 found; required: none configured`; fresh host build and run 118 Tests / 0 Failures / 0 Ignored (closes A4). `firmware-reviewer`: APPROVE WITH MINORS (200-character window acceptable). No blanket `esp_rom_*` ban (A8) |
| A14 | Open doc minors (not fixed) | (a) the FIFO rule (A7) can be bypassed with a `;` inside a comment or string in the `REG_*` argument list: add to the documented limits. (b) R5 `_second_argument` on 5000 × `uart_set_mode(` takes more than 60 s (effectively a hang): state it in the limit text (A11). (c) optional self-test case for an unterminated `/*`. Not fixed; the lint code is unchanged by this step |

Open for the caller: the `firmware-reviewer` bypass table (from the 013 review) is not in the wiki; the wiki copy is pending. Its remaining items are rows A14 (a)–(c).

### Task 014 review decisions (user, 2026-11-24)

User decisions in the session of 2026-11-24, not in raw (user decision, session 2026-11-24), taken after the task 014 implementation and the `firmware-reviewer` review (APPROVE WITH MINORS, section 7). The `architect` amended specs 014, 006 and 008; `firmware-dev` applies M1–M4 to 014. **Status 2026-11-24: M1–M4 done and re-checked; task 014 accepted 2026-11-24** (section 6 "Task 014 acceptance" below). The user approved the commit after the fix; committed and pushed as `987fbd1` (session facts 2026-11-24, not in raw).

Fix now (task 014 minors, `rs485_port` + `mb_link`):

| # | Topic | Decision |
|---|---|---|
| M1 | Interrupt flag | Pass the IRAM interrupt flag to `uart_driver_install`; this removes the IDF "flag updated" log line |
| M2 | Build guard | `#error` if `CONFIG_UART_ISR_IN_IRAM` is off (section 3, DE) |
| M3 | Counter semantics | Document `gap_bytes` as a **lower bound** and `max_byte_interval_us` as **approximate** (section 3). The reason is in the reviewer's minor text, not copied here |
| M4 | Silence check | Reject a **0 ms** silence check |

Spec 006 (amended by the architect; implemented later):

| # | Topic | Decision |
|---|---|---|
| 006-a | IDF `uart` log | Mute the IDF `uart` log tag to WARN |
| 006-b | Boot hold-receive | A failed hold-receive at boot is **fatal** |
| 006-c | Read before silence check | **No read** before a clean silence check passes |

Spec 008 (on-site, amended by the architect; section 8 and `tasks/008`):

| # | Topic | Decision |
|---|---|---|
| 008-a | DE scope on site | Check DE at boot, at open, per transaction, with concurrent flash/NVS activity, at the silence check, and at close. *(Update 2026-11-24: no instrument (Q-HW1), the six DE timing items are "not measured"; the flash/NVS run is deferred to phase 2 (Q-HW2); section 6 "Spec 008 finalisation")* |
| 008-b | Slave turnaround | Measure the slave turnaround on site. ***Replaced 2026-11-24** (user decision, session 2026-11-24) by F2 in "Spec 008 finalisation" below: `dur_ms` plus a derived upper bound (about `dur_ms` - 25 ms, "derived, not measured"); pure turnaround "unknown, no instrument"* |
| 008-c | No slave | No answering slave gives **TIMEOUT** (checked on site) |
| 008-d | End-of-frame wait | The **10–20 ms** end-of-frame wait is **accepted** (known limit, section 3) |

### Task 014 acceptance (user, 2026-11-24)

User decision in the session of 2026-11-24, not in raw (user decision, session 2026-11-24). Session facts from the M1–M4 fix are verified output, not in raw.

| # | Topic | Decision |
|---|---|---|
| K1 | M1–M4 fixes | **Done** (session facts): M1 IRAM interrupt flag on both `uart_driver_install` calls (IDF "flag updated" log gone); M2 `#error` unless `CONFIG_UART_ISR_IN_IRAM` (guard proof: build with the option off fails with the `rs485_port` `#error`); M3 counter notes in `rs485_port.h` and `mb_link.h`; M4 silence check rejects 0 ms and NULL |
| K2 | Verification | `build-verifier` PASS: fresh esp32s3 build 0 warnings; `librs485_port.a` and `libmb_link.a` built; lint 372 and ELF 32 self-tests; ELF `forbidden 0 found; required: none configured`; fresh host build and run 118 / 0 / 0 |
| K3 | Re-review | `firmware-reviewer` re-review of the fix: **APPROVE** |
| K4 | Acceptance | **Accepted 2026-11-24** (user). Committed and pushed as `987fbd1` ("Add rs485_port and mb_link FC03 read path over own RTU codec (task 014)") |
| K5 | Not verified on hardware | The DE and ISR facts (rows in section 4) remain project evidence from the IDF source; on-board checks are in task 008 (Q-HW1 / Q-HW2 answered 2026-11-24: no scope, flash/NVS run deferred to phase 2; section 8) |

### Task 006 review decisions (user, 2026-11-24)

User decision in the session of 2026-11-24, not in raw (user decision, session 2026-11-24), taken after the task 006 implementation and the `firmware-reviewer` review (APPROVE WITH MINORS, section 7). The `architect` amends specs 006, 008 and 010 (`tasks/`, not touched by the wiki-keeper). **Status 2026-11-24: T1–T7 applied, re-verified and task 006 accepted 2026-11-24** (subsection "Task 006 acceptance" below); committed and pushed as `b8ad00d`.

Fix all minors before commit (user):

| # | Topic | Decision |
|---|---|---|
| T1 | Boot hint | Derived from the real state; never "silence ok" while the bus is latched |
| T2 | Status snapshot | Published at boot and when `mb silence` stops polling |
| T3 | `mb silence` and a pending manual read | `mb silence` cancels a pending manual read; small `poll_sched` API addition; spec 010 amended |
| T4 | Console UART | Runtime check that the console UART is not the bus UART, before `bus_task` starts |
| T5 | Fault wording | "faults since boot" wording; one `# mb_link: port fault ...` comment line after a fault |
| T6 | `log_out` | Priority comment on `log_out` |
| T7 | Reviewer on-site checks | The reviewer's 7 on-site checks are added to spec 008: stack high-water after boot and after `mb silence 1` (see section 8, `bus_task` 4 KB stack); abort path checked by absence of T lines; wait out `mb silence`; pending read cancelled; no "queue free spaces" line; latch lines; console port |

Verification at implementation (session facts, verified output, not in raw): `build-verifier` PASS: fresh esp32s3 build 0 warnings; lint 372 and ELF 32 self-tests; ELF `forbidden 0 found; required: mb_link_read, mb_rtu_build_fc03 present`; image 236 KB, 77% of the app partition free; no Wi-Fi, lwip, netif, BT or Matter code linked (`nvs_flash` and `esp_phy` members are force-included by the IDF startup, not used by project code); host 118 / 0 / 0; no dependency added. `firmware-reviewer`: boot order, single-owner rule, non-blocking output, console-off-bus-UART guard and timing loop verified.

The 006 image has no flash/NVS load command, so Q-HW2 (section 8) stays open. *(Update 2026-11-24: Q-HW2 decided, defer; no command added, section 8.)*

### Task 006 acceptance (user, 2026-11-24)

User decision in the session of 2026-11-24, not in raw (user decision, session 2026-11-24). The fixes below are session facts (verified output of `build-verifier` and `firmware-reviewer`), not in raw. The session labels the fixes M1–M6 and the spec checks T7; in this wiki the same items are T1–T6 (M1 = T1 … M6 = T6, T7 = spec 008 checks).

| # | Topic | Decision / status |
|---|---|---|
| V1 | T1–T7 applied | **Done 2026-11-24** (session facts): T1 boot hint derived from the real state; `silence_running` flag (confirmed by the user) and the `# silence check: started <s> s` line; `mb status` shows `boot silence: <ms> ms, activity running` for any running check. T2 status snapshot published at boot and around `mb silence`. T3 new `poll_sched_cancel_manual`, called by `mb silence` (spec 010 amended). T4 console-UART runtime check right after the hold and before `bus_task` starts; on failure nothing starts and the hold stays. T5 `faults since boot`, one `# mb_link: port fault, link closed ...` line on transition. T6 priority comment on `log_out`. T7 the reviewer's 7 on-site checks are in spec 008 (section 8) |
| V2 | Verification | `build-verifier` PASS: fresh esp32s3 build 0 warnings; lint 372 and ELF 32 self-tests; ELF `forbidden 0 found; required: mb_link_read, mb_rtu_build_fc03 present`; guard proof: a build with a missing required symbol fails; image 236819 bytes, about 77 % of the 1 MB app partition free; no Wi-Fi, lwip, netif, BT or Matter code linked; host 119 Tests, 0 Failures (new `poll_sched` cancel-manual test passes). No dependency added |
| V3 | Re-review | `firmware-reviewer` re-review: **APPROVE**. Minor (shared status text): spec aligned to the code |
| V4 | Acceptance | **Accepted 2026-11-24** (user). Firmware committed and pushed as `b8ad00d` ("Add bus_task, mb probe console and app_main (task 006)") |
| V5 | Not verified on hardware | This is the **first flashable phase-1 probe image**; it has not been run on hardware yet. The `bus_task` 4 KB stack is still to be verified on site (section 8). Q-HW1 and Q-HW2 stay open (section 8); *answered 2026-11-24, section 8* |

### Spec 008 finalisation (user, 2026-11-24)

Session facts, not in raw: the `architect` finalised spec 008 (`tasks/008-p1-bringup-hil-checklist.md`, not touched by the wiki-keeper) on 2026-11-24:
- Q-HW1 and Q-HW2 (section 8) applied: the six DE timing items of B.6 and the D.8b turnaround are "not measured (no instrument, Q-HW1)"; the flash/NVS DE run is "not done in phase 1, repeat at phase-2 bring-up (Q-HW2)".
- Indirect evidence is recorded but **not counted as a pass**: multimeter static DE level; multimeter averaged DE level during `mb probe`; complete OK replies plus counters.
- Stale console texts aligned to the accepted 006 code `b8ad00d`, e.g. `faults since boot`; `mb silence` judged by its result lines, not by `mb status`; `mb stats` has no health field; `mb status` has no heap output.

The user's answers to the architect's four questions (user decision, session 2026-11-24, not in raw):

| # | Topic | Decision |
|---|---|---|
| F1 | Heap/leak check after UART driver delete/re-install (008 B.5) | Accepted as "not checked (no heap output in the 006 image)" in phase 1; no change to 006 |
| F2 | Phase-1 exit item 6, response time (section 1) | Answered by the whole-transaction time `dur_ms` plus a derived upper bound (about `dur_ms` - 25 ms, logged "derived, not measured"); the pure slave turnaround is "unknown, no instrument". **Replaces 008-b** ("measure the slave turnaround on site", Task 014 review decisions above) |
| F3 | New hard stop (008 B.6.b) | Multimeter averaged DE level near 3.3 V during `mb probe` on the bench -> record and stop, back to the architect. Approved; listed in the Escalations of `tasks/README.md` (session fact) |
| F4 | Phase-2 DE re-check method without an instrument | Decide later; open question before the phase-2 specs (section 8) |

Status: spec 008 finalised 2026-11-24, **awaiting user approval of the spec** (session fact). *(Update 2026-11-24: approved by the user; docs in progress, see "Spec 008 approval" below.)* The basis of the "about 25 ms" figure in F2 is not stated in the session facts given to the wiki-keeper (open, see section 8).

### Task 011 review decisions (user, 2026-11-24)

Sources: session facts, verified output, not in raw (`build-verifier`, `firmware-reviewer`); user decision, session 2026-11-24. Task 011 = fake-slave host tests (section 7).

**`build-verifier` on 011: PASS** (session facts, verified output, not in raw): fresh host build 0 warnings (`-Werror`), `137 Tests 0 Failures 0 Ignored`, OK, exit 0, 2.2 s; fresh esp32s3 build 0 warnings; bus-access lint rc 0, self-test 372 ok; ELF `ok: forbidden 0 found; required: mb_link_read,mb_rtu_build_fc03 present`; 0 fake/e2e symbols in the target ELF; no firmware component or `main` code changed; dependencies only IDF (no registry components, no esp-modbus); clang-tidy SKIPPED (not installed).

**`firmware-reviewer` on 011: REQUEST CHANGES** (session facts, not in raw).

| ID | Finding | Decision (user, session 2026-11-24) |
|---|---|---|
| Major | The >= 100 ms gap rule (D16, section 3 "Timing") is asserted only in scenarios 1 and 6. In a scratch copy, `poll_sched` mutations were **not caught by any e2e test**: gap skipped for RETRY, for MANUAL, while OFFLINE; gap not recorded after RETRY or HEALTH. The real `poll_sched` code is not at fault; this is a **test-coverage gap**. Caught by the e2e tests: no RETRY, backoff, probe interval, `offline_after` 6, POLL while OFFLINE, recovery, LATCHED accepting MANUAL/SWEEP, gap 50 ms, SWEEP gap | **Fix now:** enforce the gap in the fake on every request and assert it at the end of every scenario; prove the fix with the previously uncaught mutations. Fix with `firmware-dev` (sonnet), running; then `build-verifier` + `firmware-reviewer` re-check, then user acceptance. *Done 2026-11-24: "Task 011 acceptance" below* |
| Deviations (8) | Implementer deviations (full list below), incl. `fake_tag_last()` (`ps_link_fn` carries no request kind), unconfigured registers -> exception 02, scenario 2/3 assertion forms, replay latency = `dur_ms` incl. TIMEOUT, fixtures use SWEEP with dummy times, `mkstemp` under `/tmp` | **All 8 accepted** by the reviewer |
| m2 | Scenario 10a never runs `poll_step` before the 5000 ms window, so polling in STOPPED would pass | **Open**, not fixed |
| m3 | An untagged request defaults to kind 0, so the qty > 1 / non-MANUAL check relies on the rig calling `fake_tag_last` | **Open**, not fixed |
| m4 | Temp files leak under `/tmp` if a test fails between `mkstemp` and `unlink` | **Open**, not fixed |
| m5 | `FAKE_REPLAY_MAX` 512 may be exceeded by a real on-site 008 log (`fake_load_log` returns -1); fixtures README lacks notes that replay keys on (addr, reg, qty) and ignores kind / `t_start` | **Open**, not fixed (relevant when the 008 probe log becomes a fixture, section 8) |
| m6 | No explicit "no RETRY after RETRY" assertion in scenario 2 | **Open**, not fixed |
| Info | Sweep while OFFLINE (rule 8) is caught only by the `poll_sched` unit tests, not by the e2e tests | No action recorded |

The 8 accepted implementer deviations (session facts, not in raw; completed 2026-11-24, earlier only six were named here):

| # | Deviation (accepted by the reviewer) |
|---|---|
| 1 | `fake_tag_last()`: the rig tags the last request with its kind, because `ps_link_fn` carries no request kind |
| 2 | Unconfigured registers answer exception 02 |
| 3 | Scenario 2 asserted as: the k-th POLL at >= k*60 s, <= 3 POLLs, each POLL followed by one RETRY |
| 4 | Scenario 3: recovery bound 200 s + 30 s + 500 ms + 25 ms; while OFFLINE only HEALTH on 000; probe spacing >= 30 s from the previous probe end / the OFFLINE-entering failure |
| 5 | "Unstarred fresh" read as the polled (FAST/SLOW) unstarred registers |
| 6 | Replay latency = `dur_ms`, including TIMEOUT |
| 7 | Fixtures use SWEEP with dummy times |
| 8 | `mkstemp` under `/tmp` in scenarios 8c and 9 |

### Task 011 acceptance (user, 2026-11-24)

Sources: session facts, verified output, not in raw (`firmware-dev`, `build-verifier`, `firmware-reviewer`); user decision, session 2026-11-24.

| # | Item | Content |
|---|---|---|
| G1 | Major fix (`firmware-dev`, sonnet) | `fake_link` fails the test if a request starts < `FAKE_DEFAULT_GAP_MS` (100 ms) after the end of the previous request (end-to-start, D16, section 3 "Timing"); the first request is exempt; no setter, all scenarios use 100 ms. `assert_gaps_respected` runs at the end of every scenario. New e2e cases **11a** (MANUAL requested right after a request) and **11b** (MANUAL right after the failure that enters OFFLINE) |
| G2 | Mutation proof (implementer) | All caught: (a) RETRY gap skipped, (b) MANUAL gap skipped, (c) gap not recorded after RETRY, (d) not recorded after HEALTH, (e) gap skipped while OFFLINE, (f) gap 50 ms. 11a/11b are the only e2e tests that catch "gap skipped for MANUAL" and "gap skipped while OFFLINE" |
| G3 | `build-verifier` re-check | **PASS**: fresh host build 0 warnings, `139 Tests 0 Failures 0 Ignored`, OK, exit 0, 2.1 s; `components/` and `main/` unchanged; bus-access lint rc 0, self-test 372; target build skipped (no firmware change) |
| G4 | `firmware-reviewer` re-review | **APPROVE**. Independently re-ran mutations (b), (d), (e): all caught. Every return path of `fake_link` updates `last_end_ms`; the `>= 100` boundary matches `poll_sched`. New minor **m7**: the wait-for-OFFLINE loop in 11b has no iteration cap, so a regression would hang the suite instead of failing it. The reviewer recommended amending spec 011 for scenario 11 |
| G5 | Acceptance | **Task 011 accepted as is, 2026-11-24** (user). Minors m2–m6 and m7 stay open. Code committed as `17825cb` ("Add fake slave and end-to-end poll tests with fixtures (task 011)"); push follows after the docs commit (session fact) |
| G6 | Spec 011 not amended (user) | Scenarios 11a/11b and the in-fake gap rule are **recorded extra scope beyond spec 011**, not a spec deviation. A later review should not flag them as unspecified |

m7 is test-suite robustness only; it does not affect on-site use, so it has no section 8 entry *(inferred)*.

### Spec 008 approval (user, 2026-11-24)

Spec 008 (section "Spec 008 finalisation" above) was **approved by the user on 2026-11-24** (user decision, session 2026-11-24, not in raw). `firmware-dev` (sonnet) is writing `docs/phase1-probe/checklist.md` and `docs/phase1-probe/measurement-log-template.md` (in progress, session fact); then `firmware-reviewer`, then user acceptance.

### Recorded conflict (D19)

**Conflict (D19, recorded, not resolved):** the manual gives the address range 1–255 for reg 200 ([[concepts/device-addressing]], p4) while the Modbus spec reserves 248–255 *(external)*; the node accepts only 1..247. Also not stated: which port the address applies to, and "Address 001 – Leave free!" (p7) vs default 1 ([[concepts/device-addressing]]).

## 7. Task impact

Status 2026-11-09 (architect specs, `tasks/README.md`): 001, 009, 010, 002, 006, 011, 008 **spec ready**; 003/004/005/007 retired. **Update 2026-11-09 (session implementation facts, verified build/test output; not in raw):** task **001 done**: implemented by `firmware-dev` and accepted on 2026-11-09 after two review rounds (`firmware-reviewer` accepted; `build-verifier` all checks pass); committed in the session-end commit together with the amended spec `tasks/001-skeleton-kconfig-lint.md`. **Next: 009**, then 010, 002, 006, 011, 008. Open minor: the lint self-test has no explicit cases for `^=`, `-=`, `*=`, `/=`, `%=`, `>>=` (the R8 rule catches them; follow-up with 009 or later). Host-build details: [[concepts/dev-environment]]. **Update 2026-11-24 (session implementation facts, verified build/test output; not in raw):** task **009 done** (accepted, committed and pushed, see `log.md` 2026-11-24). Task **010** (`reg_model` + `poll_sched` + `txn_log`) **implemented** by `firmware-dev`; **verified** by `build-verifier` (PASS: esp32s3 target build 0 warnings; host tests 85, 0 failures, 0 ignored; bus-access lint + 82-case self-test clean; dependencies registry-only; clang-tidy skipped, not installed); `firmware-reviewer`: **APPROVE WITH MINORS** (no blocker/major). **Update 2026-11-24, task 010 accepted (session facts, verified output, not in raw; not committed):** spec 010 amendment (U1–U6 and reviewer minors, section 6) applied by `firmware-dev` (sonnet): new `reg_model_set_exception`, `poll_sched` start is a no-op unless STOPPED, RETRY in rule 6, rule 12 address mismatch, txn_log enum bounds, tests for U1–U6, rule 12, sweep end and a 2 h mixed-kind gap soak. Re-check by `build-verifier`: PASS (esp32s3 fresh build 0 warnings; host tests 100 Tests, 0 Failures, 0 Ignored, exit 0; bus-access lint and 82-case self-test clean; no `esp_`/FreeRTOS includes in pure components; dependencies registry-only; clang-tidy SKIPPED, not installed). `firmware-reviewer` re-review: **APPROVE**, no findings of substance. FYI (no change): `mb poll off` then `mb poll on` from OFFLINE restarts polling immediately (consequence of U4, bounded by the 5-failure re-entry). **Task 010 done, accepted 2026-11-24.** Next: 002 (esp-modbus master spike / `mb_link`, bus-touching; spec escalations S3/S5/S6/S12 stop for a user decision), then 006, 011, 008. Open hygiene minor: host build prints 4 CMake `component_validation` warnings because `firmware/test/host/main/CMakeLists.txt` globs component `test_host` sources into main (pre-existing pattern from 009, cosmetic). The 002 spec lives in `tasks/002-esp-modbus-master-spike-mb-link.md`; the old `tasks/002-esp-modbus-crc-adapter-spike.md` is a pointer, to be removed with `git rm` at commit. The 002 spec now lives in `tasks/002-esp-modbus-master-spike-mb-link.md`; the old `tasks/002-esp-modbus-crc-adapter-spike.md` is a pointer, to be removed with `git rm` at commit. **Update 2026-11-24, task 002 stopped (session facts, not in raw):** the 002 spike (`firmware-dev`) **stopped at escalation S3**; finding confirmed by `firmware-reviewer` and the orchestrator; report `docs/spikes/002-esp-modbus.md` *(project evidence)*; no `mb_link` / `rs485_port` code left in the tree. The user chose **E2** (section 6 "Task 002 spike decisions"). **Task 002 is replaced by three tasks** written by the `architect` (specs being written, 2026-11-24; numbers and file names not yet known to the wiki): (1) pure `mb_rtu`; (2) `rs485_port` + `mb_link` over the UART; (3) lint/ELF update. Then 006, 011, 008. **Update 2026-11-24, specs 012–014 ready (session facts, not in raw; not committed):** the `architect` wrote `tasks/012-mb-rtu-frame-codec.md`, `tasks/013-lint-elf-check-no-esp-modbus.md` (must land before 014) and `tasks/014-rs485-port-mb-link.md`; 002 is now a retired note; 006, 008, 010, 001 and `tasks/README.md` amended by the architect. Order (user, 2026-11-24): **012 -> 013 -> 014 -> 006 -> 008; 011 any time**; all on sonnet; independent `firmware-reviewer` review for each of 012/013/014. The open lint-self-test minor (compound operators) is scheduled in 013. **Update 2026-11-24, task 012 accepted (session facts, verified output, not in raw; not committed):** `firmware-dev` (sonnet) implemented pure `mb_rtu` (`firmware/components/mb_rtu/`, host REQUIRES in `firmware/test/host/main/CMakeLists.txt`): CRC-16/MODBUS, `mb_rtu_build_fc03`, reply classifier, `mb_rtu_fc03_reply_len`, `mb_rtu_t35_us`. `build-verifier` PASS: esp32s3 fresh build 0 warnings (`mb_rtu` compiled, not yet linked into the image); host tests 118 Tests, 0 Failures, 0 Ignored; bus-access lint rc 0 and self-test 82 ok; no `esp_` / FreeRTOS / driver includes; only the two function-code `#define`s (0x03, 0x83) in `src`; registry-only, no esp-modbus (`managed_components/` empty); clang-tidy SKIPPED (not installed). `firmware-reviewer`: **APPROVE WITH MINORS**; independent ASan/UBSan build: builder (1, 0, 1) gives `01 03 00 00 00 01 84 0A`, CRC vectors 0x4B37 and 0x1241, t3.5 at 9600 = 4011 us, 3M random-frame fuzz clean. Minors: (a) `val == NULL` moved to the caller-error rule (BAD_FRAME, after the TIMEOUT rule), spec 012 rule 2 amended; (b) the 19201 t3.5 test changed to 1750; both applied by `firmware-dev` (haiku), re-verified 118/0/0, 0 warnings, lint rc 0. (c) `mb_rtu_fc03_reply_len` does not validate qty: handled as an amendment of spec 014 (reply-length adapter in `mb_link` maps UNKNOWN and any length > `MB_RTU_MAX_REPLY_LEN` to "more", so the deadline ends collection; rx buffer `MB_RTU_MAX_REPLY_LEN + 1` with a write guard; acceptance 3k). **Task 012 done, accepted 2026-11-24; not committed.** Next: 013, then 014, 006, 011, 008.

**Update 2026-11-24, task 013 first pass (session facts, verified output, not in raw; not committed):** `firmware-dev` (sonnet) rewrote R1/R3/R4/R8 for E2 (self-test 218 cases) and added `firmware/tools/check_elf_symbols.py` (24 self-test cases, stdlib only) as POST_BUILD hook on the ELF (`CMAKE_NM` = `xtensa-esp32s3-elf-nm`); `DMT_ELF_REQUIRED_SYMBOLS` is empty until task 006. esp-modbus is gone from the manifests, the lock file and `managed_components`. `build-verifier` PASS: esp32s3 build 0 warnings; host tests 118 / 0 failures / 0 ignored; ELF summary `forbidden 0 found; required: none configured`. Guard proofs: five deliberately bad builds failed the build as expected. One proof needed a rework: an empty dummy function was inlined away, so a `noinline` function with a side effect was used for the ELF guard proof. `firmware-reviewer`: **APPROVE WITH MINORS** with a bypass table (not in the wiki yet, open). **User decisions 2026-11-24 (section 6 "Task 013 review decisions", L1–L9):** fix now (spec 013 amended by the architect, then `firmware-dev` follow-up, then `build-verifier` + `firmware-reviewer`), then accept; commit only on request. The compound-operator self-test minor (`^= -= *= /= %= >>=`) is **closed by 013**. Next: 013 follow-up, then 014 (with the R8 consequence L9), 006, 011, 008.

**Update 2026-11-24, task 013 accepted (session facts, verified output, not in raw; not committed):** the task 013 follow-up (L1–L9, spec 013 amended by the architect, applied by `firmware-dev` on sonnet) was verified by `build-verifier` PASS (esp32s3 fresh build 0 warnings; lint self-test 343 cases in 0.24 s, tree scan rc 0; ELF self-test 32 cases, fails on empty `nm` output or missing `app_main`, `--nm /bin/true` exit 1; ELF summary `forbidden 0 found; required: none configured`). `firmware-reviewer` re-review: **APPROVE WITH MINORS**, all earlier findings closed. Not re-run: host tests (blocked by the permission classifier; no C code changed in 013; `firmware-dev` reported 118 / 0 / 0). **User accepted task 013 "as is" on 2026-11-24** (section 6 "Task 013 acceptance"). Open: ROM/register TX ban (row A1, decided, not implemented) and minors (A3). Next: the lint follow-up (A1 + minors), then 014, 006, 011, 008. The commit is pending the user's request.

**Update 2026-11-24, 013 follow-up done and accepted (session facts, verified output, not in raw):** commit `9ca4517` ("Ban ROM and register UART TX routes in bus-access lint (013 follow-up)") is committed and pushed (reported by the caller; in local `git log`). `firmware-dev` (sonnet) implemented rows A5–A12: R4 bans (section 3), no blanket `esp_rom_*` ban, documented limits, 5 s timing guard; lint self-test 372 cases (0.3 s). `build-verifier` PASS: fresh esp32s3 build 0 warnings; ELF `forbidden 0 found; required: none configured`; fresh host build and run 118 / 0 / 0 (closes the host-run gap of 013). `firmware-reviewer`: APPROVE WITH MINORS (200-character window acceptable). **User accepted the follow-up as is, 2026-11-24** (section 6 row A13). Open doc minors, not fixed: section 6 row A14 (a)–(c). **Task 013 done.** **Task 014 started 2026-11-24:** `firmware-dev` (sonnet) implements `rs485_port` + `mb_link` (bus-touching, first TX code); next `build-verifier` and an independent `firmware-reviewer`; spec 014 escalation conditions are hard stops. Then 006, 011, 008.

**Update 2026-11-24, 013 pushed; follow-up started (user decisions, session 2026-11-24, not in raw):** commits `fc5b50e` (013 lint/ELF check) and `19a3e94` (013 acceptance) are pushed (user-reported; both appear in the local `git log`). The ROM/register TX ban and the minors are in section 6 rows A5–A12 (Q-a yes, Q-b yes, Q-c no). The follow-up is handed to `firmware-dev` (sonnet). Next: `build-verifier` and `firmware-reviewer` on the follow-up, then accept; commit on request. Then 014, 006, 011, 008.

**Update 2026-11-24, task 014 implemented and verified (session facts, verified output, not in raw; not committed):** `firmware-dev` (sonnet) implemented `rs485_port` + `mb_link` (own FC03 RTU master over the IDF UART in RS485 half-duplex, DE via RTS). No escalation condition of spec 014 was hit. `build-verifier` PASS: fresh esp32s3 build 0 warnings; `librs485_port.a` and `libmb_link.a` built; bus-access lint 372 and ELF self-tests 32; ELF `forbidden 0 found; required: none configured`; fresh host build and run 118 / 0 / 0; no dependency added. `firmware-reviewer`: **APPROVE WITH MINORS**. The reviewer verified the DE and ISR facts in the local IDF v6.0.2 source (section 4, project evidence) and found that no reply byte can be lost to the ISR RX-FIFO reset. The load-bearing point is `CONFIG_UART_ISR_IN_IRAM=y` (section 3). **User decisions (2026-11-24):** fix minors M1–M4 now, then re-check, accept and commit; 006 and 008 specs amended (section 6 "Task 014 review decisions"). **Status: M1–M4 in progress (`firmware-dev`), not yet re-checked.** Open: the reviewer's full minor text is not in the wiki (M3 reason).

**Update 2026-11-24, task 014 accepted (session facts, verified output, not in raw; pushed as `987fbd1`):** the M1–M4 fixes (section 6 "Task 014 review decisions") were applied by `firmware-dev` (sonnet), re-checked by `build-verifier` (PASS, see section 6 "Task 014 acceptance", K2) and the `firmware-reviewer` re-review (APPROVE). **User accepted task 014 on 2026-11-24**; the firmware is committed and pushed as `987fbd1`. **Task 014 done.** Next: 006 (bus_task, probe console, `app_main`; firmware-dev sonnet, then build-verifier and firmware-reviewer), 011 (can run in parallel), then 008. Task 008 needs the user's answers to open questions Q-HW1 and Q-HW2 (section 8). *(Update 2026-11-24: answered; spec 008 finalised 2026-11-24 applying them and decisions F1–F4, awaiting user approval of the spec; section 6 "Spec 008 finalisation".)*

**Update 2026-11-24, task 006 accepted (session facts, verified output, not in raw):** T1–T7 applied (section 6 "Task 006 acceptance", V1–V2), `firmware-reviewer` re-review APPROVE. **User accepted task 006 on 2026-11-24**; the firmware is committed and pushed as `b8ad00d`. **Task 006 done.** It is the first flashable phase-1 probe image; it has not been run on hardware. Next: 011 (fake-slave host tests, firmware-dev sonnet), then 008 (on-site checklist, needs Q-HW1 / Q-HW2). *(Update 2026-11-24: 008 finalised 2026-11-24 applying Q-HW1/Q-HW2 and F1–F4, awaiting user approval of the spec.)* The on-site probe by the user follows, then the probe log goes into `raw/` for `/capture`.

**Update 2026-11-24, Q-HW1 / Q-HW2 answered, 011 started (user decision and session facts, session 2026-11-24, not in raw):** Q-HW1: no scope or logic analyser (008 B.6 DE timing and D.8b turnaround "not measured"); Q-HW2: defer the flash/NVS DE run to phase-2 bring-up, no test command in 006 (section 8). Task 011 (fake-slave host tests) handed to `firmware-dev` (sonnet), **in progress, not verified**. The `architect` is finalising spec 008 with these answers *(update 2026-11-24: finalised, awaiting user approval, update below)*. Next: 011 verification (`build-verifier`, `firmware-reviewer`, user acceptance), then the finalised 008 spec for user review.

**Update 2026-11-24, task 006 implemented and verified, minors in progress (session facts, verified output, not in raw; not committed):** `firmware-dev` (sonnet) implemented `bus_task` (single owner of the bus), the probe console `mb ...` and `app_main`: the first flashable probe image. `build-verifier` PASS and `firmware-reviewer` APPROVE WITH MINORS (details and checks in section 6 "Task 006 review decisions", T1–T7). The user decided to fix all minors before commit. **Next:** the architect amends specs 006, 008 and 010 (running); then `firmware-dev` applies T1–T6, then `build-verifier` and `firmware-reviewer` again, then user acceptance; commit only on request. Then 011 and 008. **`bus_task` stack of 4 KB** is not yet proven: it is checked on site (section 8).

**Update 2026-11-24, 008 spec finalised, 011 implemented (session facts and user decisions, session 2026-11-24, not in raw; not committed):** the `architect` finalised spec 008 with Q-HW1/Q-HW2 applied and the console texts aligned to `b8ad00d`; the user answered the architect's four questions (section 6 "Spec 008 finalisation", F1–F4; F2 replaces 008-b). **008 now awaits the user's approval of the spec.** Task 011 was implemented by `firmware-dev` (sonnet): `firmware/test/host/main/fake_slave.{h,c}`, `test_e2e_poll.c`, fixtures `firmware/test/fixtures/{README.md,healthy-synthetic.log,starred-timeout-synthetic.log}`. Implementer report (not yet independently verified): host 137 Tests, 0 Failures, 0 Ignored (was 119), 0.04 s; all 10 scenarios fail under deliberate mutation; no contradiction with spec 010; no firmware component code changed (orchestrator confirmed via `git status`). Deviation, pending review: `fake_tag_last()` added because `ps_link_fn` carries no request kind. **Status: 011 implemented, not yet verified**; `build-verifier` and an independent `firmware-reviewer` are running. Next: 011 verification results -> user acceptance; user approves the 008 spec -> `firmware-dev` writes the docs -> `firmware-reviewer`; then flash and the bench/on-site probe. *(Update 2026-11-24: 011 verified, review REQUEST CHANGES, update below.)*

**Update 2026-11-24, task 011 verified and reviewed (session facts, verified output, not in raw; user decision, session 2026-11-24; not committed):** `build-verifier` **PASS** (host 137 / 0 / 0, 0 warnings, 2.2 s; esp32s3 0 warnings; lint 372 ok; ELF required `mb_link_read`, `mb_rtu_build_fc03` present; 0 fake/e2e symbols in the target ELF; no component code changed). `firmware-reviewer` **REQUEST CHANGES**: one major (D16 gap rule asserted only in scenarios 1 and 6; five `poll_sched` gap mutations not caught by any e2e test; test-coverage gap, not a code fault); all 8 implementer deviations accepted (incl. `fake_tag_last()`); minors m2–m6 (section 6 "Task 011 review decisions"). **User decision:** fix the major only (gap enforced in the fake on every request + asserted at the end of every scenario, proven with the uncaught mutations); **minors m2–m6 stay open**. **Status: 011 major fix in progress** (`firmware-dev`, sonnet); then `build-verifier` + `firmware-reviewer` re-check, then user acceptance. 008 spec still awaits user approval. *(Update 2026-11-24: fix done, 011 accepted, `17825cb`; 008 approved; update below.)*

**Update 2026-11-24, task 011 accepted, spec 008 approved (session facts, verified output, not in raw; user decision, session 2026-11-24):** the gap major is fixed (gap enforced in `fake_link` on every request, asserted at the end of every scenario, new e2e cases 11a/11b beyond spec 011); `build-verifier` re-check PASS (host 139 / 0 / 0, 0 warnings); `firmware-reviewer` re-review **APPROVE** with one new minor m7 (section 6 "Task 011 acceptance", G1–G6). **User accepted task 011 as is**; minors m2–m7 stay open; spec 011 not amended (11a/11b and the in-fake gap rule are recorded extra scope). Code committed as `17825cb`; push after the docs commit. **Task 011 done.** **Spec 008 approved by the user**; `firmware-dev` (sonnet) is writing `docs/phase1-probe/checklist.md` and `docs/phase1-probe/measurement-log-template.md`, then `firmware-reviewer`, then user acceptance. Then the user flashes the probe image (bench items) and runs the on-site probe.

| Task | Change |
|---|---|
| 001 | modify: new Kconfig (UART port, RX/TX/DE GPIO, baud, slave address, timeouts, poll periods, `CONFIG_DMT_BUS_WRITE` = n); lint -> `check_bus_access.py`; host runner unchanged |
| 002 | **retired 2026-11-24** (note only; replaced by 012/013/014). Was: modify -> esp-modbus master spike + `mb_link` read path + `rs485_port` (CRC adapter dropped). Must establish: DE signal, silent create/start, error distinguishability incl. exception code, internal retries / t3.5, UART ownership and the order of `uart_set_pin` / `uart_set_mode`. **2026-11-24: stopped at S3; replaced by three tasks (pure `mb_rtu`; `rs485_port` + `mb_link` over UART; lint/ELF update), specs being written by the architect** |
| 012 | new 2026-11-24, **done, accepted 2026-11-24** (not committed): pure `mb_rtu` (CRC16, FC03 builder, reply classifier), see the update above |
| 013 | new 2026-11-24, **done, accepted "as is" 2026-11-24**; commits `fc5b50e`, `19a3e94` and follow-up `9ca4517` pushed; follow-up done and accepted (section 6 rows A5–A13): lint + ELF check without esp-modbus; L1–L9 applied (section 6); ROM/register TX ban (row A1) and minors (A3) applied; open doc minors (a)–(c) in row A14; compound-operator self-test minor closed |
| 014 | new 2026-11-24, **done, accepted 2026-11-24, committed and pushed as `987fbd1`** (section 6 "Task 014 review decisions" M1–M4 and "Task 014 acceptance"; `firmware-reviewer` APPROVE WITH MINORS, re-review APPROVE); spec amended 2026-11-24 (session fact): reply-length adapter in `mb_link` (UNKNOWN or > `MB_RTU_MAX_REPLY_LEN` = "more"), rx buffer `MB_RTU_MAX_REPLY_LEN + 1` with write guard, acceptance 3k; `rs485_port` + `mb_link` read path over the IDF UART, counters |
| 003 | retire |
| 004 | retire (ideas go into `reg_model` / `poll_sched`) |
| 005 | retire (RX-only listen survives as the silence check in `rs485_port`) |
| 006 | modify: poll loop + probe console (`bus_task`, `mb` console, `app_main`; first flashable probe image). **Done, accepted 2026-11-24, committed and pushed as `b8ad00d`** (section 6 "Task 006 review decisions" T1–T7 and "Task 006 acceptance" V1–V5); `firmware-reviewer` APPROVE WITH MINORS, re-review APPROVE; not yet run on hardware |
| 007 | retire, replaced by 011 |
| 008 | modify: on-site probe checklist + log template, items 1–11 (section 1). **Spec finalised 2026-11-24** (Q-HW1/Q-HW2 applied, console texts aligned to `b8ad00d`, F1–F4 in section 6 "Spec 008 finalisation"); ~~awaiting user approval of the spec~~ **spec approved by the user 2026-11-24**; docs `docs/phase1-probe/checklist.md` and `measurement-log-template.md` **in progress** (`firmware-dev`, sonnet), then `firmware-reviewer` and user acceptance (section 6 "Spec 008 approval") |
| 009 | new: `daikin_regs` |
| 010 | new: `reg_model` + `poll_sched` |
| 011 | new: fake-slave host tests. **Implemented 2026-11-24, not yet verified** (`build-verifier` and `firmware-reviewer` running; update below). *2026-11-24: `build-verifier` PASS (137 / 0 / 0); `firmware-reviewer` REQUEST CHANGES (1 major: D16 gap coverage; 8 deviations accepted; minors m2–m6 open by user decision); major fix in progress, then re-check and user acceptance (section 6 "Task 011 review decisions")*. **Done, accepted as is 2026-11-24, committed as `17825cb`** (push after the docs commit); host 139 / 0 / 0; re-review APPROVE; minors m2–m7 open; 11a/11b and the in-fake gap rule are extra scope beyond spec 011, spec not amended (section 6 "Task 011 acceptance") |

Order (2026-11-24, user): 001, 009, 010, 012, 013 and its follow-up (all committed and pushed, latest `9ca4517`), then **014 (done, accepted, pushed as `987fbd1`) -> 006 (done, accepted 2026-11-24, pushed as `b8ad00d`) -> 008; 011 any time**. Next: 011, then 008. *(Update 2026-11-24: 011 implemented, verification running; 008 spec finalised, awaiting user approval.)* *(Update 2026-11-24: 011 review REQUEST CHANGES, major fix in progress, minors m2–m6 open.)* *(Update 2026-11-24: 011 done, accepted, committed `17825cb`; 008 spec approved, docs in progress. Next: 008 docs -> `firmware-reviewer` -> user acceptance -> commit; then flash the probe image, bench items, on-site probe.)* *(Was: 001, 009, 010, 002, 006, 011, 008; before the 013 acceptance: 013 -> 014 -> 006 -> 008.)*

## 8. Open items and risks

- **Question Q-HW1 (asked 2026-11-24; answered 2026-11-24):** is an oscilloscope or logic analyser available on the bench or on site? Needed for `tasks/008` B.6 (DE level on a scope) and D.8b (slave turnaround). If not, these are logged as **"not measured"** in the probe log (session facts 2026-11-24, not in raw).
  **Answer (user decision, session 2026-11-24, not in raw):** **no** oscilloscope and **no** logic analyser, neither on the bench nor on site. Consequence: 008 B.6 (DE timing) and D.8b (slave turnaround) are logged **"not measured"**; only console counters and timestamps are evidence.
- **Question Q-HW2 (asked 2026-11-24; decided 2026-11-24):** the phase-1 image cannot generate flash/NVS load, so the `tasks/008` B.6 run "DE with concurrent flash/NVS activity" is **not done in phase 1; repeat at phase-2 bring-up**, unless the user wants a test console command added to task 006 for it (session facts 2026-11-24, not in raw).
  **Decision (user decision, session 2026-11-24, not in raw):** **defer.** 008 B.6 "DE with concurrent flash/NVS activity" is "not done in phase 1, repeat at phase-2 bring-up". No test console command is added to the 006 image.
- **Residual risk (new 2026-11-24, from the Q-HW1 / Q-HW2 answers):** DE timing stays **unmeasured in phase 1**, including the load-bearing assumption that the TX_DONE ISR runs from IRAM (`CONFIG_UART_ISR_IN_IRAM=y`, build guard M2, section 3; risk bullet below). Phase-1 evidence for DE and slave turnaround is limited to console counters and timestamps. Re-check DE timing (incl. under flash/NVS load) at phase-2 bring-up where possible; without a scope or logic analyser the method for that re-check is still open *(inferred; user decision, session 2026-11-24)*. Indirect phase-1 evidence (multimeter static and averaged DE level, complete OK replies + counters) is recorded but not counted as a pass (spec 008 finalised, section 6 "Spec 008 finalisation").
- **Open question (before the phase-2 specs; user decision F4, session 2026-11-24):** how is DE timing (incl. under flash/NVS load) re-checked at phase-2 bring-up without a scope or logic analyser? Decide later.
- **Hard stop (new 2026-11-24, 008 B.6.b, F3; user decision, session 2026-11-24):** a multimeter averaged DE level near 3.3 V during `mb probe` on the bench -> record and stop, back to the architect.
- **Note for turning the 008 probe log into a fixture (task 011 minor m5, open; session facts, not in raw; user decision, session 2026-11-24):** the fake slave holds at most `FAKE_REPLAY_MAX` = 512 replay entries; a real on-site 008 log may exceed this, and `fake_load_log` then returns -1. Replay keys on (addr, reg, qty) and ignores the request kind and `t_start`; the fixtures README does not say so yet. Check both before using the probe log as a replay fixture (section 6 "Task 011 review decisions").
- **Open (wiki-keeper note):** the basis of the "about 25 ms" subtracted from `dur_ms` in F2 (section 6) is not in the session facts given to the wiki; ask the architect.
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
- *(Closed 2026-11-24)* esp-modbus facts to verify in the task-002 spike *(external)*: DE signal, silent create/start, error granularity, internal retries, handling of addresses > 247. Answered by the spike *(project evidence)*; error granularity insufficient (S3) -> E2; DE = RTS (section 4).
- **Risk (new 2026-11-24, E2):** own code now sits in the **TX path** (`mb_rtu` framing/CRC, `rs485_port` UART + DE handling) instead of a registry library; mitigation is review (`firmware-reviewer`, code-inspection checklists incl. the DE rules of section 4) and host tests of `mb_rtu` *(inferred)*.
- **Risk (new 2026-11-24, E2; corrected same day):** the **inter-byte timing of the slave answering on "A A B B -"** (EKWHCTRL1, *inferred*, [[entities/ekwhctrl1]]) is unknown (not stated in the manual); the own frame-end detection depends on it, and with `CONFIG_FREERTOS_HZ` = 100 the end-of-frame silence window is 10–20 ms (known limit, user 2026-11-24). Checked on site by task 008 D.8a ("the answering slave", max inter-byte interval from `mb_link_get_counters`) *(inferred)*. *(Correction: the earlier text named the INN-FR-B32 reply; that was an orchestrator wording error, user/orchestrator 2026-11-24.)*
- Fallback E1b (esp-modbus) remains available if E2 is too hard to debug on site; revert is a separate user decision (section 2).
- **Risk (new 2026-11-24, task 014 review):** a flash or NVS stall (Matter activity) during our transmit can delay the DE release if the TX_DONE ISR is not in IRAM, and DE then stays high on the shared line. Mitigation: `CONFIG_UART_ISR_IN_IRAM=y` with build guard M2 (section 3); concurrent flash/NVS activity is one of the DE checks on site (task 008, section 6 "Task 014 review decisions") *(inferred mechanism; reviewer, project evidence)*.
- **Risk (new 2026-11-24, task 014):** `gap_bytes` and `max_byte_interval_us` are approximate (M3). Quote them only as bounds in the probe log.
- **Risk (new 2026-11-24, task 006; still open after acceptance):** the `bus_task` stack of 4 KB is not yet proven. Keep it as a to-verify risk: check on site with the stack high-water mark after boot and after `mb silence 1` (task 008 reviewer check, section 6 "Task 006 review decisions" T7); if the margin is too small, the stack size is a new user decision *(session facts, not in raw)*.
- **Open (2026-11-24, after 006 acceptance):** the probe image has not run on hardware. *(Q-HW1 and Q-HW2 answered 2026-11-24, above. Update 2026-11-24: spec 008 finalised with these answers, awaiting user approval; section 6 "Spec 008 finalisation". Update 2026-11-24: spec 008 approved, docs in progress; section 6 "Spec 008 approval".)*
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
