---
title: INN-FR-B32 board
type: entity
sources: ["EKWHCTRL0.EKWHCTRL1.EKRTCTRL1_Installation manual_N420384A_English.pdf", "EKWHCTRL1,EKRTCTRL1,EKWHCTRL0_Installation manual_N420384A ModBus RTU_English.pdf"]
updated: 2026-11-02
---

# INN-FR-B32 PCB

Printed circuit board inside the EKRTCTRL1/EKWHCTRL1/EKWHCTRL0 control panels. Its `+AB-` terminal block connects the panel to a Modbus RTU network ([[sources/n420384a-english]] p1).
Registers marked `*` in [[concepts/register-map]] exist only on this board (motor speeds, water temps, OUT, STAT, water limits).
**Conflict – location:** the Daikin manuals describe this PCB as part of the wall control panels (above), while [[sources/project-brief]] calls it "the PCB in the fancoil". Unresolved; ask the user (see [[concepts/bus-topology]]).
Related: [[entities/ekrtctrl1]], [[entities/ekwhctrl1]], [[entities/ekwhctrl0]].

Planned role in the new bus topology: [[concepts/bus-topology]].
