---
title: Modbus register map
type: concept
sources: ["EKWHCTRL0.EKWHCTRL1.EKRTCTRL1_Installation manual_N420384A_English.pdf", "EKWHCTRL1,EKRTCTRL1,EKWHCTRL0_Installation manual_N420384A ModBus RTU_English.pdf"]
updated: 2026-11-28
---

# Register map

Source: [[sources/n420384a-english]] p4 (values identical in [[sources/n420384a-modbus-rtu]]; wording differences listed on that source page). Addresses are decimal. "mlt" = multiplier: raw × mlt = value (0.1 → tenths of unit). Limits must be respected. `*` = only on [[entities/inn-fr-b32]] board.

**Scope and star list (`datasheet-reader`, both files, pages rendered as images, 2026-11-08; file 1 pp. 4–5 re-checked on the image 2026-11-27, file 2 p4 text layer only):** p4 intro: "addresses of the control parameters resident on the INN-FR-B32 and EKWHCTRL0 board and the EKRTCTRL1 and EKWHCTRL1 wall-mounted control panel" (file 2: "INN-FR-B32 e EKWHCTRL0"). p4 also: "The limit values must be respected!" and "take into account the multiplication factor mlt during reading". Starred "* Only for INN-FR-B32 board": 001, 009, 015, 104, 210, 211, 212, 213, 214, 215, 218, 219, 243, 244. Unstarred: 000, 008, 105, 200, 201, 202, 203, 209, 221, 222, 230, 231, 233, 234, 242, 245, 246, 247. The table below matches this list (checked 2026-11-08, no mismatch).
- 230 MVVP3 and 234 MVVP2 are printed **unstarred** while 210–215 (incl. MVVP1 215) are starred; looks inconsistent, recorded as printed.
- Not stated whether the panel proxies starred registers. *(Inferred:)* via EKWHCTRL1 "A A B B -" only unstarred registers are reliable; starred ones may be absent or return an exception; test in phase 1 ([[concepts/bus-topology]] item 4). **Tested 2026-11-28 (D.3, session observation):** partly contradicted, star does not predict availability; see "First live values" below.

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
| 201 | PRG | – | "Flag configuration" (mode/standby/lock) | R/W | flag | 0 |
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
| 231 | SP | SPL–SHP / SPL_W–SPH_W | Absolute setpoint value (Note 2) | R/W | 0.1 | 20.0 |
| 233 | Man | printed "0/3 (inv)/5 (est)" | "Seasonal auto/manual" | R/W | 1 | 3 |
| 242 | OS1 | ±12.0 K | T1 air probe offset | R/W | 0.1 | 0 |
| 243* | OS2 | ±12.0 K | H2 probe offset | R/W | 0.1 | 0 |
| 244* | OS3 | ±12.0 K | H4 probe offset | R/W | 0.1 | 0 |
| 245 | SPL_W | 5.0–SPH_W °C | WEB min setpoint | R/W | 0.1 | 20.0 |
| 246 | SPH_W | SPL_W–40 °C | WEB max setpoint | R/W | 0.1 | 24.0 |
| 247 | WEB | – | Supervisor flags ([[concepts/web-restriction-flags]]) | R/W | flag | 0 |

