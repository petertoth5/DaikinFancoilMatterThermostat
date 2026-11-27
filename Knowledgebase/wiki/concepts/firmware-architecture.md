---
title: Firmware architecture v0.3 (approved 2026-11-08, user)
type: concept
sources: ["project-brief.md", "EKWHCTRL0.EKWHCTRL1.EKRTCTRL1_Installation manual_N420384A_English.pdf", "EKWHCTRL1,EKRTCTRL1,EKWHCTRL0_Installation manual_N420384A ModBus RTU_English.pdf", "MAX485-M_EN_10035508.pdf", "20261107_192127.jpg"]
updated: 2026-11-28
---

# Firmware architecture (approved v0.3)

**Status:** v0.3 written by `architect`, **approved by the user on 2026-11-08** (Q1–Q10 all answered with the architect's recommendation; user decision, session 2026-11-08, not in raw). Supersedes v0.2 (2026-11-07). Later user decisions (sessions 2026-11-09 and 2026-11-24, not in raw) are in section 6 and are applied in sections 1–5. The architecture as a whole, every D-number and every default value are user decisions unless tagged otherwise. **Slimmed 2026-11-24** (user decision S2, session 2026-11-24): progress narratives, verification output, review-round details, superseded texts, closed questions and the superseded v0.2 table moved verbatim to [[concepts/firmware-history]]. Header addenda history: [[concepts/firmware-history]] §1; pre-slimming wording of the sections kept here: §10.

Tags: *(external)* = Modbus spec, ESP registry, esp-modbus / IDF / esp_matter / CHIP sources, RS485 standard; not in raw. *(project evidence)* = project document `docs/spikes/002-esp-modbus.md` (task-002 spike report, 2026-11-24, source reading of esp-modbus 2.1.4 and IDF v6.0.2, no hardware attached), verified by `firmware-reviewer` and the orchestrator; not a raw source; file:line references are into esp-modbus (`EM/` = `firmware/managed_components/espressif__esp-modbus/modbus/`, `EMR/` = its repo root) or IDF (`IDF/` = `~/esp/esp-idf-v6.0.2/components/`) as in that report. *(inferred)* = design conclusion, not stated in any source. *Session facts* = verified tool output reported in the session, not in raw. Register facts cite [[concepts/register-map]], [[concepts/flag-registers]], [[concepts/web-restriction-flags]], [[concepts/device-addressing]] (all from [[sources/n420384a-english]] p3–p7, values identical in [[sources/n420384a-modbus-rtu]]). Context: [[concepts/project-goal]], [[concepts/bus-topology]], [[entities/ekwhctrl1]], [[concepts/dev-environment]]. Task specs: `tasks/README.md`.

Q1–Q10 as answered (user, 2026-11-08): Q1 DE + /RE tied to RTS with 10 kΩ pull-down (D8); Q2 233 = 0 with STAT unavailable -> last explicit season, default Heat, console flag "season auto, not mirrored" (D15); Q3 timing defaults accepted (D16); Q4 boot silence check yes (D17); Q5 247 read-only permanently (D18); Q6 address from Kconfig default 1 + RAM console override, no scan (D19); Q7 write caps accepted (D22); Q8 gate exception: stage-1 (probe) specs before the on-site items, stage-2/3 specs only after the probe log (D2; **partly superseded 2026-11-24** by the phase-2a gate change, section 6 "Working mode and gate"); Q9 esp-modbus `==2.1.4` (**superseded 2026-11-24**: dependency removed with E2); Q10 Thermostat + Fan mapping (D3/D4) re-approved.

## 1. Phases and exit criteria

Tap point and role (D1): the node connects to one A and one B of the EKWHCTRL1 **"A A B B -"** ModBus RTU port and is the **sole Modbus master** there; the **"+ A B -"** fancoil link is never touched (hard rule) ([[concepts/bus-topology]], [[entities/ekwhctrl1]], [[sources/n420384a-english]] p3). Framework rules: ESP-IDF v6.0.2, esp32s3, registry-only components ([[sources/project-brief]], [[concepts/dev-environment]]).

### Phase 1 – on-site read probe
No Matter, no Wi-Fi, FC03 only, write code compiled out (`CONFIG_DMT_BUS_WRITE=n`, section 3) **in the read image**; the separately built write image (task 019, amendment below) adds console-only FC06 writes. Polling starts automatically after a clean 5 s boot silence check (user, 2026-11-09, section 6). Results go into a **measurement log** -> `Knowledgebase/raw/` -> `/capture`. Exit criteria:

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

**Amended 2026-11-27 (user, OS6, section 6):** on the upcoming site visit (same trip, current MAX485-M setup per OS1–OS5), **after** the read probe, the user wants a **control test via console commands in the probe image**: setpoint, on/standby, fan speed, season/mode. This brings phase-3 writes forward for the probe image and amends "write code compiled out" above for that image. ~~Design being drafted by the `architect`; no spec yet, write scope and safety rules not decided.~~ **Approved 2026-11-27 (user decisions W1–W14 on the architect's console-write design, section 6 "Console-write decisions"):** writes are console-only and live in a **separately built phase-1 write variant** (W1, W3: `CONFIG_DMT_BUS_WRITE` via overlay `sdkconfig.defaults.write`, build dir `build_write/`); the Matter image stays write-free until phase 3 (W1). **One image for the trip** (W4): the write image is flashed on the bench before the trip and used **disarmed** for the read probe C/D and armed for the control test E; no flashing on site. Write set W5 (231, 201 bit 7, 201 bits 0–2, 233); on-site writes gated at run time and by checklist E.0 (W2, W14). Implementation = task 019, **code complete 2026-11-28** (019a `2d8ec44`, 019b `f65f465`; section 7); acceptance pending the bench run B.W.1–B.W.8 with the write image. If B.W does not pass before the trip, the trip stays read-only (section 8).

### Phase 2 – Matter read-only mirror
Thermostat endpoint first, then Fan (D3); Matter writes rejected. Exit: 24 h without panel anomaly; values match the panel display; bus unplugged -> attributes go stale/null within the threshold (section 5) and recover.
- **Phase 2a (user decision, session 2026-11-24; plan approved, section 6 "Phase-2a plan"):** separate IDF project `firmware/matter/` (own manifest with `espressif/esp_matter` ^1.6.1, own `sdkconfig.defaults` and partition table for 16 MB flash (16 MB inferred from N16R8), BLE commissioning per D11); reuses `daikin_regs` and `reg_model` via `EXTRA_COMPONENT_DIRS`; **no bus code** (no `bus_task`, `mb_link`, `rs485_port`, `mb_rtu`), DE not driven (pull-down holds receive). Thermostat EP1 read-only from a simulated `sim_source`; Matter writes rejected; Fan EP2 waits for 2b. The phase-1 probe project stays byte-for-byte unchanged.
  - Acceptance controllers: chip-tool (reference), Home Assistant (Core + separate `python-matter-server`), Apple Home (cross-check); SmartThings on site later (2b/3) ([[concepts/dev-environment]]).
- **Phase 2b:** the real Modbus <-> Matter mapping and anything touching the bus, only after the probe log is in `raw/` and captured.

### Phase 3 – Matter writes
Built with `CONFIG_DMT_BUS_WRITE=y`. Writes via the Matter / Home Assistant path stay here (with phase 2b); console-driven writes in the probe image come earlier (OS6, phase 1 note above, user 2026-11-27). Exit: every whitelisted write works with readback; a local change after our write wins on the next poll; rate limit and latch trigger; log audit shows no write to 200 or 247.

### Roadmap to production (architect, end of `tasks/019-probe-console-writes.md`; "for the user; no specs yet", not decided)
1. Trip with the write image (C/D/E); probe log + capture -> `raw/` -> `/capture`.
2. One combined "2b+3" task set revising 018: one Matter BUS image with the bus stack, the real mapping from the log (2b-B), Fan EP2, Matter writes reusing `write_guard` from 019 plus the D22 debounce/caps and a Matter -> bus queue, and the timing re-tune.
3. 2b-C (DE MCPWM check, Q-HW2) in the same bench pass with a diagnostic build.
4. Deployment hardware (MAX3485 module, power, enclosure; `docs/BOM.md`), then the production image is flashed once and the probe images retire.
5. Architect estimate: about 2–3 implementation passes and one more on-device flash after the trip.

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
| `write_guard` | pure | P3 | Matter intent -> FC06 operations incl. RMW of 201, limits, debounce/caps, readback check, latch (section 3). **Exists since task 019** (019a `2d8ec44`) for the phase-1 write image: console commands -> PRE / WRITE / VERIFY, arming, budget, pacing, latch; driven from `bus_task` via `bus_write.c` (019b `f65f465`); header-only in the read build (spec 019 "019b design" 1, 3). Phase 3 is to reuse it (roadmap, section 1) |

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
Note 2026-11-27 (user, OS6): a console-driven control test in the probe image is planned for the site visit. **Decided 2026-11-27 (W1–W14, section 6 "Console-write decisions"):** the phase-1 write variant has its own rules (console-originated, arming, no automatic FC06 resend, 5 s spacing, 40 frames per arm session, no per-boot cap); the Matter-path rules below stay for phase 3. The never-written list below holds for the write variant too (W5: 008 never written; W9: 247 and 201 bit 4 never written).
- Matter-originated only, and only if the value differs from a fresh poll.
- Debounce 2 s per register; caps 1 write per 5 s per register, 60/h, 500/day total (RAM counters); above a cap the Matter request is rejected (D22). Caps exist because write endurance is not stated and parameters are possibly in EEPROM *(inferred)*.
- FC03 readback after every FC06; ≤ 1 retry, only after a timeout whose readback shows the write did not land.
- 3 consecutive write failures latch writes off until reboot; reads continue.
- **Whitelist (D9):** 231; 201 bits 0–2 and 7 via RMW; 233.
  - 201 RMW: FC03 201 right before, change only bits 0–2/7, preserve bit 4 LOCK, undocumented bits 3/5/6 and reserved bits 8–15 ("System Flags reserved, do not change the status", p5, [[concepts/flag-registers]]), FC06, readback. Residual race with a local keypress (tens of ms) accepted and documented.
- **Never written:** 200 (ADR), 247 (WEB), 202/203, 245/246, 209, 210–234 (MVV), 221/222, 242–244 ([[concepts/register-map]]).
- **247 read-only permanently (D18):** flags persist over power loss and flags 1/6 can lock the user out ([[concepts/web-restriction-flags]], p6). Read only for the setpoint limits (bit 4; bit 5 -> 245/246).
- **Setpoint clamp:** [202, 203] ∩ [245, 246] if 247 bit 5 ∩ [5.0, 40.0] °C; if any limit is unread or stale, the write is refused (p4: "The limit values must be respected!", [[concepts/register-map]]). Whether the panel itself enforces these limits is unknown, so the node enforces them.

### Phase-1 write image: rules added by the 019 reviews (spec 019 amendments A6, B1–B4; architect rulings 2026-11-27, approved with the spec; implemented in 019a `2d8ec44` / 019b `f65f465`, session facts 2026-11-28, not in raw)
W1–W14 (section 6) stay the base rules; checklist B.W and E in `docs/phase1-probe/checklist.md` carry the operator procedure.
- **A6, TX suppression does not depend on hook order:** `write_guard_step` starts **no PRE, WRITE or VERIFY while `poll_sched` health is STOPPED or LATCHED**, armed or not; a pending VERIFY then ends as `verify_lost` (counts as a failure toward the 3-failure latch, W7), a pending PRE/WRITE as `ABORTED`. While armed, STOPPED maps to the disarm reason `poll_off` and LATCHED to `latched`.
- **A6, address capture:** PRE records the slave address; WRITE and VERIFY go to that recorded address. If the current address differs by then (`mb addr`): no TX, `ABORTED` before the WRITE, `verify_lost` at the VERIFY.
- **A6, lost VERIFY split:** disarm reasons `link`, `latched`, `poll_off`, `silence`, `addr` -> no VERIFY, `verify_lost`. Reasons `user`, `timeout`, `budget`, `web_flags`, `health` (DEGRADED/OFFLINE) -> the VERIFY still runs (one FC03 at the normal gap, no retry); no FC06 can go out in these states.
- **A6, hooks:** `command_poll_off`, `command_silence`, `command_addr`, `latch()` and `track_link_faults` call `write_guard_disarm` before the action takes effect, also when already disarmed. Operator rule (checklist E): `mb write disarm` stops writes, not reads; `mb poll off` (and `mb silence`, `mb addr`) stops all TX, a command in flight then counts `verify_lost`.
- **B1, Matter-side write guard:** `firmware/matter` never had a `CONFIG_DMT_BUS_WRITE` FATAL (the symbol does not exist in that project), so the W3 wording "FATAL kept in `firmware/matter`" was void. The guard is the Matter POST_BUILD ELF check `firmware/matter/tools/check_no_bus.py`, whose `FORBIDDEN_PREFIXES` gained `write_guard_` and `bus_write_` (`mb_link_write*` / `mb_rtu_*fc06*` already covered by `mb_link_` / `mb_rtu_`; local inspection 2026-11-28). New acceptance 7(d): `check_no_bus.py --nm` fails on the phase-1 write ELF and lists those symbols, passes on the Matter ELF.
- **B2, checklist / code fixes:** checklist E.0.3 limit check (initial 231 within the live limits), E.3 setpoint step +0.5 or −0.5 °C within the limits, else `E3.skip`; `gap_bytes` > 0 after a `write_TIMEOUT` -> record `E.gap`, stop E, back to the architect; E.9 log-dropped check (the `log_out` ring drops lines when full; `dropped <n>` must equal the E.0.5 value); ABORTED reason fix with host test m6b; `write_guard.h` indentation; `sdkconfig.defaults` comment.
- **B3, left as is:** `command_addr` disarms before the address is validated (unreachable, safe direction); `strtoul` accepts `+3` / `03` for season (still gated to {0, 3, 5}); two-step `set-target` + `build` README form; `log_out` ring drop (docs note only); B.W.7 stack high-water ≥ 1024 B with the write image stays a bench condition and must pass before the trip.
- **B4:** text-only fixes need no re-review (S3); the code nits were checked by `build-verifier` only.

### Compile-time gate and bus-access lint (D10, D20)
- `CONFIG_DMT_BUS_WRITE` default **n**; when off, `mb_link_write` and `write_guard` are not compiled. Phase 1: no write source file exists at all; enabling the gate fails the build until phase 3 (user, 2026-11-09). **Amended 2026-11-27 (W1, W3) for the phase-1 write variant:** the build failure (FATAL) on `CONFIG_DMT_BUS_WRITE=y` is removed in `firmware/main` and **stays** in `firmware/matter`; the default stays **n**, the write variant is built with the overlay `sdkconfig.defaults.write` into `build_write/`. Lint R7/R8 and the ELF check get **two modes** (read-only / write variant); ~~exact rules come with spec 019 (not yet written)~~ **implemented in 019 (local inspection 2026-11-28):** lint R7 `mb_link_write*` only in `mb_link_write.c/.h` and `components/bus_task/src/bus_write.c`; R8 function-code bytes only as the 0x03/0x83 defines in `mb_rtu.c` and the 0x06/0x86 defines in `mb_rtu_write.c`; new R9: FC06 codec names (`mb_rtu_build_fc06`, `mb_rtu_parse_fc06`, `mb_rtu_fc06_*`) only in `mb_rtu_write.c/.h` and `mb_link_write.c`. ELF check `--mode read|write`: read mode forbids `mb_link_write`, `mb_rtu_build_fc06`, `mb_rtu_parse_fc06`, `mb_rtu_fc06_`, `write_guard_` prefixes; write mode requires `mb_link_write`, `mb_rtu_build_fc06`, `write_guard_step`; mode chosen from `CONFIG_DMT_BUS_WRITE`. CMake refuses `CONFIG_DMT_BUS_WRITE=y` in a build dir not named `build_write` and `=n` in `build_write/`. **Corrected 2026-11-28 (B1):** there never was a FATAL in `firmware/matter`; the Matter-side guard is `check_no_bus.py` (previous subsection).
- `tools/check_bus_access.py` runs `--self-test` and the tree scan on every `idf.py build`; comments and strings are not exempt; `test/` and `test_host/` are skipped. Base rule set R1–R8 from `tasks/001-skeleton-kconfig-lint.md`, rewritten for E2 by task 013 (L1–L9, A1–A12). Rules in force (session facts; exact text in `firmware/tools/check_bus_access.py` and spec 013):
  - No esp-modbus anywhere (no `mbc_*` / `mbm_*` / `mbs_*` names, no esp-modbus headers incl. comment, digraph and `#import` forms; manifests match `esp[-_]modbus`) (R1–R3).
  - R4: `uart_write_bytes` only in `components/rs485_port/src/`; banned everywhere incl. `rs485_port`: `uart_tx_chars`, `uart_set_rts`, `uart_set_dtr`, `uart_set_loop_back`, `uart_set_line_inverse`, `_uart_set_pin*`, `uart_ll_*`, `uart_hal_*`, `esp_rom_uart_tx_one_char`, `uart_tx_one_char`, `esp_rom_output_*`, `esp_rom_uart_*`, `esp_rom_install_*` (no blanket `esp_rom_*` ban), raw `UARTn.` / `UARTn->` access, `REG_WRITE` / `WRITE_PERI_REG` / `REG_READ` / `READ_PERI_REG` on `UART_FIFO*_REG` (200-character window), and the `uart_struct.h` / `uart_ll.h` / `uart_hal.h` / `uart_reg.h` includes.
  - R5: UART setup calls only in `components/rs485_port/` (`uart_set_pin`, `uart_set_mode` with 2nd argument literally `UART_MODE_RS485_HALF_DUPLEX` or `UART_MODE_UART`, `uart_driver_install`, `uart_driver_delete`, `uart_param_config`, `uart_read_bytes`, plus `uart_set_hw_flow_ctrl`, `uart_set_baudrate`, `uart_set_parity`, `uart_set_word_length`, `uart_set_stop_bits`, `uart_set_tx_idle_num`, `uart_enable_tx_intr`, `uart_flush`, `uart_intr_config`, `uart_isr_register`); `uart_flush_input` allowed.
  - R6: `CONFIG_DMT_BUS_TX_GPIO` / `CONFIG_DMT_BUS_DE_GPIO` only in `components/rs485_port/` and `main/`. R7: `mb_link_write*` only in `mb_link/src/mb_link_write.c` / `mb_link/include/mb_link_write.h` (neither exists in phase 1) (*superseded 2026-11-28 by task 019: both files exist, compiled only in the write build; R7/R8/R9 as in the gate bullet above*).
  - R8: function-code bytes only in `mb_rtu`, only 0x03 in phase 1 (since 019 also 0x06/0x86 in `mb_rtu_write.c`, gate bullet above); `++` / `--` on function-code fields banned; deterministic comment regex. In `mb_link/` and `rs485_port/` names `fc`, `func`, `command` and strings such as `"fc=%d"` fail R8; FC-valued hex constants must be decimal or named (L9).
  - Documented limits (L8, A9–A11, A14): parenthesis nesting; FC arithmetic not evaluated; escapes not decoded; CMake/Kconfig not scanned; the single `uart_write_bytes` site checked by inspection; nested-bracket prefix `++a[b[1]].fc`; quadratic R5 helper.
- Post-build ELF check `tools/check_elf_symbols.py` (POST_BUILD, `xtensa-esp32s3-elf-nm`): forbids all `mbc_` / `mbm_` / `mbs_` symbols and `mb_link_write`; **fails** on empty `nm` output or missing `app_main` (L5); required symbols `mb_link_read`, `mb_rtu_build_fc03` since task 006. **Correction (session fact, verified by the orchestrator in `firmware/tools/check_elf_symbols.py:22`, local inspection, 2026-11-24):** the pattern is only `^(mbc_|mbm_|mbs_|mb_link_write)`; "no Wi-Fi / lwip / Matter linked" in the probe image was a manual `build-verifier` acceptance item, **not enforced by tooling**.
- Phase-2a Matter image: a **separate new** check script in `firmware/matter/` forbids `mb_link_` / `rs485_port_` / `mb_rtu_` / `bus_task_` / `mbc_` symbols; no phase-1 file touched (user decision, session 2026-11-24). **Extended 2026-11-28 (019b, B1):** also `mbm_`, `mbs_`, `write_guard_`, `bus_write_` (local inspection of `check_no_bus.py:24-25`); self-test 46 cases (was 38) (session fact, `build-verifier` 2026-11-28).
- The ELF bullet above (pattern `^(mbc_|mbm_|mbs_|mb_link_write)`) describes the pre-019 read-only script; since 019 it has the two modes described in the gate bullet.
- Superseded task-001 R1–R8 text and v0.3 lint wording: [[concepts/firmware-history]] §4.

### NVS wear
- Superseded (was: "Mirrored Matter attributes without the NONVOLATILE flag; the esp_matter defaults for thermostat attributes are to be checked"). Wrong since 2026-11-25.
- **Current:** esp_matter 1.6.1 makes OccupiedHeatingSetpoint, OccupiedCoolingSetpoint and SystemMode NONVOLATILE *(external, not in raw: esp_matter 1.6.1 source `firmware/matter/managed_components/espressif__esp_matter/components/esp_matter/data_model/esp_matter_data_model.cpp`, read by the `architect` 2026-11-25)*. The task-015 finding (session 2026-11-25, *external*) also listed ControlSequenceOfOperation as NONVOLATILE and stated that the public API cannot clear the flag; the 2026-11-25 architect note names only the three attributes above (see open question in section 8). **Resolved 2026-11-25:** ControlSequenceOfOperation **is** NONVOLATILE in esp_matter 1.6.1 *(external, not in raw: legacy `esp_matter_attribute.cpp:2131-2132` and generated `clusters/thermostat/thermostat.cpp:368`, read by the orchestrator 2026-11-25)*; no wear, because the firmware never updates it (stays 4) and external writes are rejected; no spec change. The node persists nothing else.
- Handling (decision 2a-10, spec 016): the bridge pushes only changed values; esp_matter itself also skips writes of unchanged values *(external, same source)*. Deferred persistence is enabled on the three attributes, with the global Kconfig `CONFIG_ESP_MATTER_DEFERRED_ATTR_PERSISTENCE_TIME_MS=60000`: it limits the rate to at most one NVS write per attribute per 60 s; it does not wait for a quiet period *(external, same source)*. 60 s approved by the user 2026-11-25.
- Boot: the stored value replaces the initial value at attribute creation and is visible until the first bridge cycle, which runs at once with `first=true` (under 1 s, *inferred*).
- Wear estimate (*estimate*; NVS figures *inferred* from ESP-IDF docs, external): worst case 3 attributes x 1,440 writes/day = 4,320 writes/day -> about 38 years; with a 3 s deferral it would be about 1.9 years. Unverified assumption: half of the 48 KB NVS partition is free for churn; can be checked on the device with `nvs_get_stats` after commissioning.

## 4. Hardware wiring

- Use one A and one B of "A A B B -". Doubled terminals = bridged in/out *(inferred, [[entities/ekwhctrl1]])*; continuity check with mains off (probe item 11).
- Site power (user, 2026-11-09): normal USB power; the architect's proposal (laptop on battery / USB isolator until "-" is measured) was declined. Whether the panel's low-voltage side is isolated from mains is not stated in the wiki (section 8).
- Node GND to "-" only after measuring "-" and A/B against node GND and finding them within −7..+12 V common mode *(external, RS485 standard)*. "-" is possibly shared with the CP presence contact *(inferred, [[entities/ekwhctrl1]], p3)*.
- The CP terminal and "+ A B -" are never touched.
- ESP TX -> DI: 3.3 V is fine, V_IH ≥ 2.0 V ([[sources/max485-datasheet]] p2).
- RO -> level shift -> ESP RX, because RO is a 5 V output (D13, [[entities/max485]]), unless U1 turns out to be a 3.3 V part. **Divider rejected** (architect assessment, session 2026-11-27, *inferred*): a 10k/20k RO -> RX divider leaves RX at about 1 V (undefined) while DE + /RE are high and RO is high-Z -> BADFRAME.
- **Bench wiring (D23; user decisions, session 2026-11-27, not in raw):** module VCC from the ESP board **3.3 V pin** (as run on 2026-11-27; the original D23 decision was the board 5 V pin, changed because the 5 V pin "seems not working" (user); unverified whether it is only fed from the other USB connector; 3.3 V is outside the MAX485 5 V ±5 % rating ([[sources/max485-datasheet]]) if U1 is a 5 V part, *inferred*, U1 illegible); GND; GPIO5 (TX) -> DI; GPIO6 (DE/RTS) -> DE + /RE jumpered, 10 kΩ to GND (D8 kept); RO -> BZX55C5V1 Zener used **forward as an ordinary diode** (band/cathode to RO, anode to GPIO4) -> GPIO4 (RX) with 2 × 10 kΩ in series (20 kΩ) pull-up to 3.3 V. Accepted **only if the static check passes**; check as revised by the architect 2026-11-27: RO disconnected, diode cathode to GND, GPIO4 < 0.8 V -> **PASS** 2026-11-27 (user; exact value not reported). With VCC 3.3 V, RO is at most 3.3 V, so the diode is harmless but not strictly needed *(inferred)*. Bench power: PC USB on the CH343 UART port only. A/B open, no bus. (BZX55C5V1 identification from the glass-body marking "55C5V1"; part family *external*.)
- **Module replacement (D23; user decision, session 2026-11-27):** the current MAX485-M has 10 kΩ pull-ups to VCC on RO, /RE, DE and DI (B.2 measurement, [[entities/max485]]). Tied DE + /RE = 5 kΩ to 5 V against the 10 kΩ pull-down -> DE ≈ 3.3 V while GPIO6 floats (reset/boot) -> transmitter enabled, violates D8 on a live bus (architect, *inferred*). It is used on the bench only; on site and in deployment a **MAX3485 (3.3 V) module** without DE / /RE pull-ups replaces it (buying criteria, arrival tests and its wiring: section 8). With a 3.3 V module the RO level shift is not needed *(inferred)*. With the bench VCC at 3.3 V the tied DE + /RE pull-ups give DE ≈ 2.2 V while GPIO6 floats, still above V_IH 2.0 V, so the D8 issue persists (bench only, *inferred*). **Amended 2026-11-27 (user, later in the session):** the on-site phase-1 run uses this MAX485-M after all; the MAX3485 module stays the **deployment** module (D23 row, next bullet).
- **On-site phase-1 wiring (user decisions, session 2026-11-27, not in raw; overrides the D23 "bench only" rule):** current MAX485-M, VCC from the ESP board **3V3 pin** (outside the 5 V rating if U1 is a 5 V part, *inferred*), R7 120 Ω fitted, weak module bias (idle A–B ≈ 10 mV, bench), DE + /RE to GPIO6 with a **single 10 kΩ pull-down** (D8 unchanged; boot-window risk accepted by the user, section 8), **RO direct to GPIO4** (diode removed) with the 2 × 10 kΩ (20 kΩ) pull-up to 3.3 V kept, GPIO5 -> DI. Condition: **never feed the module 5 V in this wiring** (RO would drive GPIO4 at 5 V level, *inferred*). One bench re-check with the direct RO before leaving (checklist B.16 = repeat of B.4.5 / B.7). Image: app `6654669-dirty` (phase-1 code = `b8ad00d`).
- **DE and /RE tied together to the RTS GPIO, 10 kΩ pull-down** (D8, Q1). Header RO/RE/DE/DI with separate DE and RE pins ([[sources/max485-module-photo]]); tying DE and /RE is the datasheet's send/receive switching (inferred from [[sources/max485-datasheet]] p10).
- **DE signal (task-002 spike S5, *project evidence*, source reading, no hardware):** in `UART_MODE_RS485_HALF_DUPLEX` on esp32s3 / IDF v6.0.2 **RTS** drives DE: **low while idle, high during TX** (`IDF/esp_hal_uart/esp32s3/include/hal/uart_ll.h:773-783`; `IDF/esp_driver_uart/src/uart.c:1191` raises it before the TX FIFO is filled, `:1506` drops it in the TX_DONE ISR) ([`docs/spikes/002-esp-modbus.md`](../../../docs/spikes/002-esp-modbus.md) S5).
- **Pin order (DE safety, user-approved 2026-11-24):** `uart_set_mode(UART_MODE_RS485_HALF_DUPLEX)` **first**, then `uart_set_pin` (TX, RX, DE as RTS). Reason: `sw_rts` resets to 0 (`IDF/soc/esp32s3/register/soc/uart_struct.h:517`) and the RTS pin follows its inverse, so routing RTS first drives DE **high** in between *(project evidence, S5)*. The spec 002 interface comment had the reverse order.
- **DE re-drive after driver delete (DE safety, user-approved 2026-11-24):** `uart_driver_delete` -> `uart_release_pin` disables the RTS output (`uart.c:2116`); DE then floats on the 10 kΩ pull-down. `rs485_port` writes the DE GPIO level **0 before** `gpio_output_enable` (`IDF/esp_driver_gpio/src/gpio.c:228`) *(project evidence, S5)*. Both DE rules are in the code-inspection checklists.
- **DE timing facts (task 014 review, `firmware-reviewer`, project evidence from the local IDF v6.0.2 source, not verified on hardware):** DE is high only during our own 8-byte request; RTS asserted before the TX FIFO fill (`uart.c:1191`), released in the TX_DONE ISR (`uart.c:1506`); half-duplex mode sets `sw_rts` = 1, i.e. RTS low (`uart_ll.h:773-783`); mode before pins; DE driven low before output enable (`gpio.c:228-229`); no reply byte is lost to the ISR RX-FIFO reset (`uart.c:1505`), because the receiver is off while DE is high. DE release depends on the ISR (section 3).
- **GPIO:** RX/TX/DE via Kconfig, placeholder defaults TX 5 / RX 4 / DE 6 (user, 2026-11-09). Avoid 0/3/45/46 (strapping), 19/20 (USB), 26–37 (flash 26–32, octal PSRAM 33–37 on R8), 43/44 (UART0) *(external; exact reserved set unconfirmed, no module datasheet in raw, see [[entities/esp32-s3-wroom-1]])*.
- **Termination (open):** measure A–B at the panel port (mains off, node disconnected). ≈ 120 Ω -> fit the user's separate 120 Ω at the node end; high/open -> start without and judge by the phase-1 error rate (a short 2-node run at 9600 has only minor reflections, *inferred*). Bias / fail-safe idle is assessed together with it. The current MAX485-M has a fitted **120 Ω termination (R7)** and a bias network (A–VCC 20 kΩ, B–GND 10 kΩ) (B.2 measurement 2026-11-27, [[entities/max485]]); since this module is replaced for on site (D23), the termination and bias of the **replacement** module decide whether the separate 120 Ω is fitted or the on-board one removed *(inferred)*. **On-site phase-1 rule with the MAX485-M (user decision 2026-11-27, checklist C.3/D.1):** R7 is fitted, so the separate 120 Ω is **never** fitted; panel A–B ≈ 120 Ω -> two terminations (≈ 60 Ω total), fine; high/open -> fine; kΩ range -> possibly panel bias, record; < 100 Ω or unstable -> stop, back to the architect. The MAX485 datasheet terminates both bus ends in the characteristic impedance ([[sources/max485-datasheet]] p13).

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

### Core decisions D1–D23 (user, 2026-11-08; "kept" = 2026-11-07, re-confirmed; D23 user, 2026-11-27)

| # | Status | Decision |
|---|---|---|
| D1 | changed | Tap "A A B B -", node is the sole master; "+ A B -" never touched (hard rule) |
| D2 | changed | Phases 1 probe / 2 read-only mirror / 3 writes; phase-1 specs before the on-site items, phase-2/3 specs only after the probe log (Q8). **Amended 2026-11-24:** phase 2a (bus-independent) may start now (section 6 "Working mode and gate"). **Amended 2026-11-27 (OS6):** console-driven writes in the probe image brought forward for the site visit. **Amended 2026-11-27 (W2):** gate exception, spec + build 019 (phase-1 write variant) before the probe log / bus-topology answers; on-site writes gated at run time and by checklist E.0 |
| D3 | kept | Thermostat EP1 + Fan EP2, no fan Off; Thermostat first |
| D4 | changed | v0.2 mapping + degradation for starred registers; limits from 202/203/245/246 (section 5) |
| D5 | kept | Bus is truth, last writer wins, never re-assert |
| D6 | changed | Periodic FC03 polling in two groups, 5 s / 60 s; qty > 1 only if the probe proves it |
| D7 | **superseded 2026-11-24** | (was: esp-modbus master is the only Modbus layer; sniffer/monitor dropped). Now E2: own FC03-only RTU master, no esp-modbus; sniffer/monitor stay dropped |
| D7a | retired | (was: private esp-modbus `mb_crc16` for the sniffer) |
| D7b | retired | (was: time-shared UART); `rs485_port` owns the UART (E2) |
| D8 | changed | RS485 half-duplex, RTS -> DE + /RE tied, 10 kΩ pull-down (Q1). **Note 2026-11-27 (user): not changed** for the on-site MAX485-M run; single 10 kΩ kept, the module's DE / /RE pull-ups defeat it while GPIO6 floats (boot-window risk **accepted by the user**, section 8; 10 × 10 kΩ parallel ≈ 1 kΩ pull-down rejected) |
| D9 | kept | Write whitelist 231, 201 bits 0–2/7 via RMW, 233; never 200 |
| D10 | changed | Single compile-time gate `CONFIG_DMT_BUS_WRITE`; debounce, caps, readback, ≤ 1 retry, latch. **Amended 2026-11-27 (W3, W7, W8):** same gate reused for the phase-1 write variant (overlay `sdkconfig.defaults.write`, `build_write/`); FATAL removed in `firmware/main`, kept in `firmware/matter` (**corrected 2026-11-28, B1:** no such FATAL ever existed in `firmware/matter`; the Matter guard is `check_no_bus.py`, section 3); the write variant has no automatic FC06 resend and its own pacing (W7, W8); the Matter-path caps / retry stay for phase 3 |
| D11 | kept | Commissioning BLE + Wi-Fi, dev DAC, 0xFFF1/0x8000, BOOT long-press factory reset |
| D12 | changed | Host tests against a fake `mb_link` slave; probe logs become fixtures |
| D13 | kept+ | RO level shift, DE pull-down, all bus GPIO via Kconfig. **Note 2026-11-27 (OS4):** with module VCC 3.3 V, RO goes direct to GPIO4 (no shift); a 5 V-fed module never direct |
| D14 | kept | chip-tool allowed for tests |
| D15 | changed | 233 = 0 with STAT unavailable -> last explicit season (default Heat) + console flag; if STAT answers, v0.2 D15 = b (Q2) |
| D16 | new | Timing defaults, section 3 (Q3) |
| D17 | new | Boot silence check: RX-only 5 s at boot (60 s on demand) before the master transmits; any byte -> refuse TX, latched (Q4) |
| D18 | new | 247 read-only permanently (Q5) |
| D19 | new | Slave address from Kconfig (default 1) + RAM-only console override in P1, no scan (Q6). See conflict below |
| D20 | new | Bus-access lint + ELF check (section 3) |
| D21 | new | Health ONLINE / DEGRADED / OFFLINE with backoff; never broadcast; addresses 1..247 only |
| D22 | new | Write caps / debounce values, section 3 (Q7) |
| D23 | new (2026-11-27) | Current MAX485-M (10 kΩ pull-ups on RO, /RE, DE, DI; 120 Ω termination fitted) **for the bench run only** (A/B open, no bus); on site and deployment use a **MAX3485 (3.3 V) module** without DE / /RE pull-ups (to buy; on-site run waits for it). Bench: VCC 5 V from the board (**amended 2026-11-27 (user): run with VCC from the board 3.3 V pin**, section 4), GPIO 5/4/6 and the 10 kΩ DE pull-down kept, RX via BZX55C5V1 forward + 20 kΩ pull-up to 3.3 V, accepted only if the static RX check passes; RO divider rejected (section 4). **Amended 2026-11-27 (user, later decision): the on-site phase-1 run uses the current MAX485-M** (overrides "bench only" and "on-site run waits for it"); the MAX3485 module stays the **deployment** module; on-site wiring: section 4, section 6 "On-site phase-1 decisions" |

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
| Gate | **Phase 2a may start now:** bus-independent part only (Matter skeleton: esp_matter, Wi-Fi commissioning, Thermostat endpoint fed by a fake/simulated register model; no new bus code, no writes). The real Modbus <-> Matter mapping and anything touching the bus (phase 2b) still wait for the probe log in `raw/` + `/capture`. Supersedes the earlier rule "stage 2/3 specs only after the probe log is in raw/ and captured" (D2, Q8) **for the bus-independent part only**. Phase-2a plan **approved** 2026-11-24 (next subsection). **Amended 2026-11-27 (2b-P1):** task 2b-A (bus stack + shared console) may be specified and built before the probe log, bench only with A/B open; 2b-B / 2b-C keep the probe-log gate (section 6 "Phase-2b plan") |

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
| 2a-10 | (2026-11-25, "Accept + limit") Keep the esp_matter NONVOLATILE default for the setpoints and SystemMode (section 3 "NVS wear"); task 016 updates attributes **only on change** and uses `attribute::set_deferred_persistence` on those 3 attributes with `CONFIG_ESP_MATTER_DEFERRED_ATTR_PERSISTENCE_TIME_MS=60000` (rate limit, max one write per attribute per 60 s); flash-wear estimate in spec 016 and section 3. **60 s approved by the user 2026-11-25.** Consequence *(inferred)*: one NVS write per value change (deferred); at boot the stored value overrides the initial value until the first bridge cycle (`first=true`, under 1 s, *inferred*) |

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
| Write code | ~~No write file in phase 1; enabling `CONFIG_DMT_BUS_WRITE` fails the build until phase 3 (D10)~~ **Superseded 2026-11-27 (W1, W3):** phase-1 write variant with `CONFIG_DMT_BUS_WRITE=y` via overlay `sdkconfig.defaults.write` into `build_write/`; the read-only build (default n) is unchanged; the FATAL stays in `firmware/matter` (*void, B1 2026-11-27: there was none; Matter guard = `check_no_bus.py`, section 3*) |
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

### Phase-1 bench decisions (user, session 2026-11-27, not in raw)

| # | Decision |
|---|---|
| R2 | Keep R7 (120 Ω) on the bench MAX485-M and finish section B; B.4.5 and B.7 recorded "passed on this run, not conclusive with R7 fitted"; repeat **B.1–B.7 incl. B.4.5** on the MAX3485 module before the on-site run (options R1 remove R7, R3 wait for the module not taken). **Superseded 2026-11-27 for the on-site run** by OS1/OS4 (next table): one bench re-check B.16 with RO direct on the MAX485-M instead |
| Console race | Fix the console-init race (section 7 "Phase-1 bench run") **with phase 2b**; the architect writes a small spec then; workaround `mb log on` until then. **Refined 2026-11-27:** fixed in task 2b-A (2b-P3) and backported to phase-1 `main.c` (2b-P4). **Amended 2026-11-27 (W11):** 018 Part B dropped; the phase-1 fix moved into task 019 and is **in the 019 images** (019b `f65f465`, 2b-P4 row); to be confirmed on the bench (B.W.1); the Matter-side REPL change (2b-P3) stays with the revised 2b plan |
| Bench VCC | Module VCC from the board 3.3 V pin instead of 5 V (D23 amendment, section 4) |

### On-site phase-1 decisions (user, session 2026-11-27, not in raw; architect assessment 2026-11-27 as basis)

| # | Decision |
|---|---|
| OS1 | On-site phase-1 run **with the current MAX485-M** setup (overrides D23 "bench only"); MAX3485 stays the deployment module (D23 amendment, section 4 "On-site phase-1 wiring") |
| OS2 | D8 **not** changed: single 10 kΩ DE pull-down kept; residual boot-window risk **accepted by the user** (section 8); rejected mitigation: 10 × 10 kΩ in parallel (≈ 1 kΩ) |
| OS3 | **Latch rule on site relaxed** (procedure only; the firmware latch D17 is unchanged): boot silence `activity` 1–7 -> power-cycle via USB, at most 2 retries (3 boots total), log each; `activity` ≥ 8 or the 3rd latch -> abort and report. D.10 `mb silence 60` still covers slow masters. Supersedes checklist D.2 "do not reboot to retry" |
| OS4 | RX on site: **RO direct to GPIO4** (diode removed), 2 × 10 kΩ (20 kΩ) pull-up to 3.3 V kept; condition: never feed the module 5 V in this wiring; one bench re-check of B.4.5 / B.7 with the direct RO before leaving (checklist B.16) |
| OS5 | **Hard rule:** never run an image without the bus code's early DE hold (currently the phase-2a Matter image, which never drives GPIO6) while A/B are wired to the bus; with the MAX485-M DE / /RE pull-ups its transmitter would stay enabled the whole time (mechanism *inferred*, section 8). Only the phase-1 probe image or the future 2b-A bus image (`mb_link_hold_receive` first in `app_main`) may run with A/B wired. After the site run: **disconnect A/B first, then re-flash the Matter image** (checklist A.6, D.11) |
| B.16 latch | Bench B.16 (orchestrator default per S5, 2026-11-27): a latch (`activity` > 0) -> reboot and repeat, at most 2 times (as OS3), log each boot; `BADFRAME` or `gap_bytes` > 0 -> STOP, back to the architect. Log rows `B16.*` and `D2a.ab_idle` added to `measurement-log-template.md` |
| OS6 | **Control test on site (user, 2026-11-27):** on the upcoming site visit (same trip as the read probe, current MAX485-M setup per OS1–OS5), **after** the read probe, test **control** of the fancoil via **console commands in the probe image**: setpoint, on/standby, fan speed, season/mode. Brings phase-3 writes forward for the probe image; amends the staged phasing "write code compiled out until phase 3" (section 1, D2, D10, stage-1 "Write code" row). The Matter / Home Assistant control path stays later (2b + phase 3). ~~The `architect` drafts the design (write scope, safety, console commands, checklist section E) for user approval; no spec yet.~~ **Amended 2026-11-27:** design approved by the user as W1–W14 (next table); "in the probe image" now means the separately built phase-1 write variant (W1, W4). Spec 018 (2b-A) **paused**; 018 Part B dropped, the P4 console-race backport moves into task 019 (W11) (section 7). **Status 2026-11-28:** implemented as task 019, code complete (019a `2d8ec44`, 019b `f65f465`), bench B.W pending (section 7) |

### Console-write decisions W1–W14 (user, session 2026-11-27, not in raw; on the architect's console-write design, 2026-11-27)

Register meanings: [[concepts/register-map]], [[concepts/flag-registers]], [[concepts/web-restriction-flags]] (from [[sources/n420384a-english]] p4–p6). Checklist items (C/D/E, B.W, B.16) refer to `docs/phase1-probe/checklist.md` (project document; ~~section E and B.W not yet written there~~ written by 019b `f65f465`, local inspection 2026-11-28).

| # | Status | Decision |
|---|---|---|
| W1 | approved | Writes **console-only**, in a **separately built phase-1 write variant**; the Matter image stays write-free until phase 3 |
| W2 | approved | Gate exception: spec + build of task 019 **before** the probe log / [[concepts/bus-topology]] answers; on-site writes gated **at run time** (register read fresh-OK in this session, health ONLINE) and by checklist **E.0** (W14) |
| W3 | approved | Reuse `CONFIG_DMT_BUS_WRITE`; overlay `sdkconfig.defaults.write`, build dir `build_write/` (section 3 gate amendment). **Wording replaced 2026-11-27 (spec 019 B1):** "The Matter project stays write-free until phase 3 (W1). Its POST_BUILD `check_no_bus.py` rejects any write symbol (W3)"; the earlier "FATAL kept in `firmware/matter`" was void (section 3) |
| W4 | approved | **One image for the trip:** the write image is flashed on the bench before the trip and used **disarmed** for C/D and armed for E; **no flashing on site**. User principle: as few images / flashes as possible, reach production firmware in as few steps as possible |
| W5 | approved | **Write set:** 231 setpoint (raw 0.1 °C; 5.0–40.0 °C, clamped by 202/203, by 245/246 if 247 bit 5, and by 247 bit 4); 201 bit 7 standby; 201 bits 0–2 fan (000 auto, 001 silent, 010 night, 011 max) via read-modify-write preserving all other bits; 233 season 0/3/5. **008 never written; nothing else written** |
| W6 | approved | Arming: default 120 s, range 30–600 s; restore set captured at the **first arm per boot and address** |
| W7 | approved | **No automatic FC06 resend**; any failure disarms; writes **latched after 3 failures per boot** until reboot (user kept reboot-to-unlock) |
| W8 | approved | ≥ 5 s between write commands; ≤ 40 FC06 frames per arm session (fresh budget per new arm); **no per-boot cap** |
| W9 | approved | Arming refused if 247 flags 1, 2, 3, 6, 7 or 8 are set; flags 4 / 5 feed the setpoint clamp; **247 and 201 bit 4 never written** |
| W10 | approved | Log: T kinds `PRE` / `WRITE` / `VERIFY`, `W` summary lines, `A` arm lines (exact format in spec 019) |
| W11 | approved | **One task 019**, including the 2b-P4 console-race backport to phase-1 `main.c`; **018 Part B dropped**. **Done in 019b `f65f465`** (2b-P4 row) |
| W12 | approved | Season test included (user knows the effect), run **last**, at most 60 s, then restore |
| W13 | approved | Restore order 201 -> 233 -> 231 |
| W14 | approved | **E.0 prerequisite:** D.3 OK for 201 / 231 / 233 / 202 / 203 / 247 / 105 (+ 245 / 246 if 247 bit 5); D.7 done; D.8 ≥ 10 min at < 1 % error rate and no BADFRAME in the last 10 min |

Consistency check (wiki agent, against existing wiki pages, not re-read from the PDF): W5 fan codes match 201 bits 0–2 in [[concepts/flag-registers]] (p5); 233 values 0 / 3 (winter) / 5 (summer) and 231 range 5–40 °C at 0.1 °C match [[concepts/register-map]] (p4); W9 flag numbers are the 247 bit numbers in [[concepts/web-restriction-flags]] (1 forced off, 2 program rotation, 3 standby select, 6 all keys, 8 season key; 4 inhibit extremes, 5 setpoint restriction; 7 see that page). The local key order (probe item 8) is still to be correlated on site. Supersedes for the write variant: stage-1 "Write code" row, D2 / D10 notes, OS6 "design pending" (all amended above).

### Phase-2b plan (user decisions, session 2026-11-27, not in raw; approved with all recommended defaults; architect assessment 2026-11-27)

| # | Decision |
|---|---|
| 2b-P1 | Gate amendment: spec and build **2b-A before the probe log**; bench only with A/B open, **no live bus before the probe log is captured** |
| 2b-P2 | Keep `sim_source` as a Kconfig choice |
| 2b-P3 | One shared `dmt>` REPL in a neutral module, created **before** `bus_task` / `log_out` (fixes the console-init race, section 7) |
| 2b-P4 | Backport the race fix to phase-1 `main.c`. **Done in 019b `f65f465` (W11; 018 Part B dropped):** new `main.c` order (spec 019 "019b design" 5): DE hold first, console-UART check, `uart` log WARN, REPL created + help + `mb` (and `mb write`) commands registered, then `bus_task_start`, then `esp_console_start_repl`; any failure before `bus_task_start` logs and returns, so `bus_task` is not started. On-device confirmation pending (bench B.W.1 header) |
| 2b-P5 | Polling starts after a clean silence check **regardless of commissioning** |
| 2b-P6 | LATCHED / OFFLINE -> stale attributes null per section 5; no extra fault cluster |
| 2b-P7 | Fan EP2 read-only in 2b (task 2b-B) |
| 2b-P8 | DE re-check method (F4, Q-HW2): on-chip **MCPWM capture** edge timestamps of the DE and TX pads in a diagnostic build with NVS churn (IDF `esp_driver_mcpwm`; method *inferred*, driver *external*) |
| 2b-P9 | MAX485-M allowed for 2b bring-up; MAX3485 for deployment |
| 2b-P10 | Timing re-tune per the section-3 rule from the probe log, as Kconfig defaults |
| Tasks | **2b-A** bus stack + shared console + race fix (does not need the log; bus-touching -> full review, S3); **2b-B** real mapping + Fan EP2 read-only (needs the log); **2b-C** bus re-tune + Q-HW2 DE check (needs the log; bus-touching) |
| Gate | 2b-B / 2b-C specs only after the probe log is in `raw/` and captured **and** [[concepts/bus-topology]] items 1, 1b, 3, 4, 5 are answered. Amends the "Working mode and gate" row for 2b-A only |

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
| 006 | `bus_task`, probe console, `app_main` (first flashable image) | done 2026-11-24; ~~not run on hardware~~ run on the bench 2026-11-27 (checklist B, below) | `b8ad00d` |
| 011 | fake-slave + e2e host tests (host 139 / 0 / 0) | done 2026-11-24 | `17825cb` |
| 008 | phase-1 probe checklist + log template | done 2026-11-24 | `4be04e3` (specs + wiki `13b0591`) |
| 003/004/005/007 | — | retired (007 replaced by 011) | — |
| 015 (2a-A) | Matter skeleton (`firmware/matter/`, esp_matter, BLE commissioning) | implemented + build-verified 2026-11-25; reviewed with 016 at feature end; on device 2026-11-25: D1–D5 pass, D7 HA part pass; D6, D7 Apple Home pending | `9dbace6` (specs `bb2b004`) |
| 016 (2a-B) | simulated model + provisional read-only EP1 mapping + sim console | implemented, build-verified, feature-end review of 015+016 (0 blocker, 2 major, 8 minor), fixed, delta-reviewed (no blocker/major), re-verified 2026-11-25; on device 2026-11-25: O1–O6, O8 pass, O9 pass (idle check 64 s only); O7 pending | `03dacf5` (wiki/RESUME `2f932e7`) |
| 018 (2b-A) | bus stack in the Matter image + shared `dmt>` console + console-race fix (2b-P1..P5) | spec `tasks/018-bus-in-matter-readonly.md` written by the `architect` 2026-11-27 (architect assumptions A1–A8 in the architect's report, not in the spec); **paused 2026-11-27 (user, OS6)**, to be revised after the trip into a combined 2b+3 plan (fewest steps to production, W4 principle; roadmap, section 1); **Part B dropped 2026-11-27 (W11)**, its P4 console-race backport done in 019b; not approved; bench only, A/B open; full review. ~~`tasks/README.md` row for 018 still missing; spec file uncommitted~~ (spec committed `a3ea455`; `tasks/README.md` has the 018 row, local inspection 2026-11-28) | spec `a3ea455` |
| 019 | phase-1 **write variant** (console-only writes W1–W14, `CONFIG_DMT_BUS_WRITE` overlay `sdkconfig.defaults.write`, `build_write/`, lint R7/R8/R9 + ELF two modes) + 2b-P4 console-race backport to phase-1 `main.c` (W11) | spec approved by the user with A1–A10 (2026-11-27), committed `a3ea455` (decisions `c6c2a32`); 019a review amendments A1–A10 committed `0075304`. **Code complete 2026-11-28** (019a / 019b rows, "Task 019 verified facts" below). **Acceptance pending:** bench B.W.1–B.W.8 with the write image (not yet flashed); must pass before the trip, else the trip stays read-only (section 8) | spec `a3ea455`, `0075304` |
| 019a | pure part: FC06 codec `mb_rtu_write`, `write_guard`, poll_sched kinds PRE/WRITE/VERIFY, txn_log, fake slave FC06, lint/ELF mode logic | done: committed `2d8ec44` incl. the **A6 fix-up** (no write-path TX while the scheduler is STOPPED/LATCHED; WRITE/VERIFY to the address recorded at PRE; section 3); `build-verifier` re-run (host 200/0/0 at that commit) | `2d8ec44` |
| 019b | target part: `mb_link_write`, `bus_write.c`, `mb write` console (arm/disarm/sp/standby/fan/season/restore), write build variant, P4 race backport in `main.c`, checklist B.W/E + log template, `firmware/README.md` | done: committed `f65f465` after review fixes **B1–B4** (Matter guard = `check_no_bus.py` with `write_guard_` / `bus_write_` prefixes, new 7(d), replacing the never-existing Matter write FATAL; checklist E.0.3 limit check, E.3 ±0.5 or `E3.skip`, `gap_bytes` after `write_TIMEOUT`, E.9 log-dropped check; ABORTED reason fix with test m6b); `firmware-reviewer` GO (no blocker/major), `build-verifier` a–g all pass; RESUME `26e9704` | `f65f465` |
| 2b-B | real Modbus <-> Matter mapping + Fan EP2 read-only (2b-P6, P7) | waits for the probe log + bus-topology items (gate) | — |
| 2b-C | bus timing re-tune + Q-HW2 DE check (2b-P8, P10) | waits for the probe log (gate); full review | — |
| 017 (2a fix) | force station-only Wi-Fi before `esp_matter::start` (stops the unintended softAP) | done 2026-11-25: implemented, build-verified (image 0x1b34d0, 0 own warnings, `check_no_bus` pass, host 62/0/0), reviewed (0 blocker/major; 2 optional log-text minors left: no explicit log when STA netif creation fails, `app_main.cpp` ~63; non-AP/APSTA modes labelled "STA+AP" in the warning, ~77); on device: STA only (below); 015 D5 re-check pending | `6b10c93` (spec + wiki `225bf37`) |

**Phase 1 complete. Phase 2a code complete** (015 and 016 committed and pushed); **first on-device run 2026-11-25** (below); softAP finding fixed by task 017 (below). **Phase-1 bench run (checklist section B) done 2026-11-27** with the R7 caveat (below). **Task 019 code complete 2026-11-28** (below). **Next (2026-11-28):** bench B.W.1–B.W.8 with the write image (build + flash per `firmware/README.md`, A/B open; B.W.2 repeats B.16) -> `wiki-keeper` records the 019 acceptance -> trip: C, D.1–D.10 disarmed, E control test, D.11 (one image, no flashing on site, W4; OS1–OS6) -> probe log + capture to `raw/` + `/capture` -> roadmap to production (section 1). If B.W does not pass, the trip stays read-only. *Earlier next-step texts, superseded:* ~~Next: MAX3485 module (section 8) -> arrival tests -> bench re-run B.1–B.7 (+ B.6), then on-site C/D~~ (superseded 2026-11-27, OS1). **Next (2026-11-27):** rewire RO direct + bench re-check B.16, then on-site C/D with the MAX485-M (section 6 "On-site phase-1 decisions"), probe log -> `raw/` + `/capture` -> 2b-B / 2b-C; **2b-A in parallel** (plan approved 2026-11-27, section 6 "Phase-2b plan"). **Amended 2026-11-27 (OS6):** 2b-A (spec 018) paused; next is the architect's control-test design (console writes in the probe image) -> user approval -> spec -> implementation and bench tests, then the site visit with read probe + section E control test. **Amended 2026-11-27 (spec 019 approved, A1–A10; `c6c2a32` / `a3ea455`):** 019a in implementation (`firmware-dev`) -> `build-verifier`; 019b -> `build-verifier` + full `firmware-reviewer`; bench B.W + B.16 with the write image (stays on the board for the trip); trip C, D.1–D.10, E, D.11; then the roadmap to production. B.16 with the old image optional. No bench Modbus slave (user, below). **Earlier (W1–W14 approved):** next is B.16 (RO direct), spec 019 (in progress) -> `firmware-dev` -> `build-verifier` + full review -> bench B.W + B.16 with the write image -> trip: C/D read probe (disarmed) + E control test, one image, no flashing on site (W4); after the trip: probe log -> `raw/` + `/capture`, then revise 018 into a combined 2b+3 plan.

**Task 019 verified facts** (session facts 2026-11-28, `build-verifier` and `firmware-reviewer` reports relayed by the orchestrator; not in raw): host tests **201 / 0 / 0** (`firmware/test/host`; mutation caught by m6b); read image **0x39eb0** B, no write symbols, ELF read mode ok; write image **0x3c120** B, ELF write mode ok; Matter `check_no_bus` self-test 46 (was 38) and it rejects the phase-1 write ELF (new 7(d)); Matter build ok; diffs limited to the allowed files; every checklist console string found in the sources; `firmware-reviewer` **GO** (no blocker / major); `build-verifier` a–g all **PASS**. Not yet verified on hardware: the write image has **not been flashed**; bench B.W.1–B.W.8 pending (incl. the console-race fix, B.W.7 stack ≥ 1024 B). Expected boot header of the write image: `# dmt-probe v1 addr=1 baud=9600 fw=<git short hash of HEAD at build time> writes=console`; the `fw=` string follows git HEAD at build time and changes with every commit, so the log records whatever is printed (`BW.fw`).

**Task 015 verified facts** (orchestrator + independent `build-verifier`, session 2026-11-25, not in raw; local inspection): step 0 passed: esp_matter 1.6.1 resolves registry-only on IDF v6.0.2; `dependencies.lock` = esp_matter 1.6.1, cjson 1.7.19~2, esp_delta_ota 1.1.4, esp_encrypted_img 2.7.0, esp_secure_cert_mgr 2.9.3, mdns 1.15.1 (all components.espressif.com) + idf 6.0.2; no network fetch during build, configure works offline. Build `idf.py reconfigure` + `ninja -C build -j2`; clean build about 26 min, no OOM. Image `daikin_matter_skeleton.bin` 1,728,528 B = 41.2 % of the 4 MB factory partition (`partitions.csv`: nvs 0xC000 @0x9000, phy_init 4 KB, factory 4 MB @0x20000; 16 MB flash). 0 warnings in own code (`-Wall -Wextra -Werror`); 4 third-party `std::is_trivial` deprecation warnings in connectedhomeip headers, left as is. Bus guard `tools/check_no_bus.py`: 38 self-test cases, source scan + POST_BUILD ELF `nm` check, deliberate-failure proof (linked `mb_link_dummy`) failed the build. Phase-1 tree unchanged, phase-1 build passes. Deviation from spec: `app_main` and `thermostat_ep` are C++ (esp_matter API), `reset_button` C; factory reset = GPIO0 held 5 s. Accepted at the feature-end review: `.cpp`, volatile limits, PRE_UPDATE rejection covers the registry / AAI / `set_val` paths *(external: esp_matter `data_model_provider.cpp:370-375`)*.

**Task 016 verified facts** (orchestrator + independent agents, session 2026-11-25, not in raw; local inspection): `sim_source`, `mapping`, `matter_bridge`, `sim_console`, host tests `firmware/matter/test/host` (62 tests / 0 failures / 0 ignored). Target build 0 warnings in own code; image 0x1b3260 B = 42.5 % of the 4 MB factory partition; `check_no_bus` pass; `CONFIG_ESP_MATTER_DEFERRED_ATTR_PERSISTENCE_TIME_MS=60000` resolved. Phase-1 regression: build + lint 372 + ELF check pass, `firmware/test/host` 139/0/0.
- Review fixes: `sim show` copies under the sim mutex, prints after; reset-button task stack 2048 -> 4096; `reg_model.h` include moved out of `extern "C"` in `mapping.h`; "cannot schedule a bridge cycle" warning rate-limited; dead `ESP_ERR_NOT_FINISHED` handling removed (`attribute::update` maps it to `ESP_OK`, *external*: `esp_matter_attribute_utils.cpp:204-206`); `sim show` prints n/a before the first tick; table-driven availability test over the 10 mapped registers (008 not mapped, spec M2) + out-of-range 245/246.
- Accepted deviations: `matter_bridge_start` has a 3rd parameter `update_own` (avoids a circular dependency); bridge starts after `esp_matter::start`, first cycle at once with `first=true`; limits 202/203/245/246 used only if x10 is within 500..4000, else keep + `limits_conflict`; any limit failure -> keep + `limits_conflict`, setpoint outside the limits flagged; `sim_console` owns its UART REPL (prompt `sim> `); `sim show` uses its own `map_state_t`; failures logged once per group; STAT 104 with both or neither mode bit = unusable (*provisional*).
- Notes for 2b / later: move REPL creation to a neutral module when the bus console joins; QR code printed only at `kServerReady` with 0 fabrics (no reprint after remote fabric removal); `sim offline` makes LocalTemperature null only after the stale window (up to 15 s) because `reg_model_fresh` ignores TIMEOUT (check in O4); optional minors left: `sim` usage() line, offline-with-recent-value test case.

**Phase-2a on-device run** (bench observation 2026-11-25, user + orchestrator log reading; not in raw):
- Hardware: boot log confirms 16 MB flash and 8 MB octal PSRAM (memory test OK), console UART0 GPIO 43/44, partition table as `partitions.csv`, CPU 160 MHz ([[entities/esp32-s3-wroom-1]]). Closes the "N16R8 settings inferred" item.
- D1 QR / manual code printed (public test codes) pass. D2 commissioned via the Home Assistant Android app: Google Play Services commissions into Google's fabric first (VendorId 0x6006), then shares to `python-matter-server` (fabric 2, VendorId 0xFFF1) pass. D3 HA shows 21.5 °C, Heat, target 20 °C pass.
- D4 writes of OccupiedHeatingSetpoint (0x12) and SystemMode (0x1C) rejected (`thermostat_ep: rejected external write`), values unchanged, no NVS write: pass. The HA UI optimistically kept the new setpoint until an integration reload (then 20 °C). The IM status code returned to the controller is **not verified** (`python-matter-server` logs nothing at default level).
- D5 / O8 reboot: rejoins without re-commissioning, CASE resumption to the NAS (link-local address with Synology OUI), subscription resumed, values from sim seeds: pass. O1 start values pass. O2 `sim set 0 230` -> HA 23.0 °C pass. O3 `sim stale` -> LocalTemperature null, others kept, `sim live` restores: pass (15–17 s timing not measured). O4 `sim offline` -> null, setpoints / mode kept: pass. O5 201 bit 7 -> Off, cleared -> Heat, 233 = 5 -> Cool: pass. O6 233 = 0 -> last season (Cool) kept, `season_auto=1`: pass.
- O9: setpoint change -> exactly one "Store the deferred attribute" per attribute (0x12, 0x11) 60.0 s later; two changes 4 s apart -> one store per attribute 60 s after the first change with the latest value; no store while idle (observed 64 s only; spec asks 2 min); reboot -> seed 2000 written 2.56 s after boot. SystemMode (0x1C) deferred store also observed. Pass, idle part partial.
- Not done (user chose later): D6 factory reset, D7 Apple Home, O7 SetpointRaiseLower (needs chip-tool).
- New finding: boot log shows `wifi:mode : sta + softAP` and the user's phone sees an unfamiliar Wi-Fi network near the board, although `CONFIG_ENABLE_WIFI_AP` is not set. Fixed by task 017 (next item).
- **Task 017 on device** (bench observation 2026-11-25, user + orchestrator; not in raw): flashed without erase; first boot logged the warning `Wi-Fi mode after init STA+AP, forcing STA`, then the driver logged `mode : sta` without softAP; the extra network disappeared from the phone; HA reconnected without re-pairing; the next reboot logged the expected info line (user: "log showed exactly what you mentioned"). Root cause = hypothesis H2: the closed Wi-Fi driver reported STA+AP after `esp_wifi_init` on this image (also right after erase-flash), and CHIP's `EnableStationMode` treats STA+AP as station-on and keeps it *(external: CHIP `ESP32Utils.cpp:116-120`)*; the guard rewrites the mode (stored in NVS), so later boots are STA. **Conflict (recorded, not resolved):** the IDF header states the default mode is station *(external: `esp_wifi.h:382`)*; the bench observation on this image shows STA+AP after init. Pending: 015 D5 variant (factory reset with 5 s BOOT, then the guard line on the next boot), together with D6.

**Phase-1 bench run 2026-11-27** (user-run, checklist `docs/phase1-probe/checklist.md` section B; user readings and console capture `docs/phase1-probe/bench-2026-11-27.log` (project file, untracked, not in raw); architect assessments, session 2026-11-27; not in raw):
- Setup: app `6654669-dirty` (rebuilt by `idf.py flash`; phase-1 code = `5dd15ff`, identical to `b8ad00d` for the phase-1 paths; dirty only from uncommitted wiki / RESUME). Wiring D23 with VCC from the board 3.3 V pin (section 4); GPIO 5/4/6 (B.1); A/B open; bus module = MAX485-M with R7 fitted. B.2 module measurements: [[entities/max485]]. Static RX check PASS (section 4).
- B.3 header (via `mb log on`): `# dmt-probe v1 addr=1 baud=9600 fw=6654669-dirty`. B.8 PASS: bus UART 1 (TX 5, RX 4, DE 6, 9600 8N1), console UART0 GPIO 43/44 via the CH343.
- B.4.1 PASS (`mb status` in the 5 s window: activity running). B.4.2 PASS: `# silence ok, polling on`, `H 5063 ONLINE`, `H 6192 DEGRADED`, `H 11072 OFFLINE` after the 5th failed poll (POLL + RETRY of 000/008/104/201/231, each TIMEOUT ~510 ms, expected with A/B open). B.4.3 PASS: activity 0, `gap_bytes` 0, `max_byte_interval_us` 0, faults 0, write compiled out. B.4.4 PASS DE 0.02 V.
- **B.4.5 / B.7: "passed on this run, not conclusive with R7 fitted"** (decision R2): every T line TIMEOUT, no BADFRAME. The first boot latched with `activity 1` (`H 5062 LATCHED`, no transmission). Readings in that state (DC vs node GND): DE 0.05 V, RO 3.3 V, GPIO4 3.3 V, A 1.14 V, B 1.14 V, A–B 0.01 V. Architect: activity 1 = one received char or break event; R7 collapses the module bias to about 10–20 mV, below the ±200 mV receiver threshold, so RO is undefined with A/B open and noise can flip it *(inferred)*; not a firmware fault. The module was already on 3.3 V at that boot (user), so it was a genuine one-off. 5 repeat boots and `mb silence 60` (`H 110093 STOPPED`) were clean. ESP-side isolation boot (RO and cathode open): "nothing special" (user). The capture also holds a **second LATCHED boot** (capture lines ~96–163). **Resolved 2026-11-27** (orchestrator reading of the capture, session 2026-11-27; line numbers spot-checked by the wiki agent, local inspection): the second LATCHED boot (`activity 1` at capture line 158, right after `mb log on` at line 92) is the **D23 static RX check boot** (diode cathode to GND held GPIO4 low -> activity 1, latch expected, not a fault); the following boot (line 226, `# silence ok, polling on`) is the **ESP-side isolation boot with activity 0**.
- B.5.1 PASS txns 0. B.5.2 PASS: `mb probe` 32 SWEEP 000..247 all TIMEOUT, health stays STOPPED. B.5.3 / B.5.4 PASS: 5 × `mb silence 5` ok, faults 0 unchanged, no `mb_link ... failed` line. B.5.5 heap not checked (as designed, F1). B.5.6 PASS DE 0.02 V.
- B.6.a DE static low after `mb poll off` and after `mb silence` (0.02 V); during the 5 s boot window not separately measured. B.6.b PASS DE 0.03 V (multimeter average) during `mb probe`. B.6 timing items "not measured (no instrument, Q-HW1)".
- B.9 PASS: no `queue free spaces` line in the capture. B.10 PASS: `bus_task` stack minimum free 2768 B after boot and after `mb silence 1` (3008 B before polling). B.11 PASS (abort path: `mb poll off` in the window, activity running, addr current 1, STOPPED, no T line, txns 0). B.12 pacing followed. B.13 PASS: no MANUAL line after `mb read 000 1` + `mb silence 1`, `H 6073 STOPPED`. B.14 not done (no second adapter). B.15 no port fault occurred.
- **Defect (minor, console only):** console-init race: `bus_task` / `log_out` start before `esp_console_new_repl_uart`, and `uart_param_config` resets the UART0 TX FIFO, so the boot header (printed as `# dmt-`) and usually the first `# silence check: started` line are lost (architect, from the code; observed every boot). No bus effect. Workaround `mb log on` (reprints the header); fix with phase 2b (section 6 "Phase-1 bench decisions"), now task 2b-A + backport (2b-P3, P4). **Update 2026-11-28:** the phase-1 fix is in the 019 images (019b `f65f465`, REPL created before `bus_task`; section 6 2b-P4 row); not yet confirmed on hardware (B.W.1).

## 8. Open items and risks

Closed items and the full earlier wording: [[concepts/firmware-history]] §8.

- ~~Open: phase-2 DE re-check method without scope/analyser (F4); settle before phase-2b bus work.~~ **Decided 2026-11-27 (2b-P8):** on-chip MCPWM capture edge timestamps of the DE and TX pads in a diagnostic build with NVS churn (task 2b-C); method *inferred*, `esp_driver_mcpwm` *external*; not yet proven on hardware.
- Residual risk: DE timing (incl. ISR-in-IRAM, M2) unmeasured in phase 1; evidence limited to console counters and timestamps; re-check at phase-2 bring-up *(inferred)*.
- Risk: flash/NVS stall (Matter) could delay DE release if the TX_DONE ISR is not in IRAM; mitigated by M2; flash/NVS DE run deferred to **phase 2b** (Q-HW2): the 2a image has no bus code (user decision, session 2026-11-24).
- Closed 2026-11-25 (task 015, section 7): CHIP build RAM (`-j2`, no OOM); image size (1.73 MB, 4 MB factory partition); registry-only build on IDF v6.0.2 (step 0 passed).
- Closed 2026-11-25 (task 017, section 7): unintended softAP; station-only confirmed on device (root cause H2; IDF header conflict recorded in section 7).
- Open (on device, after the 2026-11-25 run): D6 factory reset + 017 D5 (guard line on the boot after the reset); D7 Apple Home; O7 SetpointRaiseLower (chip-tool); IM status code of rejected writes (D4) unverified (check with chip-tool or `python-matter-server --log-level debug`); O9 idle check only 64 s of the 2 min asked; O3 stale timing (15–17 s) not measured; reset-task stack headroom; NVS free space for churn unverified (`nvs_get_stats` after commissioning).
- Closed 2026-11-25 (bench observation, section 7): N16R8 flash / octal PSRAM settings confirmed by the boot log; D1–D5, O1–O6, O8, O9 (idle part partial) pass.
- Open (016, *provisional*, decide with the phase-1 probe log; user decision 2026-11-25): (a) leaving standby (201 bit 7 cleared) while 233 is not fresh keeps the previous output, so Off can persist until 233 is fresh again (spec M4 literal); (b) 247 bit 5 set but 245/246 stale or unsupported -> the WEB restriction is silently dropped, published limits can be wider than the supervisor's.
- Closed 2026-11-25: ControlSequenceOfOperation NONVOLATILE question (section 3 "NVS wear"); phase-1 host tests re-run without the CPATH stub after the user installed `libbsd-dev` (139/0/0).
- Hard stops: B.6.b averaged DE near 3.3 V (F3, J2); B.5 fault change (J3); `link counters: n/a` after first H line (J7); D.3 OFFLINE -> D.4 (J6).
- Risk: B.6.b reading during an OFFLINE or cancelled sweep is a false pass (H3); mitigated by J2.
- To verify on site: `bus_task` 4 KB stack (high-water after boot and after `mb silence 1`, T7); too small -> new user decision. Bench 2026-11-27: 2768 B minimum free after boot and after `mb silence 1` (section 7, "Phase-1 bench run"). **Write image (2026-11-28):** B.W.7 requires stack high-water ≥ 1024 B after the B.W steps; less -> STOP, back to the architect (spec 019 B3, I11); bench condition, must pass before the trip.
- Closed 2026-11-27: probe image run on hardware (bench section B, section 7 "Phase-1 bench run"). Still open from it: B.4.5 / B.7 not conclusive with R7 fitted -> repeat B.1–B.7 incl. B.4.5 (+ B.6) on the MAX3485 module (R2); B.14 not done (no second adapter); DE during the 5 s boot window not separately measured. Closed 2026-11-27: second LATCHED boot = static-RX-check boot; isolation boot activity 0 (section 7). Re-run on the MAX3485 superseded for the site run by the B.16 re-check with the MAX485-M (OS1, OS4).
- ~~Open (console-init race, 2026-11-27): boot header and first silence-check line lost on UART0; fix in task 2b-A + backport to phase-1 `main.c` (2b-P3, P4); workaround `mb log on` (also on site, checklist note).~~ **Resolved in code 2026-11-28:** fixed in the 019 images (019b `f65f465`, W11; 018 Part B dropped; section 6 2b-P4 row); to be confirmed on the bench by B.W.1 (header printed at boot). `mb log on` stays harmless as a reprint.
- Open 008 docs minors (excluded by the user): B.4.2 "started" line wording; `mb probe` asynchronous / `mb regs` after the last SWEEP; median 10 ms bucket upper edge and "answered = OK + EXC"; latency line text; 5 s boot window with USB power; C.5 "in D.3" should be D.2. S1–S9 items of spec 008 not listed individually in the session facts.
- Open 011 minors m2–m7: m2 scenario 10a never runs `poll_step` before 5000 ms; m3 untagged request defaults to kind 0; m4 temp files leak under `/tmp` on failure; m5 `FAKE_REPLAY_MAX` 512 may be exceeded by the real 008 log and replay keys on (addr, reg, qty) only (check before using the log as a fixture); m6 no explicit "no RETRY after RETRY" assertion; m7 11b wait loop has no iteration cap (test robustness only, *inferred*).
- Open 013 doc minors (A14): FIFO-rule bypass with `;` in a comment/string; R5 `_second_argument` > 60 s on 5000 × `uart_set_mode(`; optional self-test for unterminated `/*`. The reviewer's 013 bypass table and the 014 M3 reason text are not in the wiki.
- Open hygiene minor: host build prints 4 CMake `component_validation` warnings (test `main` globs component `test_host` sources; cosmetic).
- "Panel is slave" is inferred ([[entities/ekwhctrl1]]); silence-check traffic or no answer -> stop, back to the architect.
- Risk (user-accepted 2026-11-09): unbiased idle line may falsely trip the boot silence check -> stop, back to the architect *(inferred mechanism)*. Seen on the bench 2026-11-27 with A/B open and R7 collapsing the bias (A–B 0.01 V): one LATCHED boot (`activity 1`), later boots clean (section 7). On site with the MAX485-M (weak bias, R7) the relaxed latch procedure OS3 applies (power-cycle for activity 1–7, at most 2 retries).
- Starred registers may be unavailable (no water temperature, no STAT, no OUT) ([[concepts/register-map]]).
- Not stated in the manual: port of the address, "Address 001 – leave free" vs default 1, timing, exception codes, write endurance, panel behaviour without polling (300 s timeout probably irrelevant, *inferred*, [[concepts/flag-registers]]).
- Shared "-" with CP: to measure. Isolation of "A A B B -" from mains not stated in the wiki (node on USB power, section 4).
- Termination / bias of the "A A B B -" segment ([[entities/max485]], [[concepts/bus-topology]] items 3, 3a). The current MAX485-M has a 120 Ω termination fitted (measured 2026-11-27); whether the node end needs termination is still decided on site with the replacement module. With 120 Ω across A–B, check that the module's bias still gives a fail-safe idle *(inferred)*. On-site phase-1 run (OS1): MAX485-M with R7, no separate 120 Ω (section 4 rule); optional D.2a A–B DC at the panel (≥ 0.2 V = panel biases); with the undefined idle, BADFRAME / `gap_bytes` / TIMEOUT may come from line noise, an error rate ≥ 1 % is recorded as conditional ("X % with MAX485-M + R7 at 3.3 V; repeat with MAX3485"), no firmware setting changed (checklist D.8).
- Open (D23, 2026-11-27): **MAX3485 (3.3 V) module** to buy, now for **deployment** (and optionally 2b); the on-site phase-1 run no longer waits for it (OS1, amended 2026-11-27). Project-doc edits applied 2026-11-27 for the MAX485-M site run (`docs/BOM.md`, `docs/phase1-probe/checklist.md`).
  - **Buying criteria** (architect, session 2026-11-27, not in raw; electrical reasons *inferred*): 3.3 V MAX3485 / SP3485-class chip with a readable marking; DE pin present (DE+ and /RE separate, or one RTS/EN pin); **no pull-up on DE or /RE** (pull-down fine; fallback: accept only if the arrival test gives DE ≤ 0.5 V); **no auto-direction boards** (conflict with D8 / sole master and RTS-driven DE); termination absent or switchable (fixed 120 Ω only OK if the panel end is also about 120 Ω, checklist C.3); bias present preferred, strong bias or true fail-safe receiver (with 120 Ω at 3.3 V about ≤ 700 Ω per leg for > 200 mV, *inferred*); pins pre-soldered; screw terminal; not isolated for now.
  - **Arrival tests** (architect): unpowered, both lead polarities: DE–VCC and /RE–VCC (expect open), DE–/RE, DE–GND, A–B, A–VCC, B–GND, RO–VCC, VCC–GND, chip photo. Powered at 3.3 V with GPIO6 undriven: DE about 0 V (≥ 0.5 V -> stop), A–B idle voltage.
  - **Wiring** with the 3.3 V module (architect): RO direct to GPIO4, DI direct from GPIO5, DE+ / /RE to GPIO6 with 10 kΩ to GND, VCC from the board 3V3. No firmware change.
- Closed 2026-11-27 (D23): bench RX circuit (BZX55C5V1 forward + 20 kΩ) static check PASS (section 4). Still open: the bench DE readings show DE only after firmware init; the **pre-init DE level is not covered** by the bench run (with the replacement module it should be held low by the 10 kΩ pull-down, *inferred*).
- **Risk accepted by the user (2026-11-27, OS2; D8 unchanged; architect assessment, mechanism *inferred*):** on the MAX485-M the DE + /RE pull-ups (5 kΩ to 3.3 V) hold DE ≈ 2.2 V (> V_IH) while GPIO6 floats, so the **driver is enabled for about 0.3 s per power-up / reset** (until `app_main`, logged at 289 ms in the bench capture, whose first statement is `mb_link_hold_receive`, `firmware/main/main.c:93`; local inspection) and **unbounded while EN/RST is held, in download mode, or in a boot loop before `app_main`**. DI is pulled up, so the driver sends a steady mark (= idle with correct polarity); with A/B swapped the panel sees an about 0.3 s break (expected harmless, *inferred*). Each serial-monitor open that auto-resets the chip adds one window. Checklist A.5/A.6/D.4 carry the operating rules. Rejected mitigation: 10 × 10 kΩ parallel (≈ 1 kΩ) pull-down.
- ~~Open (*inferred*, 2026-11-27): an image without bus code (the phase-2a Matter image, 2a-2: "DE not driven, pull-down holds receive") never drives GPIO6, so with the MAX485-M wired to a live bus the driver would stay enabled **for the whole run**; rule not decided.~~ **Decided 2026-11-27 (user, OS5, section 6):** hard rule, such an image never runs with A/B wired; disconnect A/B before re-flashing the Matter image after the site run.
- Open (2b-B, architect assumption A4 for spec 018, written there as S7; session 2026-11-27, not in raw): the Matter image runs `CONFIG_FREERTOS_HZ=1000` (esp_matter requirement, task 015) vs 100 Hz in phase 1, so the `rs485_port` end-of-frame window shrinks from about 20–30 ms to about 5–6 ms, still ≥ t3.5 = 4.0 ms at 9600 *(external, Modbus spec)*; `dur_ms` in the 2b images will differ from the probe log. Decide in 2b-B with the probe log (re-tune 2b-P10).
- Minor (2026-11-27): the current module's DI pull-up to 5 V injects about 0.1–0.2 mA into GPIO5 (outside the ESP32-S3 spec; bench only) (architect, *inferred*). Moot with the bench VCC at 3.3 V (pull-up to 3.3 V, *inferred*).
- Open (bench power, 2026-11-27): the board 5 V pin "seems not working" (user); unverified whether it is only fed from the other USB connector. MAX485-M ran at 3.3 V, out of the 5 V ±5 % rating if U1 is a 5 V part *(inferred)*; bench results are from that condition. The on-site phase-1 run uses the same 3.3 V condition (OS1).
- FC03 qty > 1 unknown.
- Whether the panel enforces 202/203/245/246 is unknown -> the node enforces them.
- Risk (E2): own code in the TX path (`mb_rtu`, `rs485_port`); mitigated by review, inspection checklists and host tests *(inferred)*.
- Risk (E2): inter-byte timing of the answering slave on "A A B B -" unknown; own frame-end detection depends on it (about 20–30 ms window); checked by 008 D.8a *(inferred)*.
- Risk (M3): `gap_bytes` / `max_byte_interval_us` approximate; quote only as bounds.
- Fallback E1b remains available; revert is a separate user decision (section 2).
- RMW race on 201 accepted.
- How Matter ecosystems render the Fan endpoint *(external)*.
- Not addressed in v0.3: SystemMode under 233 = 0 when STAT answers but both or neither of bits 0/1 are set. *Provisional* in 016 (2026-11-25): treated as unusable; confirm with the probe log.
- Risk (phase-1 write variant, W1–W14, user decisions 2026-11-27): **write endurance unknown**; whether 231 / 201 / 233 are stored in EEPROM is not stated in the manual *(inferred possibility, as for D22)*; mitigated by W8 (≥ 5 s spacing, ≤ 40 FC06 frames per arm session), W7 (no automatic resend, latch after 3 failures) and the short section-E test; no per-boot cap (W8).
- Risk (W5, W12): **season / plant side effects**: writing 233 (and standby / fan) changes the real plant; the user knows the effect of the season change (W12); mitigated by running the season test last, ≤ 60 s, restore 201 -> 233 -> 231 (W13) from the restore set of the first arm (W6). Panel or plant reaction to a season flip is not stated in raw.
- Risk (schedule, W2 / W4): if task 019 is not built **and** reviewed (full review, S3) and bench-tested (B.W + B.16 with the write image) before the trip, **the trip stays read-only** (C/D only, no section E); no flashing on site (W4). **Update 2026-11-28:** 019 is built and reviewed (`firmware-reviewer` GO, `build-verifier` a–g pass, section 7); the remaining condition is the bench run **B.W.1–B.W.8** with the write image (incl. B.W.2 = B.16 repeat and B.W.7 stack).

- **Accepted by the user (2026-11-27, session, not in raw):** no bench Modbus slave (second adapter declined), so the **first real FC06 frame goes out on site**; pre-trip verification of the write path = host tests against the fake slave (task 011 harness) only.
- Closed 2026-11-28 (spec 019 B1): Matter write guard. The Matter project never had a `CONFIG_DMT_BUS_WRITE` FATAL; its POST_BUILD `check_no_bus.py` now forbids `write_guard_` / `bus_write_` symbols (section 3). Verified by `build-verifier` 7(d) (rejects the phase-1 write ELF), session fact 2026-11-28.
- Open (accepted, spec 019 B3, 2026-11-27): the `log_out` ring drops lines when full (`emit` writes only to the ring); a dropped `T` / `W` / `A` line is missing from the console and from the saved output. Mitigation is procedural only: checklist E.0.5 / E.9 compare `log: on, dropped <n>` and record `E9.dropped` if it rose.
- Open (on site, 2026-11-27): 247 bit numbering (FLAG n = bit n, bit 0 = LSB) is *inferred* (medium-high) from the "FLAG n (247.n)" labels ([[concepts/web-restriction-flags]]); confirm by reading 247 on site before relying on W9.
- Open (2026-11-27): panel behaviour on an **out-of-range write to 231** is not stated in the manual (p4 Note 2 only says "write the value within the selected scale", [[concepts/register-map]]); the node must stay inside the limits. FLAG 4 / FLAG 5 texts describe local keys only; their effect on Modbus writes is not stated.
- Open (2026-11-27): the p6 persistence sentence ("the data is maintained") may or may not cover 231 / 233 / 201; no EEPROM / write-endurance / write-rate statement in the manual (re-checked by `datasheet-reader` 2026-11-27).
