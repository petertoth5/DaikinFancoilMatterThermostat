---
title: WEB supervisor flags (register 247)
type: concept
sources: ["EKWHCTRL0.EKWHCTRL1.EKRTCTRL1_Installation manual_N420384A_English.pdf", "EKWHCTRL1,EKRTCTRL1,EKWHCTRL0_Installation manual_N420384A ModBus RTU_English.pdf"]
updated: 2026-11-01
---

# WEB flags – reg 247 (R/W)

For webserver/supervisor control of the panel. Values persist over power loss, except FLAG 7 (cleared). When a restriction is active the local panel flashes an icon on blocked key presses.

| Flag | Function | Effect |
|---|---|---|
| 0 | Led WEB OFF | 1 = WEB icon always off |
| 1 | Forced off | Unit off, user cannot switch on (source says "247.0", likely 247.1 *(inferred)*) |
| 2 | Disable program rotation | User cannot change program |
| 3 | Disable standby select | User cannot switch on/off |
| 4 | Inhibit extremes | Setpoint adjustable but 5 °C/40 °C ends disabled |
| 5 | Setpoint restriction | Setpoint limited to SPL_W–SPH_W (reg 245/246) |
| 6 | Disable all keys | All keys blocked |
| 7 | Bypass for 1 h | Set by user holding SET+ 5 s while flags 1–6 active; "nr" shown 4 s; inhibits flags 1–6 for 1 h |
| 8 | Seasonal key disabled | Season key locked |

Setpoint entry is through reg 231. Troubleshooting (p7): cannot switch on → check flag 1, or CP presence input closed with E_SAVING (209)=0 puts the board in standby; cannot switch off/change function → flag 6; cannot move setpoint → flag 5 and regs 245/246.

See [[concepts/register-map]], [[concepts/flag-registers]].
