---
title: Bus topology (planned)
type: concept
sources: ["project-brief.md", "EKWHCTRL0.EKWHCTRL1.EKRTCTRL1_Installation manual_N420384A_English.pdf", "EKWHCTRL1,EKRTCTRL1,EKWHCTRL0_Installation manual_N420384A ModBus RTU_English.pdf", "MAX485-M_EN_10035508.pdf", "20261107_192127.jpg"]
updated: 2026-11-07
---

# Bus topology (planned)

**Current state** ([[sources/project-brief]]): one [[entities/ekwhctrl1]] and one fancoil [[entities/inn-fr-b32]], directly connected. **Target:** add the new ESP32-S3 node ([[entities/esp32-s3-wroom-1]] + [[entities/max485]]) to the bus without interrupting existing traffic.

Link rules that apply to the extra node: 9600 8N1, no star topology, own address ([[concepts/modbus-rs485-link]], [[concepts/device-addressing]]). The line must be terminated with 120 Ω (Daikin: "Complete the line with the 120 Ω resistance", [[sources/n420384a-english]] p3; "at both ends" per [[sources/max485-datasheet]] p13). The node adds termination only if it sits at a line end *(inferred)*, see open item 3.

## User decisions (session 2026-11-02, stated by the user; not in raw)
- *(superseded by the 2026-11-07 role below)* The new node shall be a **second master** (poll and write) on the bus. Risk: Modbus RTU normally has one master (inferred); whether EKWHCTRL1 is the current master is unverified, so arbitration/timing and a safe fallback must be designed explicitly. *Refined on 2026-11-07, see below.*

## User answers (session 2026-11-07, stated by the user; not in raw)
- **Tap point:** the EKWHCTRL1 <-> fancoil link uses the EKWHCTRL1 **"+ A B -"** block; the new node taps in on that pair. This agrees with the manual: "+ A B -" = "Serial connection for fancoil", while "A A B B -" = "ModBus RTU Serial Connection" ([[sources/n420384a-english]] p3, visually verified; [[entities/ekwhctrl1]]). User: "the wall panel is on the bus, this is sure". Whether EKWHCTRL1 is the **polling master** is **not explicitly confirmed** (can be checked by RX-only sniffing, inferred).
- **Role** (refines the 2026-11-02 second-master decision). User's words: "I want the new node to be part of the bus in a way it shall not influence the existing communication, but follow the commands added by the user from the wall panel and monitor the bus and follow the status. Only act as a controller on the bus if the users send commands through the matter thermostat via the matter fabric."
  → **Passive monitor by default**; acts as **master** (FC06 writes, any needed polls) **only on Matter-originated commands**. No slave role/own slave address requested. Arbitration/collision avoidance with the existing master remains a **design item for the architect** (inferred). Note: the manual flags a master timeout, legend "Timeout master (vers. Centralised B32) of 300sec" ("Com err", STAT bit 12, [[concepts/flag-registers]]); STAT exists only on [[entities/inn-fr-b32]] (p4 footnote).
- **Polarity:** user checked the hardware; the terminal label **reads "+ A B -"**, as printed for EKWHCTRL1 in both manual files (see [[concepts/modbus-rs485-link]]).
- **INN-FR-B32 location:** user was not sure yet, would check PCB labels. Since resolved from the manuals (open item 0 below); the label check is optional.
- **MAX485-M:** breakout module, photo provided ([[sources/max485-module-photo]]).

## User follow-up (session 2026-11-07, stated by the user; not in raw)
- **Module termination:** "I had to buy a 120 ohm separately, it is not on the board by default." → no on-board 120 Ω; the user has a separate 120 Ω resistor. Conflicts with the photo inference that R7 is a termination; user statement wins, R7 unexplained ([[entities/max485]], [[sources/max485-module-photo]]).
- **Existing bus termination:** "The bus currently works, but I do not know yet if there is 120 Ω termination on it or not. This has to be measured as soon as I'll have access to the fan coil again. Keep this open." → open item 3.
- **Fancoil port:** the user checked: the fancoil pair is physically on the EKWHCTRL1 **"+ A B -"** block → item 1a answered. (The "manual conflict" shown to the user came from a wiki misreading of p3; the manual agrees, see 1a.)

