---
title: N420384A Modbus RTU installation manual (file 2)
type: source
sources: ["EKWHCTRL1,EKRTCTRL1,EKWHCTRL0_Installation manual_N420384A ModBus RTU_English.pdf"]
updated: 2026-11-08
---

# N420384A – same manual, second file

7 pages, same title and structure as [[sources/n420384a-english]]. Register map, flags, addressing and troubleshooting: values identical; wording differences listed below.

## Differences from [[sources/n420384a-english]]
- EKRTCTRL1 terminal legend reads **+BA-** instead of +AB- (p2) → see [[concepts/modbus-rs485-link]].
- EKRTCTRL1 legend only: the +BA- difference is in the row "ModBus RTU serial connection"; the p2 PCB plug and wire labels print "+ A B -" in both files; EKWHCTRL1 blocks (p3) are identical in both files (visually verified by `datasheet-reader`, 2026-11-07).
- Adds legend entry **S1 – Grid safety microswitch** (p2); BO/CH called "consent output" (file 1: "contact output") (p2).
- Does **not** state the 500 m / 50 units limits (p3): both sentences are omitted (re-checked 2026-11-08).
- Reg 218/219 descriptions shortened ("Minimum water for heating" / "Maximum water for cooling", missing "temperature").
- p6 FLAG 7 key wording "SET+ e SET-" (Italian "e" = and, inferred) (`datasheet-reader`, 2026-11-08) → [[concepts/web-restriction-flags]].
- Same as file 1 (not a difference): FLAG 1 printed "(247.0)", p3 "A A B B -" block layout and serial settings (2026-11-08).
- p4 intro reads "INN-FR-B32 **e** EKWHCTRL0 board" (file 1: "and") (pdftotext check, 2026-11-07).
- Contains leftover Italian text: p2 bullet "collegarsi al connettore ModBus con serigrafia presente sulla scheda"; p3 "parità=nessuna" (file 1: "parity=none"); "stop bit" (visually verified 2026-11-07).

Flag bits on p5 visually verified identical in both files (2026-11-07) → [[concepts/flag-registers]].
