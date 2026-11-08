---
title: WEB supervisor flags (register 247)
type: concept
sources: ["EKWHCTRL0.EKWHCTRL1.EKRTCTRL1_Installation manual_N420384A_English.pdf", "EKWHCTRL1,EKRTCTRL1,EKWHCTRL0_Installation manual_N420384A ModBus RTU_English.pdf"]
updated: 2026-11-08
---

# WEB flags – reg 247 (R/W)

For webserver/supervisor control of the panel (p6: "For the interaction with a webserver supervisor, some control flags have been added"). Flags are readable and writable. p6: "If the power supply is cut off, the data is maintained ... except for 'FLAG 7' that will be deleted". When a restriction is active the local panel flashes an icon on blocked key presses. (Quotes checked by `datasheet-reader`, both files, pages rendered as images, 2026-11-08.)

| Flag | Function | Effect |
|---|---|---|
| 0 | Led WEB OFF | 1 = WEB icon always off |
| 1 | Forced off | Unit off, user cannot switch on (printed "(247.0)" in **both** files; typo for 247.1 *(inferred)*) |
| 2 | Disable program rotation | User cannot change program |
| 3 | Disable standby select | User cannot switch on/off |
| 4 | Inhibit extremes | Setpoint adjustable but 5 °C/40 °C ends disabled |
| 5 | Setpoint restriction | Setpoint limited to SPL_W–SPH_W (reg 245/246) |
| 6 | Disable all keys | All keys blocked |
| 7 | Bypass for 1 h | User can bypass the webserver restrictions FLAG 1–6 for 1 h: hold the SET+ keys 5 s (file 2: "SET+ e SET-"); "nr" shown 4 s; cleared on power loss |
| 8 | Seasonal key disabled | Season key locked |

Setpoint entry: p6 "To enter the setpoint, write the absolute value within the selected scale in register 231 SP." ([[concepts/register-map]]). Troubleshooting (p7): cannot switch on → check flag 1, or CP presence input closed with E_SAVING (209)=0 puts the board in standby; cannot switch off/change function → flag 6; cannot move setpoint → flag 5 and regs 245/246.

See [[concepts/register-map]], [[concepts/flag-registers]].
