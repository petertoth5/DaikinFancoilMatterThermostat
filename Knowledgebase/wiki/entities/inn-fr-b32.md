---
title: INN-FR-B32 board
type: entity
sources: ["EKWHCTRL0.EKWHCTRL1.EKRTCTRL1_Installation manual_N420384A_English.pdf", "EKWHCTRL1,EKRTCTRL1,EKWHCTRL0_Installation manual_N420384A ModBus RTU_English.pdf", "project-brief.md"]
updated: 2026-11-07
---

# INN-FR-B32 PCB

**Fancoil control PCB**, located in the electrical box of the appliance (fancoil) ([[sources/n420384a-english]] p1–p3; visually verified by `datasheet-reader`, PDF pages rendered as images):
- p1: "The terminal board +AB- of the INN-FR-B32 Printed circuit board allows the connection of the same present in the control panels EKRTCTRL1, EKWHCTRL1 and EKWHCTRL0 to a serial communication network "Modicon ModBus" RTU." p1 also: "...In this case the connection from the electronic board INN-FR_B32 is not necessary because the electronic controls EKWHCTRL1 have a specific serial port to connect."
- p1 figure "A INN-FR-B32 board" shows terminals (A B/C D DIP, RTU jumper, H4, H2, AIR, COMM, MOTOR, GRID, CP, + A B -, N EV1 N EV2, L N, CHILLER BOILER) identical to the **N540022B** PCB drawn on p2/p3; N540022B is not printed on p1. Identity INN-FR-B32 = N540022B is by terminal-label comparison *(inferred, high confidence)*.
- p2 "Access to the printed circuit board": "follow the instructions in the installation manual of the appliance", "unscrew the fixing screws of the electric box". Nothing shows the board inside a wall control.

**Location – resolved:** the manual figures agree with [[sources/project-brief]] ("the PCB in the fancoil"). The earlier "location conflict" (2026-11-02) was a misreading of p1. The user may still confirm via the PCB label (optional).

Registers marked `*` in [[concepts/register-map]] exist only on this board (most motor-speed registers 210–215, water temps, OUT, STAT, water limits; 230/234 are not starred).
OUT (009) and STAT (104) are footnoted "* Only for INN-FR-B32 board" (p4); whether they are readable in this installation is open ([[concepts/flag-registers]]).
Related: [[entities/ekrtctrl1]], [[entities/ekwhctrl1]], [[entities/ekwhctrl0]].

Planned role in the new bus topology: [[concepts/bus-topology]].
