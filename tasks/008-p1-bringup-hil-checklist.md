# 008 - Phase-1 on-site probe: checklist and measurement log template (document task)

Phase: 1. Status: spec ready (v0.3, 2026-11-08; amended 2026-11-09: automatic polling start, normal USB power, user decisions). Depends on: 006 (command names), 002 (spike answers S5/S7/S10). May be drafted in parallel with 010/011; finalised after 006 and the 002 spike report.
Review: **bus-touching** (it tells the user how to connect and transmit). `firmware-reviewer` reviews it independently against the wiki and the 006 command list.

A **document for the user**, not firmware. The user performs it on site; the filled log goes to `Knowledgebase/raw/` and is the gate input for the phase-2/3 specs (D2, Q8).

## Goal
A safety-first, step-by-step checklist to connect the probe node to the EKWHCTRL1 **"A A B B -"** port, read registers with the phase-1 image and answer exit items 1-11 of architecture section 1; plus a log template with a field for every item and measurement.

## Files / directories owned
- `docs/phase1-probe/checklist.md` (new)
- `docs/phase1-probe/measurement-log-template.md` (new)

## Wiki pages relied on
- `concepts/firmware-architecture`, sections "1. Phases and exit criteria" (items 1-11, the user-verified items), "3. Bus-safety rules" (Fallbacks, D17), "4. Hardware wiring" (incl. termination rule), "8. Open items and risks"
- `concepts/bus-topology` (open items 1, 1b, 2, 3, 3a, 4, 5, 7), `entities/ekwhctrl1` (6-way block, CP on the same block)
- `concepts/device-addressing` (keypad procedure p6; "leave 001 free" vs default 1), `concepts/modbus-rs485-link`, `concepts/register-map`, `concepts/flag-registers`, `concepts/web-restriction-flags`
- `entities/max485` (RO 5 V level, supply unknown, no on-board termination, bias unknown), `entities/esp32-s3-wroom-1`

## Content requirements

### A. Safety (first page, tick boxes, before any wiring step)
1. Fancoil and wall panel are mains powered. Open housings, connect or disconnect wires and do resistance/continuity measurements **only with mains off**, following the appliance manual.
2. **Never touch the "+ A B -" block** (panel <-> fancoil) **and never the CP terminal** of the "A A B B -" block: do not loosen, measure on or reconnect them. Only one A, one B and "-" of "A A B B -" are used.
3. Node GND to "-" **only after** C.4 showed "-", A and B within -7..+12 V of node GND (architecture section 4, RS485 common-mode range, external). The node runs on normal USB power (user decision 2026-11-09). Note: whether the panel's low-voltage side is isolated from mains is not stated in the wiki (architecture section 8).
4. Live voltage readings (C.4) only on the low-voltage terminals with mains parts covered; if that is not possible, skip them, leave node GND unconnected and say so in the log.
5. **Abort** (unexpected panel/fancoil behaviour, error code, icon, smell, heat): type `mb poll off`, unplug the node's USB power (DE then held low by the 10 kOhm pull-down / MAX485 unpowered), mains off before touching any wire. Record what happened. Unplugging USB alone also stops all transmission.
6. **The node transmits on its own.** With a clean boot silence check, polling starts automatically and the first frame goes out about 5 s after power-up (006, user decision 2026-11-09). Therefore: connect or disconnect A/B only with the node unpowered (and mains off, A.1); open the serial console before powering the node while it is wired, ready to type `mb poll off`. A `mb poll off` typed within the first 5 s prevents the first frame (006); later it stops polling after the current transaction.

### B. Bench (before going on site)
1. Wiring per architecture section 4: TX GPIO -> DI; RO -> level shift/divider -> RX GPIO (RO is a 5 V output unless U1 is a 3.3 V part, `entities/max485`); DE and /RE tied -> DE GPIO with 10 kOhm to GND (Q1, D8). Record the GPIOs; they must equal `CONFIG_DMT_BUS_TX_GPIO` / `RX_GPIO` / `DE_GPIO` (defaults 5/4/6 are placeholders). Record U1 marking and module supply.
2. Unpowered module: A-B resistance; DE-/RE continuity; DE-GND = 10 kOhm.
3. Flash the phase-1 image; record the git commit and that `build-verifier` reported lint and ELF check passed.
4. Node powered, bus not connected: after about 5 s an `H ... ONLINE` line appears (automatic start), then `H ... DEGRADED` and, at the 5th failure, `H ... OFFLINE` (no slave). `mb status` shows boot silence activity 0, `write: compiled out`. Then `mb poll off` (`H ... STOPPED`) and measure the DE pin **low** (multimeter). If activity > 0 with A/B open, the receiver sees an unbiased floating line (bias resistors on the module unverified; this risk note user-approved 2026-11-09): record and stop; this goes back to the architect, not to a relaxed firmware setting.
5. Abort-path check: reboot and type `mb poll off` within the first 5 s: `mb stats` shows 0 transactions and health STOPPED (A.6). Then `mb probe` (no slave): all TIMEOUT, health stays STOPPED. `mb silence 5` five times: passes each time, free heap in `mb status` stable (S10). DE pin low again afterwards.

