---
title: EKWHCTRL1 wall control
type: entity
sources: ["EKWHCTRL0.EKWHCTRL1.EKRTCTRL1_Installation manual_N420384A_English.pdf", "EKWHCTRL1,EKRTCTRL1,EKWHCTRL0_Installation manual_N420384A ModBus RTU_English.pdf"]
updated: 2026-11-08
---

# EKWHCTRL1

Wall control with two serial terminal blocks ([[sources/n420384a-english]] p3, section "Electrical connections EKWHCTRL1 with on-board Printed circuit board EKWHCTRL0"; identical in [[sources/n420384a-modbus-rtu]] p3; **visually verified** by `datasheet-reader`, PDF pages rendered as images):
- **"+ A B -"** = legend **B "Serial connection for fancoil"**.
- **"A A B B -"** = legend **A "ModBus RTU Serial Connection"**.

(An earlier wiki version had these two roles reversed; corrected 2026-11-07 after the visual check.)
Also has a CP presence terminal and L/N supply. Hosts [[entities/ekwhctrl0]] as on-board PCB. EKWHCTRL1 has "a specific serial port to connect", so a connection from the INN-FR-B32 board is not needed in that case (p1). Wiring rules: [[concepts/modbus-rs485-link]]. Related: [[entities/ekrtctrl1]], [[entities/inn-fr-b32]].

## p3 lower diagram (visually verified)
Legend: "1. Terminal block for device connection", "2. The terminal block for CP presence contact connection", "3. PCB". Item 1 is the panel "+ A B -"; item 2 points at the block that also carries "A A B B -" (see below). Four wires run from the panel "+ A B -" to the "+ A B -" plug of PCB #1 (N540022B = [[entities/inn-fr-b32]], inferred); a dashed 2-wire "RS485" line daisy-chains to PCB #2 and ends in "120 Ω". No star. The diagram does not say who polls whom on "+ A B -". The only drawn 120 Ω and the "RS485" dashed chain are on this fancoil link (board 1 -> board 2), **not** on "A A B B -" (`datasheet-reader`, p3 rendered at 600 dpi, 2026-11-08).

## "A A B B -" port: manual facts (`datasheet-reader`, both N420384A files, all pages rendered as images, p3 at 600 dpi, 2026-11-08; PDF page = printed page)
- **Block layout (p3 upper figure):** 6-way, "[contact symbol] A A B B -". Lower diagram item 2 "The terminal block for CP presence contact connection": the CP switch is wired between the contact-symbol terminal and "-"; nothing is wired to A/A/B/B. So CP and the Modbus port share one block, probably with a common "-" *(inferred)*. This resolves the former open question (d) below.
- **Not stated** in either file: master/slave role on this port, meaning of the doubled A/B terminals, wiring, timing, polling interval, response time, broadcast, exception codes, write endurance.
- **Role: slave** *(inferred, medium-high)*, from: p1 "connection ... to a serial communication network 'Modicon ModBus' RTU"; p3 "For connection of the electronic board to a 'Modicon ModBus' RTU serial communication network: – connect to the terminal block on the wall control unit"; p6 "For the interaction with a webserver supervisor, some control flags have been added" and "...once they are recognised by the system".
- **Doubled A/B** = daisy-chain in/out *(inferred, medium)*.
- **Serial settings (p3):** baudrate=9600, data bits=8, parity=none (file 2: "parità=nessuna"), stop bit=1; functions "03" and "06" for read and write of a single register; nothing else listed. The port they apply to is not named ([[concepts/modbus-rs485-link]]).
- **Address:** one address documented (reg 200 / keypad, p4/p6); which port it applies to is not stated *(inferred: the panel's slave address)* ([[concepts/device-addressing]]).
- **Registers:** p4 map is for "the INN-FR-B32 and EKWHCTRL0 board and the EKRTCTRL1 and EKWHCTRL1 wall-mounted control panel"; starred registers are "Only for INN-FR-B32 board"; whether the panel proxies them is not stated ([[concepts/register-map]]).

## This installation (user statements, not in raw)
- **2026-11-08 (user, second round):** **nothing is connected to "A A B B -" today.** The node will be the **sole Modbus master** on "A A B B -" (FC03 polling, FC06 only on Matter commands); staged phasing read probe -> read-only mirror -> writes ([[concepts/bus-topology]]). The user's "-AB+" meant "+ A B -" (panel <-> fancoil, untouched by the node).
- **2026-11-08 (user decision):** the new node connects to the **"A A B B -"** ModBus RTU port, **not** to "+ A B -". User: "I found out that the -AB+ connector is really for the direct connection of the fancoil to the thermostat. So this connector is not the modbus we shall use but we have to use the AABB connector instead. This needs a redesign [...]" (full quote in [[concepts/bus-topology]]). Agrees with the manual p3 legend (A = Modbus RTU, B = fancoil). Redesign recorded as architecture v0.3, approved by the user 2026-11-08 ([[concepts/firmware-architecture]]).
- 2026-11-07: the fancoil pair is physically on the **"+ A B -"** block (still valid). *(Superseded 2026-11-08: "the new node is to tap in on that pair".)* This **agrees** with the manual ("+ A B -" = serial connection for fancoil). No source-vs-user conflict exists here.

## Open questions
- (a) *(Not relevant for the node since 2026-11-08; the node does not touch "+ A B -".)* The manual calls "+ A B -" the "serial connection for fancoil", not Modbus RTU. Is the panel <-> fancoil link actually Modbus RTU 9600 8N1? p1 says the INN-FR-B32 "+AB-" terminal connects to a Modbus RTU network and p3 draws the line as "RS485", so likely yes *(inferred, unverified)*.
- (b) Role on "A A B B -": manual silent; slave inferred (above). Confirm in phase 1 ([[concepts/bus-topology]] items 1/1b). Former wording (2026-11-07): who is master on the "+ A B -" link (no longer relevant for the node).
- (c) **Resolved 2026-11-08 (user):** tap point = **"A A B B -"**, and nothing else is connected to it. Former wording (2026-11-07): "A A B B -" appears unused; possible alternative tap point, not decided.
- (d) **Resolved 2026-11-08 (manual, `datasheet-reader`):** the item-2 "CP" label refers to the contact-symbol terminal of the shared 6-way block; CP is wired contact terminal <-> "-" (see above). **New sub-question:** is "-" a shared reference/GND for CP and Modbus? (phase-1 check).
- (e) Phase-1 checks on this port: answers at reg-200 address, 9600 8N1?; starred registers data or exception?; FC03 multi-register?; Daikin A/B vs MAX485 A/B; termination at panel and node?; panel fail-safe when the master stops polling? ([[concepts/bus-topology]]).

Planned role in the new bus topology: [[concepts/bus-topology]]. Project: [[concepts/project-goal]].
