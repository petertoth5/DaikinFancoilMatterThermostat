---
title: EKWHCTRL1 wall control
type: entity
sources: ["EKWHCTRL0.EKWHCTRL1.EKRTCTRL1_Installation manual_N420384A_English.pdf", "EKWHCTRL1,EKRTCTRL1,EKWHCTRL0_Installation manual_N420384A ModBus RTU_English.pdf"]
updated: 2026-11-07
---

# EKWHCTRL1

Wall control with two serial terminal blocks ([[sources/n420384a-english]] p3, section "Electrical connections EKWHCTRL1 with on-board Printed circuit board EKWHCTRL0"; identical in [[sources/n420384a-modbus-rtu]] p3; **visually verified** by `datasheet-reader`, PDF pages rendered as images):
- **"+ A B -"** = legend **B "Serial connection for fancoil"**.
- **"A A B B -"** = legend **A "ModBus RTU Serial Connection"**.

(An earlier wiki version had these two roles reversed; corrected 2026-11-07 after the visual check.)
Also has a CP presence terminal and L/N supply. Hosts [[entities/ekwhctrl0]] as on-board PCB. EKWHCTRL1 has "a specific serial port to connect", so a connection from the INN-FR-B32 board is not needed in that case (p1). Wiring rules: [[concepts/modbus-rs485-link]]. Related: [[entities/ekrtctrl1]], [[entities/inn-fr-b32]].

## p3 lower diagram (visually verified)
Legend: "1. Terminal block for device connection", "2. The terminal block for CP presence contact connection", "3. PCB". Item 1 is the panel "+ A B -"; item 2 is the "A A B B -" block, labelled "CP" with a contact symbol drawn across it, nothing else wired to its A A B B terminals. Four wires run from the panel "+ A B -" to the "+ A B -" plug of PCB #1 (N540022B = [[entities/inn-fr-b32]], inferred); a dashed 2-wire "RS485" line daisy-chains to PCB #2 and ends in "120 Ω". No star. The diagram does not say who polls whom on "+ A B -".
The item-2 "CP" labelling of the "A A B B -" block is recorded as printed; it sits oddly with legend A (Modbus RTU) on the upper diagram (unresolved, open question below).

## This installation (user statement, session 2026-11-07, not in raw)
The fancoil pair is physically on the **"+ A B -"** block, and the new node is to tap in on that pair. This **agrees** with the manual ("+ A B -" = serial connection for fancoil). No source-vs-user conflict exists here.

## Open questions
- (a) The manual calls "+ A B -" the "serial connection for fancoil", not Modbus RTU. Is the panel <-> fancoil link actually Modbus RTU 9600 8N1? p1 says the INN-FR-B32 "+AB-" terminal connects to a Modbus RTU network and p3 draws the line as "RS485", so likely yes *(inferred, unverified)*; RX-only sniffing will tell.
- (b) Who is master on that link (unchanged, [[concepts/bus-topology]]).
- (c) The panel's own Modbus RTU port "A A B B -" exists and appears unused in this install *(inferred from the user statement that the fancoil is on "+ A B -"; ask the user whether anything is connected)*. Possible alternative tap point for the new node: architect/user decision, **not decided**.
- (d) Why the p3 lower diagram labels the "A A B B -" block "CP presence contact" (item 2).

Planned role in the new bus topology: [[concepts/bus-topology]]. Project: [[concepts/project-goal]].
