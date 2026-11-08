---
title: Modbus RTU / RS485 link
type: concept
sources: ["EKWHCTRL0.EKWHCTRL1.EKRTCTRL1_Installation manual_N420384A_English.pdf", "EKWHCTRL1,EKRTCTRL1,EKWHCTRL0_Installation manual_N420384A ModBus RTU_English.pdf", "MAX485-M_EN_10035508.pdf"]
updated: 2026-11-08
---

# Modbus RTU / RS485 link

**Protocol:** Modicon Modbus RTU, 9600 baud, 8 data bits, no parity, 1 stop bit (8N1). Function codes: **03** read single register, **06** write single register ([[sources/n420384a-english]] p3: baudrate=9600, data bits=8, parity=none, stop bit=1; functions "03" and "06" "for read and write of a single register"; file 2 prints "parità=nessuna"). Only these two are documented; nothing else is listed. Whether FC03 accepts quantity > 1 is not stated (phase-1 test). The manual does not name the port these settings apply to (visually re-checked by `datasheet-reader`, 2026-11-08).

**Wiring (p3):** shielded bipolar RS485 cable, ≥0.35 mm²; keep separate from power cables; shortest leads; terminate the line with **120 Ω**; **no star topology**. Max 500 m total and 50 units per wall panel (stated only in [[sources/n420384a-english]]). "Complete the line with the 120 Ω resistance" and "It is forbidden make "star" connections" are in both files (p3, visually verified).
**Ambiguity – which port these cabling rules apply to (2026-11-08, `datasheet-reader`, p3 at 600 dpi):** the manual does not say whether they cover the EKWHCTRL1 "+ A B -" fancoil chain, the "A A B B -" Modbus RTU port, or both. The only drawn 120 Ω and the "RS485" dashed chain are on the fancoil link (board 1 -> board 2), not on "A A B B -" ([[entities/ekwhctrl1]]). "50 units ... to each wall panel" reads like the fancoil chain *(inferred)*. So these rules are not confirmed for the node's "A A B B -" segment; termination there is a phase-1 check ([[concepts/bus-topology]] item 3).

**Conflict – EKRTCTRL1 terminal legend (unresolved source conflict):** in the EKRTCTRL1 legend row "ModBus RTU serial connection" (p2), [[sources/n420384a-english]] prints "+AB-" and [[sources/n420384a-modbus-rtu]] prints "+BA-". In both files the p2 PCB plug and wire labels print "+ A B -" (visually verified by `datasheet-reader`). EKRTCTRL1 is not installed here.
**EKWHCTRL1: no conflict.** Both files print "+ A B -" (and "A A B B -") for EKWHCTRL1 (p3, visually verified), and the user confirmed the "+ A B -" label on the hardware (session 2026-11-07, not in raw). Port roles: "+ A B -" = serial connection for fancoil, "A A B B -" = Modbus RTU ([[entities/ekwhctrl1]]). Whether the "+ A B -" fancoil link runs Modbus RTU 9600 8N1 is likely but unverified *(inferred)*, see [[concepts/bus-topology]].
**Open – A/B electrical mapping:** how Daikin "A"/"B" map to MAX485 A (non-inverting) / B (inverting) ([[sources/max485-datasheet]] p7) is still to verify by measurement or sniffing *(inferred)*.

**Conflict/gap – 500 m/50 units:** missing from file 2; treat as valid but unconfirmed there.

Devices: [[entities/ekrtctrl1]], [[entities/ekwhctrl1]], [[entities/inn-fr-b32]]. Addressing: [[concepts/device-addressing]]. Registers: [[concepts/register-map]].

*(Superseded 2026-11-08: "New third node planned on this bus".)* The new node connects as sole master to the EKWHCTRL1 "A A B B -" port, not to the "+ A B -" fancoil bus (user, 2026-11-08, not in raw): [[concepts/bus-topology]].