## First live values (on site 2026-11-28)
Source: **session observation**, not in raw (user's console output on site, D.2 poll lines and D.3 `mb probe` / `mb regs` in `docs/phase1-probe/site-2026-11-28.log` / `site-2026-11-28-d.log`, to go to raw after the trip; relayed by the orchestrator). Read via [[entities/ekwhctrl1]] "A A B B -", crossed wiring, FC03 qty 1, address 1 ([[concepts/firmware-architecture]] section 7). Raw = register content as polled (hex, unscaled). "Scaled" = as the firmware scaled it (raw × mlt from the table above), **derived** (not checked against the panel display).

**D.3 sweep result (2026-11-28):** every documented register (32: 14 starred, 18 unstarred) answered, either OK or **exception 02** (illegal data address); **no TIMEOUT, no BADFRAME**. `dur_ms` 120–130. Answers exit item 4 ([[concepts/firmware-architecture]] section 1, [[concepts/bus-topology]] item 4). Registers outside the documented list (e.g. 208) were not swept.

| Reg | Raw | Scaled (derived) | Printed default | Note |
|---|---|---|---|---|
| 000 T1 | `00b3` (D.2), `00b4` | 17.9, 18.0 °C | – | |
| 008 SP | `00dc` | 22.0 °C | – | |
| 104* STAT | `8802` (bits `1000100000000010`) | flag | – | starred, answered; bits: [[concepts/flag-registers]] |
| 105 ALR_STAT | `0000` | flag | – | no bit set |
| 200 ADR | `0001` | 1 | 1 | matches C.5 address 01 ([[concepts/device-addressing]]) |
| 201 PRG | `0083` (bits `0000000010000011`) | flag | 0 | bits: [[concepts/flag-registers]] |
| 202 SPL | `00a0` | 16.0 °C | 16.0 | |
| 203 SPH | `0118` | 28.0 °C | 28.0 | |
| 209 E_SAVING | – | 0 | 0 | |
| 210* MVV5 | – | 400 | 400 | starred, answered |
| 211* MVV4 | – | 550 | 550 | starred, answered |
| 212* MVV3 | – | 680 | 680 | starred, answered |
| 213* MVV2 | – | 1100 | 1100 | starred, answered |
| 214* MVV1 | – | 1500 | 1500 | starred, answered |
| 215* MVVP1 | – | 1700 | 1700 | starred, answered |
| 218* LLO | `012c` | 30.0 °C | 30.0 | starred, answered |
| 219* LHI | `00c8` | 20.0 °C | 20.0 | starred, answered |
| 221 ACL | – | 0 | 0 | |
| 222 ACL_TIM | – | 0 | – | |
| 231 SP | `00dc` | 22.0 °C | 20.0 | same raw value as 008 |
| 233 Man | `0003` | 3 | 3 | the printed "3 (inv)" value |
| 242 OS1 | `ffec` | −2.0 K | 0 | firmware treats it as signed 16-bit; p4 gives only "±12.0 K", the encoding is not stated in the table above |
| 245 SPL_W | `00c8` | 20.0 °C | 20.0 | |
| 246 SPH_W | `00f0` | 24.0 °C | 24.0 | |
| 247 WEB | `0000` | flag | 0 | all flags 0 ([[concepts/web-restriction-flags]]) |

"–" in the Raw column: the relayed summary gave only the scaled value.

**Exception 02 (illegal data address):** 001* T2, 009* OUT, 015* MOT_SET, **230 MVVP3**, **234 MVVP2**, 243* OS2, 244* OS3.

Observations (recorded, not interpreted):
- **The star does not predict availability via the panel.** Starred 104, 210–215, 218, 219 answer with data; starred 001, 009, 015, 243, 244 raise exception 02. The earlier *(inferred)* line above ("only unstarred registers are reliable; starred ones may be absent") is **partly contradicted** by this observation.
- **Conflict (manual star marking vs observation, not resolved):** 230 MVVP3 and 234 MVVP2 are printed **unstarred** (i.e. not marked INN-FR-B32-only, p4) but raise exception 02 via "A A B B -", while their starred siblings 210–215 answer. This adds to the 230/234 star inconsistency recorded above.
- 218 LLO 30.0 > 219 LHI 20.0 as scaled; both equal the printed defaults. Recorded as observed.
- Values equal to the printed default: 200, 202, 203, 209, 210–215, 218, 219, 221, 233, 245, 246, 247. Differ from the default: 201, 231 (22.0 vs 20.0), 242 (−2.0 vs 0). Comparison arithmetic only.
- Bit decoding of 104 / 201: [[concepts/flag-registers]] section "Observed values" (derived).

### Block reads (D.5, on site 2026-11-28)
Source: session observation, not in raw (user's console output, `docs/phase1-probe/site-2026-11-28-d.log`, to go to raw after the trip; relayed by the orchestrator). Crossed wiring, address 1. Answers exit item 5 ([[concepts/firmware-architecture]] section 1, [[concepts/bus-topology]] item 1b).

| Command | Result | Note |
|---|---|---|
| `mb read 200 4` | OK `0001 0083 00a0 0118` | = 200 ADR 1, 201 PRG `0x0083`, 202 SPL `0x00A0`, 203 SPH `0x0118`, identical to the D.3 single reads; `dur_ms` 127 (qty-1 lines 120–130) |
| `mb read 000 2` | exception 02 | range includes 001, which alone gives exception 02 |
| `mb read 242 4` | exception 02 | range includes 243 / 244, which alone give exception 02 |

Observed: FC03 qty > 1 works for a fully supported contiguous range; a range containing **any** unsupported register is answered with exception 02 for the **whole** request (no partial data). That this is the usual Modbus slave behaviour is external knowledge (Modbus specification, not in raw), not checked against a source. Consequence for mapping *(inferred)*: block reads are possible only over contiguous runs of registers that each answer OK (e.g. 200–203, 210–215, 218–219, 245–247 per the D.3 table); runs containing 001, 009, 015, 230, 234, 243 or 244 must be split. Only 200 qty 4 was tested OK; other runs and larger quantities are untested.

### Local keypress correlation (D.7, on site 2026-11-28, complete)
Source: session observation, not in raw (user's `mb regs` output and panel reading, relayed by the orchestrator; capture `docs/phase1-probe/site-2026-11-28-d.log`, to go to raw after the trip). Start state (user): panel ON, high fan, setpoint 22 (= 008 / 231 `0x00DC` = 22.0 since D.2).
- **Setpoint up, once, on the panel:** panel shows **22.5**; the next `mb regs` shows **008 SP `0x00E1` = 22.5** and **231 SP `0x00E1` = 22.5**. Both registers follow the local keypress (observed); step 0.5 °C per press (one data point). ~~Setpoint down: not yet done.~~
- **Setpoint down, once, on the panel:** panel 22.5 -> **22.0**; `mb regs` shows **008 SP `0x00DC` = 22.0** and **231 SP `0x00DC` = 22.0**. With the up press: 008 and 231 both follow local setpoint changes in either direction, 0.5 °C per press (raw tenths, ± 5 counts) (observed, two presses).
- 000 T1 drifted `0x00B4` -> `0x00B7` (18.0 -> 18.3 °C) over the same period; not a keypress effect *(inferred)*.
- Standby -> on (201 bit 7, 104 bit 11) and fan modes (201 bits 0–2: 0 auto, 1 low, 2 night, 3 high): [[concepts/flag-registers]] "Standby transition observed" and "Fan mode observed".
- 233 read `0x0003` at t ≈ 941 s (unchanged since D.2; no season change made yet). ~~Season change and on -> standby: pending.~~ Done, see below.
- **Season (233), panel control (D.7):** panel in heating -> `T 1142896 126 1 MANUAL 233 1 OK 0003`; user switched to **cooling** -> `T 1182231 131 1 MANUAL 233 1 OK 0005`; switched back to **heating** -> `T 1293158 124 1 MANUAL 233 1 OK 0003`. **Observed on this panel: 3 = heating, 5 = cooling**, consistent with the printed "0/3 (inv)/5 (est)" under the Italian reading inv = inverno (winter), est = estate (summer) *(reading inferred)*. 233 follows the local season change (observed). **Meaning of 0 still not observed** (never seen). No anomaly reported by the user.
  - Duration in cooling: user estimate ≈ 1.5–2 min; the POLL 233 lines (local inspection of the capture by the wiki agent) show `0005` from 1151192 ms to 1216222 ms and `0003` again at 1221212 ms, i.e. ≈ 65–75 s. **Discrepancy (user estimate vs capture timestamps), not resolved**; the capture timestamps are the measured value.
  - Side effect seen in 104 (local inspection of the capture): 104 `8002` -> `8000` (bit 1 "Mod. Risc." cleared) while 233 = 5, back to `8002` at 1220522 ms. See [[concepts/flag-registers]] "Season and STAT bit 1".
- **Fan restored night -> high:** `T 1092210 122 1 MANUAL 201 1 OK 0003` (POLL 201 lines: `0002` -> `0000` at ≈ 1040.8 s, `0003` at ≈ 1085.8 s, local inspection). Which control set / cleared night: still not stated by the user.
- **Standby on/off:** both directions seen in 201 bit 7 (and 104 bit 11): [[concepts/flag-registers]] "Standby transition observed".
- **End state of D.7 (user):** ON, heating (233 = 3), fan high (201 = 3), setpoint 22.0 = the D.7 start state.

- Writing the setpoint: p6 "To enter the setpoint, write the absolute value within the selected scale in register 231 SP."; 008 is the read-only real setpoint.
- PRG 201 legend (p5, image-verified 2026-11-27): "PRG: 000=Sutomatic; 001=Silent; 010=Night; 011=Maximum" (printed typo "Sutomatic"), LOCK, Stby; bits 3, 5, 6 printed "-" with no legend; "8-15: System Flags reserved, do not change the status" ([[concepts/flag-registers]]).
- Reg 231 Note 2 (p4): "Absolute setpoint value from 5 to 40. For the setpoint, write the value within the selected scale." Behaviour on an out-of-range write (refused, clamped, exception?) is **not stated**.
- Reg 233 printed "0/3 (inv)/5 (est)", description "Seasonal auto/manual", default 3. The meaning of **0 is not stated**; 0 = seasonal auto (and 3/5 = manual winter/summer) is *inferred* from the description. Observed 2026-11-28 (session observation, D.7 above): 3 = heating, 5 = cooling on this panel; 0 not observed.
- Reg 218/219: file 2 descriptions omit "temperature" ("Minimum water for heating" / "Maximum water for cooling") ([[sources/n420384a-modbus-rtu]]).
- Reg 233 labels "inv"/"est" read as Italian (inverno = winter, est**ate** = summer) *(inferred)*.
- Limits column shows "SHP" for 231 and "SPH" elsewhere; likely a typo for SPH *(inferred)*.
- MVV limit for 210 prints "MVV3-4" (unclear whether minus 4 or MVV3/MVV4).
- Registers 210–219 hold fan RPM-like values, unit not stated.
- Address and function codes: [[concepts/modbus-rs485-link]], [[concepts/device-addressing]].
