# Wiki Index

Catalog of all wiki pages. Maintained by the agent; see root `CLAUDE.md`.

## Sources
- [n420384a-english](sources/n420384a-english.md) — Daikin N420384A Modbus RTU manual (file 1; +AB-, 500 m / 50 units limits)
- [n420384a-modbus-rtu](sources/n420384a-modbus-rtu.md) — same manual, file 2 (+BA-, S1 grid switch; differences listed)
- [max485-datasheet](sources/max485-datasheet.md) — MAX485 family chip datasheet: 5 V supply, pinout, DE/RE, RO level
- [project-brief](sources/project-brief.md) — user brief: goal, hardware, dev-env rules

## Concepts
- [project-goal](concepts/project-goal.md) — Matter-over-Wi-Fi thermostat scope, deferred items
- [bus-topology](concepts/bus-topology.md) — planned third node on the Daikin bus, open questions
- [dev-environment](concepts/dev-environment.md) — ESP-IDF + component-registry-only rule, host state
- [modbus-rs485-link](concepts/modbus-rs485-link.md) — 9600 8N1, FC 03/06, cable/termination rules, polarity conflict
- [register-map](concepts/register-map.md) — full register table with ranges, defaults, quirks
- [flag-registers](concepts/flag-registers.md) — PRG, OUT, STAT, ALR_STAT bit meanings
- [web-restriction-flags](concepts/web-restriction-flags.md) — reg 247 supervisor lock/restriction flags, troubleshooting
- [device-addressing](concepts/device-addressing.md) — address 1–255, keypad procedure

## Entities
- [ekrtctrl1](entities/ekrtctrl1.md) — wall control, board connections
- [ekwhctrl1](entities/ekwhctrl1.md) — wall control with own Modbus port
- [ekwhctrl0](entities/ekwhctrl0.md) — kit installed with EKWHCTRL1
- [inn-fr-b32](entities/inn-fr-b32.md) — PCB carrying the Modbus terminal
- [esp32-s3-wroom-1](entities/esp32-s3-wroom-1.md) — target MCU module
- [max485](entities/max485.md) — RS485 transceiver: chip facts, level-shift concern, module gaps
