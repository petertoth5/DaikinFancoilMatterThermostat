# Wiki Index

Catalog of all wiki pages. Maintained by the agent; see root `CLAUDE.md`.

## Sources
- [n420384a-english](sources/n420384a-english.md) — Daikin N420384A Modbus RTU manual (file 1; +AB-, 500 m / 50 units limits)
- [n420384a-modbus-rtu](sources/n420384a-modbus-rtu.md) — same manual, file 2 (+BA-, S1 grid switch; differences listed)
- [max485-datasheet](sources/max485-datasheet.md) — MAX485 family chip datasheet: 5 V supply, pinout, DE/RE, RO level
- [max485-module-photo](sources/max485-module-photo.md) — user photo of the MAX485-M breakout: RO/RE/DE/DI header, U1 marking illegible; R7 "121" vs user: no on-board termination
- [project-brief](sources/project-brief.md) — user brief: goal, hardware, dev-env rules

## Concepts
- [project-goal](concepts/project-goal.md) — Matter-over-Wi-Fi thermostat scope, deferred items
- [bus-topology](concepts/bus-topology.md) — planned third node: passive monitor + Matter-triggered master, fancoil on "+ A B -"; open: Modbus on that link, master, tap point, termination, A/B mapping
- [dev-environment](concepts/dev-environment.md) — ESP-IDF + component-registry-only rule, host state
- [modbus-rs485-link](concepts/modbus-rs485-link.md) — 9600 8N1, FC 03/06, cable/termination rules, EKRTCTRL1 +AB-/+BA- legend conflict; EKWHCTRL1 "+ A B -" (no conflict)
- [register-map](concepts/register-map.md) — full register table with ranges, defaults, quirks
- [flag-registers](concepts/flag-registers.md) — PRG, OUT, STAT, ALR_STAT bit tables (visually verified), label conflicts
- [web-restriction-flags](concepts/web-restriction-flags.md) — reg 247 supervisor lock/restriction flags, troubleshooting
- [device-addressing](concepts/device-addressing.md) — address 1–255, keypad procedure

## Entities
- [ekrtctrl1](entities/ekrtctrl1.md) — wall control, board connections
- [ekwhctrl1](entities/ekwhctrl1.md) — wall control: "+ A B -" = fancoil serial link, "A A B B -" = Modbus RTU port (p3, visually verified)
- [ekwhctrl0](entities/ekwhctrl0.md) — kit installed with EKWHCTRL1
- [inn-fr-b32](entities/inn-fr-b32.md) — fancoil control PCB (N540022B in figures) with the "+AB-" Modbus terminal
- [esp32-s3-wroom-1](entities/esp32-s3-wroom-1.md) — target MCU module
- [max485](entities/max485.md) — RS485 breakout module + chip facts, termination/level-shift concerns
