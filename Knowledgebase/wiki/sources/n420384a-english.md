---
title: N420384A Modbus RTU installation manual (file 1)
type: source
sources: ["EKWHCTRL0.EKWHCTRL1.EKRTCTRL1_Installation manual_N420384A_English.pdf"]
updated: 2026-11-07
---

# N420384A – Installation, connection and settings, wall control EKWHCTRL1 / EKRTCTRL1 for Modbus RTU

7-page Daikin manual (doc code N420384A) on connecting the wall controls to a Modicon Modbus RTU network and the register map exposed over it.

## Contents (page → topic)
- p1 Description, safety warnings → [[entities/inn-fr-b32]], [[entities/ekwhctrl0]]
- p2 Board access, EKRTCTRL1 wiring → [[entities/ekrtctrl1]]
- p3 EKWHCTRL1 + EKWHCTRL0 wiring, cable rules, serial settings → [[concepts/modbus-rs485-link]], [[entities/ekwhctrl1]]
- p4 Register table → [[concepts/register-map]]
- p5 PRG/OUT/STAT/ALR_STAT bit flags → [[concepts/flag-registers]] (visually verified 2026-11-07 by `datasheet-reader`; identical in file 2; ALR_STAT table/legend label conflicts noted there)
- p6 WEB flags (reg 247), address setting → [[concepts/web-restriction-flags]], [[concepts/device-addressing]]
- p7 Troubleshooting → [[concepts/web-restriction-flags]]

## Distinguishing facts (vs. [[sources/n420384a-modbus-rtu]])
- EKRTCTRL1 legend row "ModBus RTU serial connection" reads **+AB-** (p2); PCB plug/wire labels "+ A B -" in both files (visually verified by `datasheet-reader`, 2026-11-07).
- States cable limits: max **500 m** total, max **50 units** per wall panel (p3).
- EKRTCTRL1 legend has no S1 entry; BO/CH described as "contact output" (p2).
- p3 serial settings in English ("parity=none"); file 2 has "parità=nessuna".

## p1–p3 figures (visually verified by `datasheet-reader`, PDF pages rendered as images, 2026-11-07)
- p1: INN-FR-B32 "+AB-" terminal board connects the control panels to a Modbus RTU network; the p1 board figure matches the N540022B PCB of p2/p3 (inferred) → [[entities/inn-fr-b32]] (fancoil control PCB).
- p3: EKWHCTRL1 "+ A B -" = B "Serial connection for fancoil"; "A A B B -" = A "ModBus RTU Serial Connection"; lower diagram daisy-chains PCBs on "RS485" ending in 120 Ω → [[entities/ekwhctrl1]].
