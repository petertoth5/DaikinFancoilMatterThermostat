---
title: EKRTCTRL1 wall control
type: entity
sources: ["EKWHCTRL0.EKWHCTRL1.EKRTCTRL1_Installation manual_N420384A_English.pdf", "EKWHCTRL1,EKRTCTRL1,EKWHCTRL0_Installation manual_N420384A ModBus RTU_English.pdf"]
updated: 2026-11-01
---

# EKRTCTRL1

Wall-mounted fancoil control. Connects to Modbus RTU via the ModBus connector on its [[entities/inn-fr-b32]] board.
Board connections (legend, p2): ModBus serial, H2 hot-water probe (10 kΩ), optional AIR probe, M1 DC-inverter fan motor, CP presence input, Y1/N-EV1 water valve (230 V/50 Hz, 1 A), L-N 230 V/50 Hz supply, BO boiler contact and CH chiller contact (free contacts, max 1 A). H4 probe terminal also shown on diagram.
Terminal polarity label conflicts between sources: see [[concepts/modbus-rs485-link]].
Address can be set from keypad: [[concepts/device-addressing]].