## Firmware plan (user decision 2026-11-07, not in raw)
The node's role is implemented in phases ([[concepts/firmware-architecture]]): P1 RX-only sniffer (DE and /RE to GND, no TX code) measures open items **1, 1b, 4, 5** below; items **3/3a** are checked in the P1 bring-up checklist (`tasks/008` -> `docs/p1-bringup/`); P2 read-only Matter mirror; P3 guarded Matter-triggered writes (outline for item 2: idle windows learned in P1, pacing, readback, latching kill switch), specified only after the P1 data. Tap point for TX (item 6): listen on "+ A B -" first, decide after P1 (D1).

## Open questions
**Answered**
- Q1 tap point: "+ A B -" block of EKWHCTRL1 (user, 2026-11-07); fancoil pair confirmed on that block (item 1a below). Alternative tap point: see open item 6.
- Q2 role: passive monitor + Matter-triggered master (user, 2026-11-07).
- Q3 MAX485-M: breakout board with RO/RE/DE/DI header, separate DE/RE ([[sources/max485-module-photo]]). *Hardware details still open, see below.*
- Q4 own slave address: not requested (follows from Q2).
- Q5 polarity: for EKWHCTRL1 no conflict ever existed (both files print "+ A B -"), and the user confirmed the label on the hardware. The "+AB-"/"+BA-" legend conflict concerns only EKRTCTRL1 (not installed here) and stays an unresolved source conflict ([[concepts/modbus-rs485-link]]).
- 1a fancoil port: the fancoil pair is on the EKWHCTRL1 "+ A B -" block (user observation, 2026-11-07). **Agrees with the manual**: p3 legend B "Serial connection for fancoil" = "+ A B -" ([[sources/n420384a-english]] p3, visually verified; [[entities/ekwhctrl1]]). The earlier "source vs user" conflict was a wiki misreading and is withdrawn.
- 0 INN-FR-B32 location (**resolved**, 2026-11-07): INN-FR-B32 is the fancoil control PCB in the appliance's electrical box (shown as N540022B in the p2/p3 figures; identity by terminal-label comparison, inferred, high confidence; [[sources/n420384a-english]] p1–p3). The manual figures agree with the brief; the user may confirm via the PCB label (optional) ([[entities/inn-fr-b32]]).

**Still open**
1. Who is master on the "+ A B -" link today (EKWHCTRL1 polling the fancoil, or otherwise)? Verify by RX-only sniffing (inferred).
1b. Is the "+ A B -" fancoil link actually Modbus RTU 9600 8N1? The manual calls it "Serial connection for fancoil", not Modbus RTU (p3); p1 says the INN-FR-B32 "+AB-" terminal connects to a Modbus RTU network and p3 draws the line as "RS485", so likely yes *(inferred, unverified)*. Sniffing will tell.
2. Arbitration/bus-idle detection for Matter-triggered writes without colliding with the existing master (architect design item).
3. **Existing bus termination (keep open, measure on-site):** is the existing EKWHCTRL1 <-> fancoil line already terminated with 120 Ω, and at which end(s)? Where will the user's separately bought 120 Ω go? User: the bus works today, termination unknown, to be measured when the fancoil is accessible. Rule: Daikin requires terminating the line with 120 Ω and no star topology ([[concepts/modbus-rs485-link]], [[sources/n420384a-english]] p3); 120 Ω at both bus ends per [[sources/max485-datasheet]] p13 ([[entities/max485]]). The MAX485-M itself has no on-board termination (user).
3a. MAX485-M hardware: U1 marking, supply voltage, bias resistors, R7 role, RO 5 V level shift ([[entities/max485]]).
4. Are OUT (009) and STAT (104), marked "only for INN-FR-B32 board", readable in this setup? Relevant because the node wants to follow status ([[concepts/flag-registers]]).
5. Daikin A/B vs MAX485 A (non-inverting) / B (inverting): the electrical mapping is still to verify by measurement or sniffing *(inferred)* ([[concepts/modbus-rs485-link]], [[entities/max485]]).
6. **Tap point (design decision, not decided):** "+ A B -" fancoil pair (user's wiring) vs the EKWHCTRL1 Modbus RTU port "A A B B -", which appears unused here *(inferred from the user statement; ask whether anything is connected)*. Architect/user decision ([[entities/ekwhctrl1]]).
7. The p3 lower diagram labels the "A A B B -" block "CP presence contact" (item 2), which sits oddly with legend A (Modbus RTU); recorded as printed, unresolved ([[entities/ekwhctrl1]]).