### C. On site, mains off, node not connected
1. Photo of both terminal blocks before any change.
2. Continuity A-A and B-B of the doubled terminals (item 11); choose one A and one B.
3. Resistance A-B at the panel port (item 11; termination rule of architecture section 4: about 120 Ohm -> fit the user's 120 Ohm at the node end; high/open -> start without and judge by the error rate of item 7). Resistance "-" to the CP terminal (shared "-"?, item 11; measure without touching the CP wiring).
4. Mains on, node powered but not wired to the panel: with the meter between node GND and each panel terminal, DC voltage of "-", A, B; then A-B at the panel. Mains off.
5. Confirm the panel address on the panel itself (item 1): standby, hold AUTO 5 s, read the value, wait 10 s without changing anything (`concepts/device-addressing`, p6). Record it. If it differs from the image's address, use `mb addr <n>` in D.3 (never scan).

### D. Connect and probe
1. Mains off, node unpowered: connect node A/B (and GND to "-" only if A.3 allows) with short leads, no star; fit 120 Ohm only if C.3 says so. Mains on.
2. Open the serial console and start saving the complete serial output, then power the node. Boot silence check runs 5 s; `mb status` must show activity 0 (item 3). **If activity > 0 the node latches and will not transmit**: do not reboot to retry; abort and record (foreign traffic or an unbiased line; both go to the architect). With activity 0 polling starts by itself (`H ... ONLINE`, A.6): watch the panel from the first frame (item 9). If C.5 gave an address other than the image's, type `mb poll off` within the 5 s window, then `mb addr <n>` and `mb poll on` (if the window was missed: `mb addr <n>` at once).
3. Logging is on from power-up (006), so the saved output holds every `T` line from the first frame; type `mb log off` only if the console is too noisy to work in (then `mb log on` before D.7). `mb probe`, then `mb regs` (items 1, 2, 4).
4. If nothing answers: `mb poll off`, unplug the node, mains off, swap A/B on the node side only, mains on, repeat D.2-D.3 (item 2). Still nothing: `mb poll off`, stop and report.
5. Multi-register test: `mb read 200 4`, `mb read 000 2`, `mb read 242 4` (item 5).
6. Polling has run since D.2 (`mb poll on` only if it was stopped). Watch the panel display for anything new, e.g. a WEB icon (item 9, `concepts/web-restriction-flags` flag 0).
7. Keypress correlation with logging on (default; `mb log on` only if it was switched off in D.3), each action with wall-clock time: setpoint up, setpoint down (231 vs 008), each fan mode (201 bits 0-2 order), season change (233 values 3/5), standby on/off (201 bit 7) (item 8).
8. Soak >= 1 h of polling; `mb stats` at 1, 10, 60 min: >= 1000 transactions, latency min/median/max (item 6), error rate < 1 %, no OFFLINE entry; panel normal: display, icons, error codes, local keys (item 7).
9. `mb poll off` for > 300 s (e.g. 10 min): watch panel and fancoil; `mb poll on`; `mb regs` for 105 bit 0 and, if readable, 104 bit 12 (item 10, `concepts/flag-registers`).
10. `mb silence 60` with the panel connected: expected activity 0 (item 3, panel is a slave, inferred).
11. `mb poll off`, unplug the node, mains off, disconnect the node, photo, mains on; confirm panel and fancoil work as before and that "+ A B -" was untouched.

### Exit items (architecture section 1; phase 1 is done when each has an answer or "unknown + reason")
| # | Item (short) | Steps | Bus-topology item |
|---|---|---|---|
| 1 | Panel slave address: on the panel and by a bus reply | C.5, D.3 | 1 |
| 2 | A/B polarity: valid replies at 9600 8N1 | D.3, D.4 | 5, 1b |
| 3 | Silence check saw no foreign traffic | D.2, D.10 | 1 |
| 4 | Every documented register: OK + raw/scaled, exception + code, or timeout | D.3 | 4 |
| 5 | FC03 qty > 1 works? | D.5 | 1b |
| 6 | Response time min/median/max over >= 1000 transactions | D.8 | 2 |
| 7 | >= 1 h polling, error rate < 1 %, no panel anomaly | D.8 | 2 |
| 8 | Keypress correlation 231/008, 201 bits 0-2, 233, 201 bit 7 | D.7 | - |
| 9 | Panel shows anything when polled (e.g. WEB icon) | D.2, D.6 | - |
| 10 | Panel behaviour when polling stops > 300 s and resumes | D.9 | 2 |
| 11 | A-A/B-B continuity, A-B resistance, "-" vs node GND voltage, U1/supply/RO level, "-" shared with CP | B.1, C.2-C.4 | 3, 3a, 7 |

State that the user personally verifies items 1, 2, 7, 8, 9, 10, 11 and that "+ A B -" was untouched (architecture section 1).

### Log template
Header block (date, git commit, GPIOs, module U1/supply, address used, termination fitted y/n); one table or field per exit item and per measurement step, blank cells, units stated; panel-action table (time, action, registers changed, old -> new); "unexpected observations" free text. Last lines: "Save the filled log and the complete serial output to `Knowledgebase/raw/` (e.g. `raw/phase1-probe-<date>.md`, `raw/phase1-probe-<date>.log`) and ask `wiki-keeper` to run /capture. The `.log` can be copied to `firmware/test/fixtures/probe-<date>-capture.log` (011)."

## Acceptance checks
1. Every console command named exists in 006 with the same spelling and arguments.
2. Every exit item 1-11 and every bus-topology item listed has at least one log field.
3. A.1-A.6 appear as tick boxes before the first wiring step; no step asks for battery power or a USB isolator (user decision 2026-11-09).
4. Every fact cites its wiki page; external facts marked "external"; nothing the wiki marks open or inferred is stated as fact (e.g. "panel is a slave" is inferred).
5. Plain language, numbered steps, tick boxes, no emojis.

## Test strategy
Review only (`firmware-reviewer`, against the wiki, 006 and the 002 spike report); no code.

## Recommended implementer model
sonnet: safety-relevant document cross-checked against several wiki pages and the console spec; haiku risks dropping or overstating facts.

## Out of scope
- Firmware changes; phase-2/3 steps; any write; address scanning; touching "+ A B -" or CP.
