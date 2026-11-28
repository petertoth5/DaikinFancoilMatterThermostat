---
title: Status and configuration flag registers
type: concept
sources: ["EKWHCTRL0.EKWHCTRL1.EKRTCTRL1_Installation manual_N420384A_English.pdf", "EKWHCTRL1,EKRTCTRL1,EKWHCTRL0_Installation manual_N420384A ModBus RTU_English.pdf"]
updated: 2026-11-28
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

Footnote: "* Only for INN-FR-B32 board" → OUT and STAT exist only on [[entities/inn-fr-b32]] (p4). **Open:** are they readable in other setups / in this installation? *(2026-11-28, this installation via the panel: STAT yes, OUT exception 02; see "Observed values".)* Relevant because the new node wants to follow status ([[concepts/bus-topology]]).

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

## Observed values (on site 2026-11-28, D.2 / D.3)
Source: **session observation**, not in raw (user's console output, `mb regs`, via EKWHCTRL1 "A A B B -", FC03 qty 1, address 1; [[concepts/register-map]] "First live values", [[concepts/firmware-architecture]] section 7). The decoding below is **derived**: it applies the p5 bit tables above with bit 0 = LSB (the p5 tables number bits 0–15) to the raw value; it is not checked against the panel display, except the standby bits (see "Standby transition observed" below).

| Reg | Raw | Bits set | Decoding (derived, from the tables above) |
|---|---|---|---|
| 009* OUT | exception 02 | – | not readable via the panel |
| 104* STAT | `8802` = `1000100000000010` | 15, 11, 1 | bit 11 "stby" (standby); bit 1 "Mod. Risc." (active mode heating, Italian reading *inferred*); **bit 15 printed "0"** |
| 105 ALR_STAT | `0000` | none | no alarm bit set (incl. bit 0 "Com") |
| 201 PRG | `0083` = `0000000010000011` | 7, 1, 0 | bits 2–0 = `011` = "Maximum" (assuming the printed "011" is written bit 2..bit 0, *inferred*); bit 4 LOCK = 0; bit 7 Stby = 1 (standby); bits 3, 5, 6 = 0; bits 8–15 = 0 |

- **Conflict (manual vs observation, not resolved):** STAT bit 15 is printed as a fixed "0" (p5), but reads **1** in `8802`. Possible explanations (bit numbering, an undocumented flag) are not decided.
- 104 bit 11 "stby" and 201 bit 7 "Stby" are both 1, i.e. both say standby (derived, consistent with each other). ~~Whether the panel was actually in standby during D.3 is an open question for the user.~~ Answered: yes, see "Standby transition" below.

### Standby transition observed (on site 2026-11-28, D.6 / D.7)
Source: session observation, not in raw (user report relayed by the orchestrator; poll lines in `docs/phase1-probe/site-2026-11-28-d.log`, project file, to go to raw after the trip; the lines below were spot-checked by the wiki agent, local inspection of that file).
- During D.3 the panel was in **standby**: the user left it there after checklist C.5 (address read needs standby). At the start of D.7 the user reports the panel **ON**, fan shown as "two fan symbols = high fan speed" (user's reading of the display), setpoint 22. The panel state before C.5 is unknown to the user.
- Poll lines: 104 `8802` at t = 450532 ms, `8002` at 455532 ms; 201 `0083` at 450752 ms, `0003` at 455762 ms. So 104 bit 11 and 201 bit 7 cleared together, between ≈ 450.5 s and ≈ 455.5 s, and stayed cleared (`8002` / `0003` at 460762 / 460982 ms). The exact time of the user's keypress was not timed (orchestrator asked the user).
- **Confirmed by observation (one transition, standby -> on):** 201 bit 7 "Stby" = 1 means standby, 104 bit 11 "stby" = 1 means standby, with bit 0 = LSB numbering. This matches the p5 tables and supports bit 0 = LSB for these two registers. ~~The reverse transition (on -> standby) is still to be seen in D.7.~~ Seen, see the next bullet.
- **Both directions confirmed (D.7, 2026-11-28):** on -> standby `T 1336882 130 1 MANUAL 201 1 OK 0083` (bit 7 set); standby -> on `T 1354200 122 1 MANUAL 201 1 OK 0003` (bit 7 cleared). POLL lines (local inspection of the capture by the wiki agent): 201 `0003` -> `0083` and 104 `8002` -> `8802` together between 1320.5 s and 1325.5 s; back to `0003` / `8002` at 1355.5 s. **201 bit 7 = standby confirmed in both directions; 104 bit 11 also follows in both directions** in the capture. Note: the orchestrator's relay said 104 bit 11 was seen in the first transition only; the capture POLL 104 lines show it in this second pair too (recorded; capture taken as the measured value). Bits 0–2 stayed `011` (high) through standby.
- **201 bits 0–2 = `011` (3)** both in standby and after switching on, while the panel shows "high fan speed" (user): consistent with p5 "011 Maximum" — ~~one data point only; the full order of the fan modes comes from the D.7 fan-mode steps.~~ full mapping now observed, see "Fan mode observed" below. Whether the p5 "PRG mode" names (Automatic / Silent / Night / Maximum) are the fan speeds shown on the display is *(inferred)*, not stated in p5.
- Unchanged across the transition: 104 bit 15 = 1 (the printed-"0" conflict above stays) and 104 bit 1 = 1.
- 104 bit 12 "Com err" = 0 and 105 bit 0 "Com" = 0 while polled (observed; relevance to this port unchanged, see 300 s note above).
- Reserved / "-" bits observed: 201 bits 3, 5, 6, 8–15 = 0; 104 bits 7 ("0"), 3, 2 (empty) = 0; 105 bits 15–12 ("0") = 0.

### Fan mode observed, PRG bits 0–2 (on site 2026-11-28, D.7)
Source: session observation, not in raw (user's panel reading and `mb read 201 1` after each change, relayed by the orchestrator; each change confirmed in the POLL 201 lines of `docs/phase1-probe/site-2026-11-28-d.log`, project file, to go to raw after the trip; times are the orchestrator's reading of that capture).

| 201 raw | Bits 2–0 | Panel display (user) | How reached | When (capture) |
|---|---|---|---|---|
| `0x0003` | `011` | "high", two fan icons | from boot; again via fan button | from boot; again ≈ 930 s |
| `0x0001` | `001` | "low", one fan icon | fan button | ≈ 830 s, MANUAL read at 842387 ms |
| `0x0000` | `000` | "auto" | fan button | ≈ 885 s, MANUAL read at 898340 ms |
| `0x0002` | `010` | **night mode** | a **different control** than the fan button (not specified by the user) | MANUAL read at 988834 ms |

- **Observed mapping on this panel:** 0 = auto, 1 = low, 2 = night, 3 = high (bit 0 = LSB; bits 3–15 unchanged at 0 in these reads, bit 7 = 0 = on).
- **Fan button cycle:** high -> low -> auto -> high only (user: "no other option"). Night (2) is **not** in the fan-button cycle.
- **Comparison with p5** (000 Automatic, 001 Silent, 010 Night, 011 Maximum): bit values for auto (0) and night (2) match the printed names; 1 and 3 match **if** "Silent" = displayed "low" and "Maximum" = displayed "high" *(inferred naming match, not stated in p5)*. Recorded, not resolved further.
- Not observed: values 100–111 (p5 undefined); whether a write of 2 (night) behaves like the panel's night control (no writes on this trip).
- **Use in the Matter mapping (user decision 2026-11-28, PR3, not in raw):** Low = 001, High = 011, Auto = 000; night (010) is read as Low with a console flag; codes 4–7 keep the last value. Details: [[concepts/firmware-architecture]] section 5 EP2 / section 6 "Combined 2b+3 plan".
- **Restore night -> high (D.7):** `T 1092210 122 1 MANUAL 201 1 OK 0003`. POLL 201 (local inspection): `0002` -> `0000` (auto) at ≈ 1040.8 s, then `0003` at ≈ 1085.8 s. Which control set and cleared night: still not stated by the user.

### Season and STAT bit 1 (on site 2026-11-28, D.7)
Source: session observation, not in raw (POLL 104 / 233 lines in `docs/phase1-probe/site-2026-11-28-d.log`, local inspection by the wiki agent).
- With 233 = 5 (cooling, [[concepts/register-map]] "Local keypress correlation") 104 read `8000` (bit 1 = 0) from 1155512 ms to 1215532 ms; with 233 = 3 (heating) it reads `8002` (bit 1 = 1), also in standby (`8802`). p5 bit 1 "Mod. Risc." = heating mode *(Italian reading, inferred)* is **consistent with this observation** (one heating -> cooling -> heating cycle).
- The first POLL 233 = `0005` (1151192 ms) precedes the first 104 = `8000` (1155512 ms), and 104 = `8002` (1220522 ms) precedes 233 = `0003` (1221212 ms); both within one 5 s poll cycle, so no order between the two registers can be derived *(inferred)*.

## Open
- ~~Fan-mode order of 201 bits 0–2 (only 3 = high seen so far)~~ **Answered 2026-11-28** (see "Fan mode observed"). Still open: ~~the on -> standby direction~~ (seen 2026-11-28, both directions); which panel control sets / clears night mode (not specified by the user).
- What reserved / "-" / "0" bits read back as (not documented). Partly observed 2026-11-28 (above); 104 bit 15 = 1 conflicts with the printed "0".
- OUT/STAT availability outside INN-FR-B32 (see above); via EKWHCTRL1 "A A B B -": phase-1 test ([[concepts/bus-topology]] item 4). **Answered 2026-11-28:** STAT 104 readable, OUT 009 exception 02.

See [[concepts/register-map]]; sources [[sources/n420384a-english]], [[sources/n420384a-modbus-rtu]].
