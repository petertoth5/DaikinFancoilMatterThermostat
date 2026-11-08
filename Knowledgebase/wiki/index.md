# Wiki Index

Catalog of all wiki pages. Maintained by the agent; see root `CLAUDE.md`.

## Sources
- [n420384a-english](sources/n420384a-english.md) — Daikin N420384A Modbus RTU manual (file 1; +AB-, 500 m / 50 units limits)
- [n420384a-modbus-rtu](sources/n420384a-modbus-rtu.md) — same manual, file 2 (+BA-, S1 grid switch; differences listed)
- [max485-datasheet](sources/max485-datasheet.md) — MAX485 family chip datasheet: 5 V supply, pinout, DE/RE, RO level
- [max485-module-photo](sources/max485-module-photo.md) — user photo of the MAX485-M breakout: RO/RE/DE/DI header, U1 marking illegible; R7 "121" vs user: no on-board termination
- [project-brief](sources/project-brief.md) — user brief: goal, hardware, dev-env rules

## Concepts
- [project-goal](concepts/project-goal.md) — Matter-over-Wi-Fi thermostat scope; architecture v0.3 approved 2026-11-08; gate open for stage-1 (probe) specs only; specs ready 2026-11-09; task 001 done 2026-11-09, 009 next
- [bus-topology](concepts/bus-topology.md) — node <-> EKWHCTRL1 "A A B B -" point-to-point, node sole master, nothing else connected (user 2026-11-08); EKWHCTRL1 slave (inferred); phase-1 test questions; v0.3 firmware plan pointer (D1 hard rule, gate exception)
- [dev-environment](concepts/dev-environment.md) — ESP-IDF + component-registry-only rule, host state, approved components (esp-modbus ==2.1.4 as serial master, esp_matter ^1.6.1); host-target facts (libbsd-dev pending, `--preview set-target linux`, no clang-tidy)
- [modbus-rs485-link](concepts/modbus-rs485-link.md) — 9600 8N1, FC 03/06 (port not named), cable/120 Ω rules ambiguous as to port, EKRTCTRL1 +AB-/+BA- legend conflict
- [register-map](concepts/register-map.md) — full register table, star list (INN-FR-B32 only) checked, 230/234 star inconsistency, quirks
- [flag-registers](concepts/flag-registers.md) — PRG, OUT, STAT, ALR_STAT bit tables (visually verified), label conflicts; 300 s timeout probably not about "A A B B -"
- [web-restriction-flags](concepts/web-restriction-flags.md) — reg 247 supervisor flags, persistence (FLAG 7 cleared), FLAG 1 "247.0" typo, troubleshooting
- [firmware-architecture](concepts/firmware-architecture.md) — v0.3 (approved 2026-11-08): sole master on "A A B B -", phases probe -> read-only mirror -> writes, components, bus-safety rules, wiring, Matter mapping, D1–D22 + stage-1 detail decisions (2026-11-09: auto polling after silence check, USB power, lint/health/Kconfig details, boot-window commands, `mb silence` leaves polling off, `mb log` default on; lint R1–R8 per task 001), task impact (001 done 2026-11-09, 009 next); v0.2 as superseded history
- [device-addressing](concepts/device-addressing.md) — address 1–255 (reg 200), keypad [+]/[—] procedure, "leave 001 free"; port not stated

## Entities
- [ekrtctrl1](entities/ekrtctrl1.md) — wall control, board connections
- [ekwhctrl1](entities/ekwhctrl1.md) — wall control: "+ A B -" = fancoil link, "A A B B -" = Modbus RTU port (6-way block shared with CP); port facts, slave inferred; node taps "A A B B -"
- [ekwhctrl0](entities/ekwhctrl0.md) — kit installed with EKWHCTRL1
- [inn-fr-b32](entities/inn-fr-b32.md) — fancoil control PCB (N540022B in figures) with the "+AB-" Modbus terminal
- [esp32-s3-wroom-1](entities/esp32-s3-wroom-1.md) — target MCU module
- [max485](entities/max485.md) — RS485 breakout module + chip facts, termination/level-shift concerns
