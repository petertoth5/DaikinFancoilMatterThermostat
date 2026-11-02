---
title: EKWHCTRL1 wall control
type: entity
sources: ["EKWHCTRL0.EKWHCTRL1.EKRTCTRL1_Installation manual_N420384A_English.pdf", "EKWHCTRL1,EKRTCTRL1,EKWHCTRL0_Installation manual_N420384A ModBus RTU_English.pdf"]
updated: 2026-11-02
---

# EKWHCTRL1

Wall control with its own serial port for Modbus RTU (terminal block "+ A B -") and a second serial port ("A A B B -") towards the fancoil ([[sources/n420384a-english]] p3). Has a CP presence terminal and L/N supply.
Hosts [[entities/ekwhctrl0]] as on-board PCB. Wiring rules: [[concepts/modbus-rs485-link]]. Related: [[entities/ekrtctrl1]], [[entities/inn-fr-b32]].

Planned role in the new bus topology: [[concepts/bus-topology]]. Project: [[concepts/project-goal]].
