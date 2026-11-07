# 008 - P1 bring-up HIL checklist and measurement log template (document task)

Phase: P1. Status: spec ready. Depends on: 006 (command names); can be drafted in parallel with 003-007 and finalised after 006.

This is a **document for the user**, not firmware. The user performs it on site; the filled log becomes a raw source for the wiki.

## Goal
Write a step-by-step, safety-first checklist that lets the user connect the listen-only node to the EKWHCTRL1 "+ A B -" link without any chance of disturbing it, and a measurement log template whose answers close (or narrow) the bus-topology open items needed before P2 and P3.

## Zero-TX rules (restate prominently at the top of the checklist)
- MAX485-M **DE and /RE wired to GND**, **DI not connected**: verify with a continuity tester before connecting the bus.
- Firmware: P1 image only (listen-only: `UART_MODE_UART`, RX pin only, no TX code, no esp-modbus controller; lint and ELF check passed in the build report). Record the git commit of the flashed image.
- No bias or termination resistor added by the node in P1; the separately bought 120 ohm is **not** fitted in P1.
- If anything on the existing system behaves differently after connecting (panel error, "Com err", fancoil stops responding): disconnect A/B of the node first, then investigate.

## Files / directories owned
- `docs/p1-bringup/checklist.md` (new)
- `docs/p1-bringup/measurement-log-template.md` (new)

## Wiki pages relied on
- `concepts/firmware-architecture` (P1 scope, console commands, D1/D2)
- `concepts/bus-topology` (open items 1, 1b, 3, 3a, 4, 5; tap point "+ A B -")
- `concepts/modbus-rs485-link` (9600 8N1, 120 ohm, no star, cable rules)
- `entities/max485` (DE/RE, RO 5 V level, no on-board termination, R7 unexplained, supply unknown)
- `entities/ekwhctrl1`, `entities/inn-fr-b32`, `entities/esp32-s3-wroom-1`
- `concepts/flag-registers` (STAT bit 12 "Com err" 300 s master timeout; STAT/OUT INN-FR-B32 only)

## Content requirements
Checklist sections, in order:
1. **Safety**: the fancoil and panel are mains powered; open housings and touch terminals only with mains off, following the appliance manual; only the low-voltage "+ A B -" terminals are used.
2. **Before connecting (bench)**: identify U1 / module supply if possible; measure the module A-B resistance unpowered (record; tells whether R7 is a termination); continuity DE-GND, /RE-GND, DI open; level path RO -> ESP32-S3 RX GPIO (level shifter or proven 3.3 V module, D2) and common ground plan; confirm the RX GPIO equals `CONFIG_DMT_BUS_RX_GPIO` (default 4 is a placeholder) and avoid strapping/USB/PSRAM pins (external ESP32-S3 datasheet facts, to be confirmed by the user's board docs).
3. **On site, bus not yet connected**: DC voltages "+" to "-", A to "-", B to "-", A to B with the existing system running (D2: measure "+"/"-"); line termination measurement as far as possible (open item 3, needs mains off and both ends accessible); photo of the wiring.
4. **Flash and boot without bus**: flash the P1 image (flashing path open: usbipd vs Windows, `concepts/dev-environment` open item 1); `bus status` shows zero traffic and no errors; `bus rx restart 100` -> heap equal before/after, no errors.
5. **Connect** ground, then A and B of the node (short stub, no star) with the system running; watch the panel for 5 minutes.
6. **Observe**: `bus status`, `bus addrs`, `bus regs`, `bus timing` after 1 min, 10 min, 60 min; `bus dump chunks on` for at least 2 minutes and save the log; with dump on, the user performs on the wall panel: setpoint up/down, mode change, fan mode change, standby on/off, each noted with wall-clock time.
7. **Polarity / baud fallback**: if no valid frames and many frame errors: disconnect the node's A/B, swap them on the node side only, retry (open item 5); if still no valid frames, rebuild with another `CONFIG_DMT_BUS_BAUD` only after asking the architect (open item 1b).
8. **Disconnect and restore**; confirm the panel and fancoil work as before.

Measurement log template: tables with blank cells for every value above, plus explicit questions mapped to open items:
- Q-1 (item 1): which slave addresses are addressed; is there one requester pattern; poll cycle period per register.
- Q-1b: valid CRC frames at 9600 8N1? frame error count.
- Q-4 (item 4) and D15 risk: are 104 (STAT) and 009 (OUT) ever read on the link? values seen? (if STAT never appears, the D15 Heat/Cool display under 233 = 0 has no data source: report to the architect).
- Which of 000, 008, 105, 201, 202, 203, 231, 233, 245, 246, 247 appear, read or written, and how often (inputs for the P2 mapping).
- Which registers the panel writes (FC06) on each user action (table: action, time, register, value).
- Idle gaps: histogram from `bus timing` (input for the P3 idle-window rule, item 2).
- Undocumented registers or function codes seen.
- Exceptions seen.
- `bus rx restart` result (U2 reinstall precondition).
- Module A-B resistance, bus voltages, termination findings (items 3, 3a).
State at the end: "Save the filled log and the raw dump to `Knowledgebase/raw/` (for example `raw/p1-measurements-<date>.md` and `raw/p1-dump-<date>.txt`) and ask `wiki-keeper` to run /capture; the dump can also be copied to `firmware/test/fixtures/` as a `source=capture` fixture."

## Acceptance checks
1. Every console command named exists in 006 with the same spelling.
2. Every open item listed above (1, 1b, 3, 3a, 4, 5) and the D15 STAT risk has at least one log field.
3. Each zero-TX rule appears in the checklist with a tick box.
4. Every fact cites its wiki page; external facts are marked "external"; nothing about the bus is stated as fact that the wiki marks open or inferred.
5. Plain language, numbered steps, tick boxes; no emojis.

## Test strategy
Review by `firmware-reviewer` (or the architect) against the wiki pages and 006's command list; no code.

## Recommended implementer model
sonnet: a safety-relevant document that has to be cross-checked against several wiki pages and the console spec; haiku risks dropping or overstating facts.

## Out of scope
- Firmware changes; P2/P3 steps; any transmit test; fitting the 120 ohm resistor; deciding the TX path or tap point (D1: after P1).
