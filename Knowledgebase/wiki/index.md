# Wiki Index

Catalog of all wiki pages. Maintained by the agent; see root `CLAUDE.md`.

## Sources
- [n420384a-english](sources/n420384a-english.md) — Daikin N420384A Modbus RTU manual (file 1; +AB-, 500 m / 50 units limits)
- [n420384a-modbus-rtu](sources/n420384a-modbus-rtu.md) — same manual, file 2 (+BA-, S1 grid switch; differences listed)
- [max485-datasheet](sources/max485-datasheet.md) — MAX485 family chip datasheet: 5 V supply, pinout, DE/RE, RO level
- [max485-module-photo](sources/max485-module-photo.md) — user photo of the MAX485-M breakout: RO/RE/DE/DI header, U1 marking illegible; R1–R4 at the logic pins, R7 "121" = 120 Ω termination (conflict with user statement resolved by measurement 2026-11-27)
- [project-brief](sources/project-brief.md) — user brief: goal, hardware, dev-env rules

## Concepts
- [project-goal](concepts/project-goal.md) — Matter-over-Wi-Fi thermostat scope; v0.3 approved 2026-11-08; phase 1 complete, bench run (checklist B) done 2026-11-27; on site with the MAX485-M after bench re-check B.16 (user 2026-11-27); phase-2a plan approved 2026-11-24 (separate Matter project, EP1 read-only on a simulated model); phase-2b plan approved 2026-11-27 (2b-A before the probe log, 2b-B/2b-C after); working mode S1–S5
- [bus-topology](concepts/bus-topology.md) — node <-> EKWHCTRL1 "A A B B -" point-to-point, node sole master, nothing else connected (user 2026-11-08); EKWHCTRL1 slave (inferred); phase-1 test questions; v0.3 firmware plan pointer (D1 hard rule, gate exception)
- [dev-environment](concepts/dev-environment.md) — ESP-IDF + component-registry-only rule, host state, approved components (esp_matter ^1.6.1; esp-modbus removed 2026-11-24, E2; own project code compliant with registry-only rule); host-target facts; ELF-check scope; Matter controllers (chip-tool, HA Core + python-matter-server, Apple Home; SmartThings later); python-matter-server NAS setup, router IPv6/roaming fix, WSL2 flashing via usbipd-win (2026-11-25)
- [modbus-rs485-link](concepts/modbus-rs485-link.md) — 9600 8N1, FC 03/06 (port not named), cable/120 Ω rules ambiguous as to port, EKRTCTRL1 +AB-/+BA- legend conflict
- [register-map](concepts/register-map.md) — full register table, star list (INN-FR-B32 only) checked, 230/234 star inconsistency, quirks
- [flag-registers](concepts/flag-registers.md) — PRG, OUT, STAT, ALR_STAT bit tables (visually verified), label conflicts; 300 s timeout probably not about "A A B B -"
- [web-restriction-flags](concepts/web-restriction-flags.md) — reg 247 supervisor flags, persistence (FLAG 7 cleared), FLAG 1 "247.0" typo, troubleshooting
- [firmware-architecture](concepts/firmware-architecture.md) — v0.3: sole master on "A A B B -", phases (1 probe, 2a/2b mirror, 3 writes), components, E2 own FC03 RTU master, bus-safety and lint rules, wiring/DE, Matter mapping, all decisions (D1–D23 (D23 2026-11-27: MAX3485 module for deployment, bench RX diode circuit, bench VCC 3.3 V; amended: MAX485-M on site), phase-1 bench decisions R2 / console-race fix with 2b, on-site phase-1 decisions OS1–OS5 (MAX485-M, D8 unchanged with user-accepted boot-window risk, relaxed latch rule, RO direct, OS5 hard rule: no image without early DE hold (2a Matter) with A/B wired) + B.16 latch default, OS6 on-site console control test after the read probe, console-write decisions W1–W14 (approved: separate phase-1 write variant `CONFIG_DMT_BUS_WRITE` overlay / `build_write/`, one image for the trip, write set 231 / 201 bit 7 / 201 bits 0–2 / 233, arming, pacing, latch, E.0 prerequisite; task 019), phase-2b plan 2b-P1..P10 + tasks 2b-A/B/C, S1–S5, phase-2a plan 2a-1..10 (2a-10 NVS persistence), stage-1, U, E, L/A, M, T, F/J, task 011) as compact rows, task status table (015 `9dbace6`, 016 `03dacf5`, both committed; 018 (2b-A) spec paused, Part B dropped; 019 write-variant spec being written), 015/016 verified facts, first on-device run 2026-11-25 (D1–D5, O1–O6, O8, O9 pass; softAP fixed by 017 `6b10c93`, STA only on device), phase-1 bench run 2026-11-27 (B.1–B.15 results, B.4.5/B.7 not conclusive with R7, second LATCHED boot = static RX check, console-init race), open items/risks incl. accepted DE boot-window risk, write endurance / season side effects / schedule risk (no 019 before the trip -> read-only trip), 2b-B item 1000 Hz tick end-of-frame window, MAX3485 buying criteria + arrival tests
- [firmware-history](concepts/firmware-history.md) — progress diary split off firmware-architecture 2026-11-24 (S2): header addenda, full task review/acceptance subsections, verification output, section 7/8 full text, superseded lint/D7 texts, superseded v0.2 table
- [device-addressing](concepts/device-addressing.md) — address 1–255 (reg 200), keypad [+]/[—] procedure, "leave 001 free"; port not stated

## Entities
- [ekrtctrl1](entities/ekrtctrl1.md) — wall control, board connections
- [ekwhctrl1](entities/ekwhctrl1.md) — wall control: "+ A B -" = fancoil link, "A A B B -" = Modbus RTU port (6-way block shared with CP); port facts, slave inferred; node taps "A A B B -"
- [ekwhctrl0](entities/ekwhctrl0.md) — kit installed with EKWHCTRL1
- [inn-fr-b32](entities/inn-fr-b32.md) — fancoil control PCB (N540022B in figures) with the "+AB-" Modbus terminal
- [esp32-s3-wroom-1](entities/esp32-s3-wroom-1.md) — target MCU module; 16 MB flash / 8 MB octal PSRAM confirmed by boot log 2026-11-25; bench UART/console pins and 5 V pin issue 2026-11-27
- [max485](entities/max485.md) — RS485 breakout module + chip facts; B.2 bench measurements 2026-11-27 (10 kΩ pull-ups on all logic pins, bias, 120 Ω termination fitted); ran at 3.3 V on 2026-11-27, A–B idle 0.01 V with R7 fitted; used for the on-site phase-1 run (RO direct, user-accepted DE boot-window risk); MAX3485 module for deployment (D23 amended)
