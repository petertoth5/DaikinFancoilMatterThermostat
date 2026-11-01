---
title: Device addressing
type: concept
sources: ["EKWHCTRL0.EKWHCTRL1.EKRTCTRL1_Installation manual_N420384A_English.pdf", "EKWHCTRL1,EKRTCTRL1,EKWHCTRL0_Installation manual_N420384A ModBus RTU_English.pdf"]
updated: 2026-11-01
---

# Device addressing

- Address range 1–255, default 1, held in register 200 (ADR, R/W) — [[concepts/register-map]].
- Keypad procedure (p6): panel in standby → hold AUTO 5 s → address flashes (001) → set with the up/down keys → press standby or wait 10 s to confirm.
- Recommendation: keep a table of addresses/locations; **leave address 001 free** for future replacements/additions.
- Applies to [[entities/ekrtctrl1]] and [[entities/ekwhctrl1]] (also EDB649 / ESD659 kits per manual). Link: [[concepts/modbus-rs485-link]].
