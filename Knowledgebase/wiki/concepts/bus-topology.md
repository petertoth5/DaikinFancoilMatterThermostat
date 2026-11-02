---
title: Bus topology (planned)
type: concept
sources: ["project-brief.md", "EKWHCTRL1,EKRTCTRL1,EKWHCTRL0_Installation manual_N420384A ModBus RTU_English.pdf", "MAX485-M_EN_10035508.pdf"]
updated: 2026-11-02
---

# Bus topology (planned)

**Current state** ([[sources/project-brief]]): one [[entities/ekwhctrl1]] and one fancoil [[entities/inn-fr-b32]], directly connected. **Target:** add the new ESP32-S3 node ([[entities/esp32-s3-wroom-1]] + [[entities/max485]]) to the bus without interrupting existing traffic.

Link rules that apply to the extra node: 9600 8N1, 120 Ω termination, no star topology, own address ([[concepts/modbus-rs485-link]], [[concepts/device-addressing]]).

## Open questions
0. **Conflict:** the brief places the INN-FR-B32 "in the fancoil"; the Daikin manuals describe it as the board inside the wall controls ([[entities/inn-fr-b32]]). Which is physically on the bus?
1. Which device is the bus master today (EKWHCTRL1 towards the fancoil)? Is the EKWHCTRL1↔INN-FR-B32 link the same bus as the "+ A B -" Modbus port, or the separate "A A B B -" port ([[entities/ekwhctrl1]])? Wiring (where exactly the new node taps in) depends on it.
2. Should the new node be a second master, or a passive listener plus slave? Modbus RTU normally allows one master (inferred; not stated in the manuals).
3. MAX485-M: bare chip or module? Supply voltage, termination resistor present on it, DE/RE wiring and RX level shifting (see [[sources/max485-datasheet]], [[entities/max485]]). Note: if the existing bus is already terminated per the Daikin rules *(inferred; the brief does not say)*, a module with a fixed 120 Ω would add a termination in the middle of the line if the new node is not at an end.
4. Does the new node need its own Modbus address, or does it act as a replacement for/proxy of the wall control?
5. Terminal polarity conflict +AB- vs +BA- ([[concepts/modbus-rs485-link]]) must be verified on hardware.
