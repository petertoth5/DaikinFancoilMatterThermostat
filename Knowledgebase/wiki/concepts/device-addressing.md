---
title: Device addressing
type: concept
sources: ["EKWHCTRL0.EKWHCTRL1.EKRTCTRL1_Installation manual_N420384A_English.pdf", "EKWHCTRL1,EKRTCTRL1,EKWHCTRL0_Installation manual_N420384A ModBus RTU_English.pdf"]
updated: 2026-11-28
---

# Device addressing

- Address range 1–255, default 1, held in register 200 (ADR, R/W) — [[concepts/register-map]].
- Keypad procedure (p6): panel in standby → hold AUTO 5 s → "001" flashes → set 1–255 with the **[+] / [—]** keys (as drawn in the manual; an earlier wiki version said "up/down") → confirm with the standby key or wait 10 s (`datasheet-reader`, pages rendered as images, 2026-11-08).
- Reg 200 ADR range 1..255, default 1 (p4, note 1).
- Recommendation: keep a table of addresses/locations; p7: "Do not assign any fancoil to 001 ... Address 001 – Leave free!" (for future replacements/additions).
- **Which port:** only one address is documented; the manual does not say which port (EKWHCTRL1 "A A B B -" or the fancoil link) it applies to *(inferred: the panel's slave address on "A A B B -")*. Note the tension: default is 1, yet p7 says leave 001 free (recorded as printed). Phase-1 test: does the panel answer at its reg-200 address on "A A B B -"? ([[concepts/bus-topology]] item 1).
- **This installation (user, on site 2026-11-28, checklist C.5, not in raw):** address read on the [[entities/ekwhctrl1]] panel with the keypad procedure above (standby, hold AUTO 5 s): **01**. Matches the phase-1 image address (`addr=1`), so no `mb addr` change is needed ([[concepts/firmware-architecture]] section 7). ~~Whether the panel answers at 01 on "A A B B -" is still to be shown (D.2+).~~ **Confirmed by bus reply 2026-11-28 (D.1 result / D.2, session observation, not in raw):** the panel answers FC03 at address 1 on "A A B B -". So the keypad / reg-200 address is the slave address on this port (fact for this installation; the manual still does not say so).
- Applies to [[entities/ekrtctrl1]] and [[entities/ekwhctrl1]] (also EDB649 / ESD659 kits per manual). Link: [[concepts/modbus-rs485-link]].
