---
title: WEB supervisor flags (register 247)
type: concept
sources: ["EKWHCTRL0.EKWHCTRL1.EKRTCTRL1_Installation manual_N420384A_English.pdf", "EKWHCTRL1,EKRTCTRL1,EKWHCTRL0_Installation manual_N420384A ModBus RTU_English.pdf"]
updated: 2026-11-28
---

# WEB flags – reg 247 (R/W)

For webserver/supervisor control of the panel (p6: "For the interaction with a webserver supervisor, some control flags have been added. The flags are grouped in a single register, accessible in reading/writing."). When a restriction is active the local panel flashes an icon on blocked key presses (p6). Source: [[sources/n420384a-english]] p6, same in [[sources/n420384a-modbus-rtu]] except where noted.

**Verification:** quotes checked by `datasheet-reader` 2026-11-08 (both files, images) and **re-checked 2026-11-27** by `datasheet-reader`: file 1 pp. 3–7 image-verified, file 2 p6 image-verified (file 2 pp. 3–4 text layer only).

**Persistence (p6, exact):** "If the power supply is cut off, the data is maintained. The selections carried out by the user are maintained if the power supply is cut off, except for "FLAG 7" that will be deleted." Ambiguous whether "the data" covers only reg 247 or also other registers (231, 233, 201...); not stated. The manual says nothing about EEPROM storage, write endurance or a permitted write rate for 231 / 233 / 201 (or 247).

**Bit numbering:** the manual labels the rows "FLAG n (247.n)" and never uses the words "bit" or "LSB". FLAG n = bit n with bit 0 = LSB is ***inferred*** (confidence medium-high): consistent with the p5 bit tables, which number bits 0–15 ([[concepts/flag-registers]]). To confirm on site by reading 247. **2026-11-28 (D.3, session observation, not in raw):** 247 read `0x0000` (all flags 0, no supervisor restriction set; value equals the printed default 0). This **does not confirm** the numbering (every numbering reads 0 when no flag is set); confirmation still needs a read with a flag set. **Still open.** Live values: [[concepts/register-map]] "First live values".

| Flag | Function (printed) | Effect (printed / summary) |
|---|---|---|
| 0 | "Led WEB OFF" | "If = 1, the ⚠ icon is always set to off" (the icon is printed as a symbol; reading it as the "WEB icon" is an interpretation *(inferred)*). **On site 2026-11-28 (D.6, user, not in raw):** with 247 = 0 and polling running, the panel showed nothing new, no WEB icon ([[concepts/firmware-architecture]] section 7) |
| 1 | Forced off | Machine off, user cannot switch it on. Row printed "FLAG 1 **(247.0)**" in **both** files (confirmed on the image 2026-11-27); typo for 247.1 *(inferred)* |
| 2 | Disables the rotation of the programmes | User cannot change the program |
| 3 | Disables select/deselect of the stby function | User cannot switch the unit on and off |
| 4 | Inhibits the extremes | p6: "The user can adjust the set point within the preset scale, but the extremes 5°C and 40°C are disabled. Pressing the SET+ and SET- keys outside the limits makes the icon flash temporarily..." (file 2: "SET+ **e** SET-", Italian "e" = and, *inferred*). Describes **local keys only**; the effect on Modbus writes (e.g. 231) is **not stated** |
| 5 | "Enables the set point restriction" | p6: "The user can adjust the setpoint within SPL_W-SPH_W, preventing off-scale values at the same time." (reg 245/246) |
| 6 | Disables all keys | All keys blocked |
| 7 | "Webserver function bypassed for one hour" / "⏻ and SET+ keys for 5 sec." | p6: "Condition: FLAG 1…6=1 ⏻ and SET+ keys pressed for at least 5 seconds. "nr" (not restricted) is displayed for 4 seconds." "Action: FLAG 7=1 inhibition of FLAGS 1/6 for 1 hour." Same wording in both files (power key printed as a symbol). Reading "FLAGS 1/6" as flags 1–6 is an interpretation *(inferred)*. Cleared on power loss |
| 8 | Seasonal key disabled | Season key locked |

**Correction 2026-11-27:** earlier wiki text put the file-2 wording "SET+ e SET-" in FLAG 7 and gave the FLAG 7 keys as "SET+" only; image check shows "SET+ e SET-" is in FLAG 4's effect text, and FLAG 7 needs the power key **and** SET+ (both files).

Below the table (p6): "The setpoint can normally be adjusted locally within the limits SPL-SPH or SPL_W and SPH_W with Flag 5=1." Setpoint entry: p6 "To enter the setpoint, write the absolute value within the selected scale in register 231 SP." ([[concepts/register-map]]). Behaviour on out-of-range writes to 231 is not stated.

Troubleshooting (p7): cannot switch on → check flag 1, or CP presence input closed with E_SAVING (209)=0 puts the board in standby; cannot switch off/change function → flag 6; cannot move setpoint → flag 5 and regs 245/246.

See [[concepts/register-map]], [[concepts/flag-registers]], [[concepts/firmware-architecture]] (W9 uses these flags).
