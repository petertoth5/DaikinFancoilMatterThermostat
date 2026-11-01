---
title: Status and configuration flag registers
type: concept
sources: ["EKWHCTRL0.EKWHCTRL1.EKRTCTRL1_Installation manual_N420384A_English.pdf", "EKWHCTRL1,EKRTCTRL1,EKWHCTRL0_Installation manual_N420384A ModBus RTU_English.pdf"]
updated: 2026-11-01
---

# Flag registers (p5)

## PRG – reg 201 (R/W)
Bits 0–2 PRG mode: 000 Automatic, 001 Silent, 010 Night, 011 Maximum. Bit 4 LOCK: 1 = keyboard disabled locally. Bit 7 Stby: 1 = standby. Bits 8–15 reserved, do not change.

## OUT – reg 009 (R)
Bit 0 EV1, bit 1 EV2, bit 2 CHILLER, bit 3 BOILER (relay outputs).

## STAT – reg 104 (R)
Bit 0 cooling mode active, bit 1 heating mode active (label "Mod. Raff./Risc."; order per layout *(inferred)*). Bit 4 F.V. H2 (fan stop, water unsuitable on H2), bit 5 F.V. H4, bit 6 B.A. (stop for unsuitable water, standby 45 min), bit 8 antifreeze, bit 9 alarm present, bit 10 test mode, bit 11 standby, bit 12 Com err (master timeout 300 s), bit 13 H2 absent at start, bit 14 H4 absent at start.
Bit positions are read from the PDF diagram text layout and should be verified against the original page.

## ALR_STAT – reg 105 (R)
Com (blind-version comms error), AIR probe, H4 probe, Acq. Dan. (harmful water block), H2 probe, H4 not suitable, Hi Res (high temperature resistance), Mot (motor fault), GRID contact open, H2 not suitable, Filter (maintenance), "2 AIR M4/M5" (comms error with remote-probe fancoil or more than one board with air probe; E2 shown).

See [[concepts/register-map]]; source [[sources/n420384a-english]].
