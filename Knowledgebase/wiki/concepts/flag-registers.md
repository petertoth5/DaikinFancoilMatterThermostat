---
title: Status and configuration flag registers
type: concept
sources: ["EKWHCTRL0.EKWHCTRL1.EKRTCTRL1_Installation manual_N420384A_English.pdf", "EKWHCTRL1,EKRTCTRL1,EKWHCTRL0_Installation manual_N420384A ModBus RTU_English.pdf"]
updated: 2026-11-08
---

# Flag registers (p4 table, p5 bits)

Bit tables and the p4 register rows were **visually verified** by `datasheet-reader` (PDF pages read as images) in [[sources/n420384a-english]] and [[sources/n420384a-modbus-rtu]]; p5 is identical in both files.

## Register rows (p4)
| Reg | Name | R/W | Default |
|---|---|---|---|
| 009* | OUT | R | – |
| 104* | STAT | R | – |
| 105 | ALR_STAT | R | – |
| 201 | PRG | R/W | 0 |

Footnote: "* Only for INN-FR-B32 board" → OUT and STAT exist only on [[entities/inn-fr-b32]] (p4). **Open:** are they readable in other setups / in this installation? Relevant because the new node wants to follow status ([[concepts/bus-topology]]).

## PRG – reg 201 (R/W) (p5)
Bits 0–2 PRG mode: 000 Automatic (source typo "Sutomatic"), 001 Silent, 010 Night, 011 Maximum; values 100–111 undefined. Bit 4 LOCK: 1 = keyboard disabled locally. Bit 7 Stby: 1 = standby. Bits 3, 5, 6 shown as "-" (undocumented). Bits 8–15: "System Flags reserved, do not change the status".

## OUT – reg 009 (R) (p5)
Only bits 7–0 are shown: bit 0 EV1, bit 1 EV2, bit 2 CHILLER, bit 3 BOILER (relay outputs); no meaning reported for bits 4–7. Bits 8–15 are not shown.

## STAT – reg 104 (R) (p5, bit positions visually verified)
| Bit | Label | Meaning (legend) |
|---|---|---|
| 15 | "0" | – |
| 14 | H4 asnt | H4 absent at start |
| 13 | H2 asnt | H2 absent at start |
| 12 | Com err. | "Timeout master (vers. Centralised B32) of 300sec" |
| 11 | stby | standby |
| 10 | test | test mode |
| 9 | alrm | alarm present |
| 8 | Antig | antifreeze |
| 7 | "0" | – |
| 6 | B.A. | stop for unsuitable water, standby 45 min |
| 5 | F.V. H4 | fan stop, water unsuitable on H4 |
| 4 | F.V. H2 | fan stop, water unsuitable on H2 |
| 3 | (empty) | – |
| 2 | (empty) | – |
| 1 | Mod. Risc. | active mode: heating *(Italian reading, inferred)* |
| 0 | Mod. Raff. | active mode: cooling *(Italian reading, inferred)* |

Label nuance: the table cells read "Mod. Risc." / "Mod. Raff."; the legend reads "Mod. Risc./Mod. Rafr: Active mode".
**300 s timeout (re-assessed 2026-11-08, `datasheet-reader`, pages rendered as images):** STAT (104, starred = INN-FR-B32 only) bit 12 "Com err: Timeout master (vers. Centralised B32) of 300sec"; related ALR_STAT (105) bit 0 "Com.: 'Blind' version communication error". The manual does not state which link this concerns, any polling obligation, or the timeout behaviour. *(Inferred, low-medium:)* it concerns a panel-less INN-FR-B32 driven by a central master, and is **not evidence about the EKWHCTRL1 "A A B B -" port** the node uses. *(Superseded: the 2026-11-07 wiki line "Bus-design relevance: bit 12 flags a 300 s master timeout" presented it as relevant for the node.)* Panel fail-safe when the master stops polling stays a phase-1 question ([[concepts/bus-topology]] item 2).

## ALR_STAT – reg 105 (R) (p5)
| Bit | Table label | Legend / note |
|---|---|---|
| 15–12 | "0" | – |
| 11 | 2 AIR M5 | legend: "2 AIR M4 (only for M4 control panel)" — **conflict** |
| 10 | Filter | legend: "Filtro" (language only); maintenance |
| 9 | H2 n. id | H2 not suitable |
| 8 | SW GRL | legend: "GRID: GRID contact open" — **conflict** |
| 7 | Mot | motor fault |
| 6 | Hi Res | high temperature resistance |
| 5 | H4 n. id | H4 not suitable |
| 4 | H2 | H2 probe |
| 3 | Acq. Dan. | harmful water block |
| 2 | H4 | H4 probe |
| 1 | AIR | AIR probe |
| 0 | Com | "Com.: 'Blind' version communication error" (see 300 s note above) |

**Conflicts table vs legend (recorded, not resolved):** bit 8 "SW GRL" vs "GRID"; bit 11 "2 AIR M5" vs "2 AIR M4"; bit 10 Filter/Filtro is a language difference only. Legend for bit 11 (visually verified by `datasheet-reader`, p5): "2 AIR M4 (only for M4 control panel): Communication error with fancoil with remote air probe or presence of more than one board with air probe ('E2' displayed)".

## Open
- What reserved / "-" / "0" bits read back as (not documented).
- OUT/STAT availability outside INN-FR-B32 (see above); via EKWHCTRL1 "A A B B -": phase-1 test ([[concepts/bus-topology]] item 4).

See [[concepts/register-map]]; sources [[sources/n420384a-english]], [[sources/n420384a-modbus-rtu]].
