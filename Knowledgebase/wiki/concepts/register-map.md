---
title: Modbus register map
type: concept
sources: ["EKWHCTRL0.EKWHCTRL1.EKRTCTRL1_Installation manual_N420384A_English.pdf", "EKWHCTRL1,EKRTCTRL1,EKWHCTRL0_Installation manual_N420384A ModBus RTU_English.pdf"]
updated: 2026-11-08
---

# Register map

Source: [[sources/n420384a-english]] p4 (values identical in [[sources/n420384a-modbus-rtu]]; wording differences listed on that source page). Addresses are decimal. "mlt" = multiplier: raw × mlt = value (0.1 → tenths of unit). Limits must be respected. `*` = only on [[entities/inn-fr-b32]] board.

**Scope and star list (`datasheet-reader`, both files, pages rendered as images, 2026-11-08):** p4 intro: "addresses of the control parameters resident on the INN-FR-B32 and EKWHCTRL0 board and the EKRTCTRL1 and EKWHCTRL1 wall-mounted control panel" (file 2: "INN-FR-B32 e EKWHCTRL0"). p4 also: "The limit values must be respected!" and "take into account the multiplication factor mlt during reading". Starred "* Only for INN-FR-B32 board": 001, 009, 015, 104, 210, 211, 212, 213, 214, 215, 218, 219, 243, 244. Unstarred: 000, 008, 105, 200, 201, 202, 203, 209, 221, 222, 230, 231, 233, 234, 242, 245, 246, 247. The table below matches this list (checked 2026-11-08, no mismatch).
- 230 MVVP3 and 234 MVVP2 are printed **unstarred** while 210–215 (incl. MVVP1 215) are starred; looks inconsistent, recorded as printed.
- Not stated whether the panel proxies starred registers. *(Inferred:)* via EKWHCTRL1 "A A B B -" only unstarred registers are reliable; starred ones may be absent or return an exception; test in phase 1 ([[concepts/bus-topology]] item 4).

| Reg | Name | Range | Description | R/W | mlt | Default |
|---|---|---|---|---|---|---|
| 000 | T1 | 0–100.0 °C | Air temperature | R | 0.1 | – |
| 001* | T2 | 0–100.0 °C | Hot water temp H2 | R | 0.1 | – |
| 008 | SP | 0–100.0 °C | Real setpoint | R | 0.1 | – |
| 009* | OUT | – | Output relay status ([[concepts/flag-registers]]) | R | flag | – |
| 015* | MOT_SET | 0–1700 | Motor speed (set) | R | 1 | – |
| 104* | STAT | – | Status flags | R | flag | – |
| 105 | ALR_STAT | – | Alarm flags | R | flag | – |
| 200 | ADR | 1–255 | Device address | R/W | 1 | 1 |
| 201 | PRG | – | Mode/standby/lock flags | R/W | flag | 0 |
| 202 | SPL | 5.0–SPH °C | Min setpoint | R/W | 0.1 | 16.0 |
| 203 | SPH | SPL–35.0 °C | Max setpoint | R/W | 0.1 | 28.0 |
| 209 | E_SAVING | 0–8.5 K | Presence-contact offset / standby | R/W | 0.1 | 0 |
| 210* | MVV5 | 400–(MVV3-4) | Min speed in MIN and Night | R/W | 1 | 400 |
| 211* | MVV4 | MVV5–MVV2 | Max speed in Night; min in AUTO | R/W | 1 | 550 |
| 212* | MVV3 | MVV5–MVV1 | Max in MIN; min in MAX | R/W | 1 | 680 |
| 230 | MVVP3 | MVV5–MVVP1 | As MVV3 with Performance | R/W | 1 | 920 |
| 213* | MVV2 | MVV4–1500 | Max speed in AUTO | R/W | 1 | 1100 |
| 234 | MVVP2 | MVV4–1500 | As MVV2 with Performance | R/W | 1 | 1220 |
| 214* | MVV1 | MVV3–1500 | Max speed in MAX | R/W | 1 | 1500 |
| 215* | MVVP1 | MVV1–1700 | As MVV1 with Performance | R/W | 1 | 1700 |
| 218* | LLO | 0–100.0 °C | Min water temp for heating | R/W | 0.1 | 30.0 |
| 219* | LHI | 0–100.0 °C | Max water temp for cooling | R/W | 0.1 | 20.0 |
| 221 | ACL | 0–32000 h | Maintenance interval (0 = off) | R/W | 1 | 0 |
| 222 | ACL_TIM | 0–ACL | Fan running hours | R/W | 1 | – |
| 231 | SP | SPL–SHP / SPL_W–SPH_W | Absolute setpoint (5–40 °C) | R/W | 0.1 | 20.0 |
| 233 | Man | 0 / 3 (winter) / 5 (summer) | Seasonal auto/manual | R/W | 1 | 3 |
| 242 | OS1 | ±12.0 K | T1 air probe offset | R/W | 0.1 | 0 |
| 243* | OS2 | ±12.0 K | H2 probe offset | R/W | 0.1 | 0 |
| 244* | OS3 | ±12.0 K | H4 probe offset | R/W | 0.1 | 0 |
| 245 | SPL_W | 5.0–SPH_W °C | WEB min setpoint | R/W | 0.1 | 20.0 |
| 246 | SPH_W | SPL_W–40 °C | WEB max setpoint | R/W | 0.1 | 24.0 |
| 247 | WEB | – | Supervisor flags ([[concepts/web-restriction-flags]]) | R/W | flag | 0 |

## Notes / source quirks
- Writing the setpoint: p6 "To enter the setpoint, write the absolute value within the selected scale in register 231 SP."; 008 is the read-only real setpoint.
- PRG 201 bits 8–15: "System Flags reserved, do not change the status" (p5, [[concepts/flag-registers]]).
- Reg 218/219: file 2 descriptions omit "temperature" ("Minimum water for heating" / "Maximum water for cooling") ([[sources/n420384a-modbus-rtu]]).
- Reg 233 labels "inv"/"est" are Italian (inverno = winter, est**ate** = summer) *(inferred)*.
- Limits column shows "SHP" for 231 and "SPH" elsewhere; likely a typo for SPH *(inferred)*.
- MVV limit for 210 prints "MVV3-4" (unclear whether minus 4 or MVV3/MVV4).
- Registers 210–219 hold fan RPM-like values, unit not stated.
- Address and function codes: [[concepts/modbus-rs485-link]], [[concepts/device-addressing]].
