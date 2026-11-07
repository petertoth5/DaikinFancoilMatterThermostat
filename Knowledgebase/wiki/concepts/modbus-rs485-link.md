---
title: Modbus RTU / RS485 link
type: concept
sources: ["EKWHCTRL0.EKWHCTRL1.EKRTCTRL1_Installation manual_N420384A_English.pdf", "EKWHCTRL1,EKRTCTRL1,EKWHCTRL0_Installation manual_N420384A ModBus RTU_English.pdf", "MAX485-M_EN_10035508.pdf"]
updated: 2026-11-07
---

# Modbus RTU / RS485 link

**Protocol:** Modicon Modbus RTU, 9600 baud, 8 data bits, no parity, 1 stop bit (8N1). Function codes: **03** read single register, **06** write single register ([[sources/n420384a-english]] p3). Only these two are documented.

**Wiring (p3):** shielded bipolar RS485 cable, ≥0.35 mm²; keep separate from power cables; shortest leads; terminate the line with **120 Ω**; **no star topology**. Max 500 m total and 50 units per wall panel (stated only in [[sources/n420384a-english]]). "Complete the line with the 120 Ω resistance" and "It is forbidden make "star" connections" are in both files (p3, visually verified).

**Conflict – EKRTCTRL1 terminal legend (unresolved source conflict):** in the EKRTCTRL1 legend row "ModBus RTU serial connection" (p2), [[sources/n420384a-english]] prints "+AB-" and [[sources/n420384a-modbus-rtu]] prints "+BA-". In both files the p2 PCB plug and wire labels print "+ A B -" (visually verified by `datasheet-reader`). EKRTCTRL1 is not installed here.
**EKWHCTRL1: no conflict.** Both files print "+ A B -" (and "A A B B -") for EKWHCTRL1 (p3, visually verified), and the user confirmed the "+ A B -" label on the hardware (session 2026-11-07, not in raw). Port roles: "+ A B -" = serial connection for fancoil, "A A B B -" = Modbus RTU ([[entities/ekwhctrl1]]). Whether the "+ A B -" fancoil link runs Modbus RTU 9600 8N1 is likely but unverified *(inferred)*, see [[concepts/bus-topology]].
**Open – A/B electrical mapping:** how Daikin "A"/"B" map to MAX485 A (non-inverting) / B (inverting) ([[sources/max485-datasheet]] p7) is still to verify by measurement or sniffing *(inferred)*.

**Conflict/gap – 500 m/50 units:** missing from file 2; treat as valid but unconfirmed there.

Devices: [[entities/ekrtctrl1]], [[entities/ekwhctrl1]], [[entities/inn-fr-b32]]. Addressing: [[concepts/device-addressing]]. Registers: [[concepts/register-map]].

New third node planned on this bus: [[concepts/bus-topology]].
