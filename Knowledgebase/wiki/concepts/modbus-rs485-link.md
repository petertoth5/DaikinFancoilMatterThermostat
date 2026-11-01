---
title: Modbus RTU / RS485 link
type: concept
sources: ["EKWHCTRL0.EKWHCTRL1.EKRTCTRL1_Installation manual_N420384A_English.pdf", "EKWHCTRL1,EKRTCTRL1,EKWHCTRL0_Installation manual_N420384A ModBus RTU_English.pdf"]
updated: 2026-11-01
---

# Modbus RTU / RS485 link

**Protocol:** Modicon Modbus RTU, 9600 baud, 8 data bits, no parity, 1 stop bit (8N1). Function codes: **03** read single register, **06** write single register ([[sources/n420384a-english]] p3). Only these two are documented.

**Wiring (p3):** shielded bipolar RS485 cable, ≥0.35 mm²; keep separate from power cables; shortest leads; terminate the line with **120 Ω**; **no star topology**. Max 500 m total and 50 units per wall panel (stated only in [[sources/n420384a-english]]).

**Conflict – terminal polarity:** [[sources/n420384a-english]] labels EKRTCTRL1 terminals "+AB-"; [[sources/n420384a-modbus-rtu]] labels it "+BA-". The EKWHCTRL1 diagram in both says "+ A B -". Verify on hardware before wiring.

**Conflict/gap – 500 m/50 units:** missing from file 2; treat as valid but unconfirmed there.

Devices: [[entities/ekrtctrl1]], [[entities/ekwhctrl1]], [[entities/inn-fr-b32]]. Addressing: [[concepts/device-addressing]]. Registers: [[concepts/register-map]].
