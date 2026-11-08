---
title: Bus topology (planned)
type: concept
sources: ["project-brief.md", "EKWHCTRL0.EKWHCTRL1.EKRTCTRL1_Installation manual_N420384A_English.pdf", "EKWHCTRL1,EKRTCTRL1,EKWHCTRL0_Installation manual_N420384A ModBus RTU_English.pdf", "MAX485-M_EN_10035508.pdf", "20261107_192127.jpg"]
updated: 2026-11-08
---

# Bus topology (planned)

**Current state** ([[sources/project-brief]]): one [[entities/ekwhctrl1]] and one fancoil [[entities/inn-fr-b32]], directly connected. **Target:** add the new ESP32-S3 node ([[entities/esp32-s3-wroom-1]] + [[entities/max485]]) without interrupting the existing panel <-> fancoil traffic.

> **2026-11-08 (user decisions): tap point = EKWHCTRL1 "A A B B -" ModBus RTU port, NOT the "+ A B -" fancoil link. Nothing else is connected to "A A B B -". The node is the SOLE Modbus master on "A A B B -" (FC03 polling, FC06 only on Matter commands). Staged phasing: on-site read probe -> Matter read-only mirror -> Matter writes. Architecture v0.2 superseded; **v0.3 approved by the user 2026-11-08** ([[concepts/firmware-architecture]]).** Sections dated 2026-11-07 that assume "+ A B -" or a shared bus are kept as history and marked superseded.

Resulting topology *(inferred from the user answers)*: point-to-point segment **node (master) <-> EKWHCTRL1 "A A B B -"**; the "+ A B -" panel <-> fancoil link stays untouched by the node.

Link facts for that segment: 9600 8N1, FC 03/06 ([[concepts/modbus-rs485-link]], [[sources/n420384a-english]] p3); slave address set on the panel, reg 200 ([[concepts/device-addressing]]). The manual does not say which port these settings or the p3 cabling rules (120 Ω, no star, 500 m / 50 units) refer to; see [[concepts/modbus-rs485-link]] and open items 1b/3. The MAX485 datasheet puts the characteristic-impedance termination at both bus ends ([[sources/max485-datasheet]] p13).

## User decisions (session 2026-11-02, stated by the user; not in raw)
- *(superseded by the 2026-11-07 role, itself superseded 2026-11-08)* The new node shall be a **second master** (poll and write) on the bus. Risk: Modbus RTU normally has one master (inferred); whether EKWHCTRL1 is the current master is unverified, so arbitration/timing and a safe fallback must be designed explicitly.

## User answers (session 2026-11-08, second round, stated by the user; not in raw)
- **Manual read by the user:** one of the two N420384A PDFs already in raw ([[sources/n420384a-english]], [[sources/n420384a-modbus-rtu]]); no new source.
- **Nothing is connected to the EKWHCTRL1 "A A B B -" port today.** Answers the 2026-11-08 "anything else connected?" question.
- **Role on "A A B B -": the node is the SOLE Modbus master.** Periodic **FC03** polling mirrors panel/fancoil state into Matter; **FC06** writes only on Matter commands. **Supersedes** the 2026-11-07 "passive monitor by default" role (text kept below), which only made sense for a shared bus.
- **Phasing (staged):** phase 1 = on-site read probe (confirm slave address, A/B polarity, which registers answer); phase 2 = Matter read-only mirror; phase 3 = Matter writes with readback and limits. **Write code compiled out until phase 3.**
- **"-AB+"** in the user's earlier message meant the **"+ A B -"** block (panel <-> fancoil, stays untouched by the node). The earlier "(inferred)" reading is now confirmed by the user.
- Consequence *(inferred from these answers)*: second-master arbitration and bus-idle detection no longer apply on "A A B B -" (sole master, nothing else connected).

## User decision (session 2026-11-08, first round, stated by the user; not in raw)
- User's words: "I read through the EKWHCTRL1 Modbus RTU user manual by myself recently. I found out that the -AB+ connector is really for the direct connection of the fancoil to the thermostat. So this connector is not the modbus we shall use but we have to use the AABB connector instead. This needs a redesign because the whole concept relied on the false information I estimated before."
- → **Tap point decided:** the new node connects to the EKWHCTRL1 **"A A B B -"** port (legend A "ModBus RTU Serial Connection"), **not** to the **"+ A B -"** fancoil link (legend B "Serial connection for fancoil"). This agrees with the manual ([[sources/n420384a-english]] p3, visually verified by `datasheet-reader`, PDF pages rendered as images; [[entities/ekwhctrl1]]). The user's "-AB+" means the block printed "+ A B -" (confirmed by the user, second round).
- The fancoil itself stays on "+ A B -" (unchanged; 2026-11-07 wiring observation still valid).
- *(Superseded by the second-round answer: sole master.)* Role on the new port: pending user re-confirmation in redesign. The 2026-11-07 role (passive monitor by default, master only on Matter commands) was framed for listening to existing traffic on "+ A B -". On "A A B B -" there may be no existing traffic to monitor, and EKWHCTRL1's role there (master or slave) is not yet extracted from the manual.
- *(Answered in the second round: nothing connected.)* Unknown, ask user: is anything else already connected to "A A B B -"?

## User answers (session 2026-11-07, stated by the user; not in raw)
- *(Superseded 2026-11-08: tap point is now "A A B B -", see above.)* **Tap point:** the EKWHCTRL1 <-> fancoil link uses the EKWHCTRL1 **"+ A B -"** block; the new node taps in on that pair. This agrees with the manual: "+ A B -" = "Serial connection for fancoil", while "A A B B -" = "ModBus RTU Serial Connection" ([[sources/n420384a-english]] p3, visually verified; [[entities/ekwhctrl1]]). User: "the wall panel is on the bus, this is sure". Whether EKWHCTRL1 is the **polling master** is **not explicitly confirmed** (can be checked by RX-only sniffing, inferred).
- **Role** *(superseded 2026-11-08 by "sole master on A A B B -", see above)*. User's words: "I want the new node to be part of the bus in a way it shall not influence the existing communication, but follow the commands added by the user from the wall panel and monitor the bus and follow the status. Only act as a controller on the bus if the users send commands through the matter thermostat via the matter fabric."
  → **Passive monitor by default**; acts as **master** (FC06 writes, any needed polls) **only on Matter-originated commands**. No slave role/own slave address requested. Arbitration/collision avoidance with the existing master remains a **design item for the architect** (inferred). Note: the manual flags a master timeout, legend "Timeout master (vers. Centralised B32) of 300sec" ("Com err", STAT bit 12, [[concepts/flag-registers]]); STAT exists only on [[entities/inn-fr-b32]] (p4 footnote). *(2026-11-08: this timeout most likely concerns a panel-less, centrally driven INN-FR-B32 and is not evidence about the "A A B B -" port, inferred low-medium; see [[concepts/flag-registers]].)*
- **Polarity:** user checked the hardware; the terminal label **reads "+ A B -"**, as printed for EKWHCTRL1 in both manual files (see [[concepts/modbus-rs485-link]]).
- **INN-FR-B32 location:** user was not sure yet, would check PCB labels. Since resolved from the manuals (open item 0 below); the label check is optional.
- **MAX485-M:** breakout module, photo provided ([[sources/max485-module-photo]]).

## User follow-up (session 2026-11-07, stated by the user; not in raw)
- **Module termination:** "I had to buy a 120 ohm separately, it is not on the board by default." → no on-board 120 Ω; the user has a separate 120 Ω resistor. Conflicts with the photo inference that R7 is a termination; user statement wins, R7 unexplained ([[entities/max485]], [[sources/max485-module-photo]]).
- **Existing bus termination:** "The bus currently works, but I do not know yet if there is 120 Ω termination on it or not. This has to be measured as soon as I'll have access to the fan coil again. Keep this open." → open item 3.
- **Fancoil port:** the user checked: the fancoil pair is physically on the EKWHCTRL1 **"+ A B -"** block → item 1a answered. (The "manual conflict" shown to the user came from a wiki misreading of p3; the manual agrees, see 1a.)

## Firmware plan v0.3 (user decision 2026-11-08, not in raw)
Architecture v0.3, approved 2026-11-08 ([[concepts/firmware-architecture]]):
- **D1 hard rule:** the node taps one A and one B of "A A B B -" and is the sole master there; **"+ A B -" (and the CP terminal) are never touched** (section 4 there).
- **Gate exception (Q8, D2):** stage-1 (phase-1 probe) specs are released **before** the on-site items below are answered; stage-2/3 specs only after the probe log is in `raw/` and captured.
- The phase-1 probe exit criteria (firmware-architecture section 1, items 1–11) answer the [P1] items below: 1 (items 1, 3), 1b (items 2, 5), 2 (items 6, 7, 10), 3 and 3a (item 11, termination procedure in section 4), 4 (item 4), 5 (item 2), 7 (item 11) *(mapping inferred by wiki-keeper)*.
- Boot silence check (D17) refuses to transmit if any foreign traffic is seen on "A A B B -" (tests the inferred "panel is slave", item 1).

## Firmware plan v0.2 (user decision 2026-11-07, not in raw)
*(Superseded 2026-11-08: P1 listening on "+ A B -" no longer applies; replaced by the staged phasing above (read probe -> read-only mirror -> writes) and by v0.3 (section above).)* The node's role is implemented in phases ([[concepts/firmware-architecture]]): P1 RX-only sniffer (DE and /RE to GND, no TX code) measures open items **1, 1b, 4, 5** below; items **3/3a** are checked in the P1 bring-up checklist (`tasks/008` -> `docs/p1-bringup/`); P2 read-only Matter mirror; P3 guarded Matter-triggered writes (outline for item 2: idle windows learned in P1, pacing, readback, latching kill switch), specified only after the P1 data. Tap point for TX (item 6): listen on "+ A B -" first, decide after P1 (D1).

## Manual facts on "A A B B -" (both N420384A files, `datasheet-reader`, all pages rendered as images, p3 at 600 dpi, 2026-11-08)
Details and quotes: [[entities/ekwhctrl1]]. Summary:
- **Not stated** anywhere in the manual for "A A B B -": master/slave role, meaning of the doubled A/B terminals, wiring, timing, polling interval, response time, broadcast, exception codes, write endurance.
- EKWHCTRL1 is a **Modbus slave** on that port *(inferred, medium-high)*, from p1/p3/p6 wording ([[entities/ekwhctrl1]]). Doubled A/B = daisy-chain in/out *(inferred, medium)*.
- The block is 6-way "[contact symbol] A A B B -"; the CP presence contact is wired between the contact-symbol terminal and "-", nothing on A/A/B/B (p3).
- Registers reachable: the p4 map; starred ("* Only for INN-FR-B32 board") registers may be absent via the panel *(inferred)* ([[concepts/register-map]]).

## Open questions
**Answered**
- Q1 tap point: *(superseded 2026-11-08)* "+ A B -" block of EKWHCTRL1 (user, 2026-11-07); fancoil pair confirmed on that block (item 1a below). **Now: "A A B B -", see item 6.**
- Q2 role: *(2026-11-07: passive monitor + Matter-triggered master; superseded)* **Now: sole Modbus master on "A A B B -"**, FC03 polling, FC06 only on Matter commands (user, 2026-11-08).
- Q3 MAX485-M: breakout board with RO/RE/DE/DI header, separate DE/RE ([[sources/max485-module-photo]]). *Hardware details still open, see below.*
- Q4 own slave address: not needed; the node is the master (follows from Q2).
- Q5 polarity: for EKWHCTRL1 no conflict ever existed (both files print "+ A B -"), and the user confirmed the label on the hardware. The "+AB-"/"+BA-" legend conflict concerns only EKRTCTRL1 (not installed here) and stays an unresolved source conflict ([[concepts/modbus-rs485-link]]).
- 1a fancoil port: the fancoil pair is on the EKWHCTRL1 "+ A B -" block (user observation, 2026-11-07). **Agrees with the manual**: p3 legend B "Serial connection for fancoil" = "+ A B -" ([[sources/n420384a-english]] p3, visually verified; [[entities/ekwhctrl1]]). The earlier "source vs user" conflict was a wiki misreading and is withdrawn.
- 0 INN-FR-B32 location (**resolved**, 2026-11-07): INN-FR-B32 is the fancoil control PCB in the appliance's electrical box (shown as N540022B in the p2/p3 figures; identity by terminal-label comparison, inferred, high confidence; [[sources/n420384a-english]] p1–p3). The manual figures agree with the brief; the user may confirm via the PCB label (optional) ([[entities/inn-fr-b32]]).
- 6 tap point: **DECIDED 2026-11-08** = EKWHCTRL1 **"A A B B -"** (quote above); agrees with the manual p3 legend A/B. **Nothing else is connected to it** (user, 2026-11-08).
- 8 node role on "A A B B -": **sole master** (user, 2026-11-08).
- "-AB+" meaning: = "+ A B -" block (user, 2026-11-08).

**Still open** (2026-11-08; items marked [P1] are to be answered by the phase-1 on-site read probe)
1. EKWHCTRL1's role on "A A B B -": not stated in the manual; **slave** *(inferred, medium-high)*, see [[entities/ekwhctrl1]]. [P1] Does EKWHCTRL1 answer at its reg-200 address (default 1) at 9600 8N1 on "A A B B -"? Note: the manual documents only one address (reg 200 / keypad, p4/p6) and does not say which port it applies to (inferred: the panel's slave address, [[concepts/device-addressing]]).
1b. Serial settings: p3 gives 9600 / 8 data bits / no parity / 1 stop bit and FC "03"/"06" without naming the port ([[concepts/modbus-rs485-link]]). [P1] confirm on "A A B B -"; [P1] does FC03 accept quantity > 1 (not stated)?
2. *(Reframed 2026-11-08)* Arbitration/bus-idle detection: **no longer applies on "A A B B -"** (sole master, nothing else connected; inferred from the user answers). Remaining: [P1] polling pacing / response time (not stated), and the panel's fail-safe behaviour when the master stops polling (not stated; the 300 s STAT timeout is probably not about this port, [[concepts/flag-registers]]).
3. **Termination (keep open, measure on-site):** concerns the short **node <-> "A A B B -"** segment. The only drawn 120 Ω is on the fancoil RS485 chain, and the p3 cabling rules do not say which port they apply to ([[concepts/modbus-rs485-link]]). [P1] Is termination needed at the panel and at the node on this short link; does EKWHCTRL1 terminate internally? Unknown. The MAX485-M has no on-board termination; the user has a separate 120 Ω (user). The former question on the existing "+ A B -" line termination no longer concerns the node's segment *(inferred)*.
3a. MAX485-M hardware: U1 marking, supply voltage, bias resistors, R7 role, RO 5 V level shift ([[entities/max485]]). Unchanged.
4. [P1] Starred registers ("* Only for INN-FR-B32 board", incl. OUT 009 and STAT 104): do they return data or an exception via "A A B B -"? Not stated whether the panel proxies them ([[concepts/register-map]], [[concepts/flag-registers]]).
5. [P1] Daikin A/B vs MAX485 A (non-inverting) / B (inverting) on the "A A B B -" terminals, and which A/B pair to use *(inferred)* ([[concepts/modbus-rs485-link]], [[entities/max485]]).
7. *(Resolved 2026-11-08 from the manual, `datasheet-reader`)* The p3 item-2 "CP presence contact" block: the block is 6-way "[contact symbol] A A B B -"; the CP switch is drawn between the contact-symbol terminal and "-", so CP and the Modbus port share one block, probably with a common "-" *(inferred)* ([[entities/ekwhctrl1]]). **New sub-question** [P1]: is "-" a shared reference/GND for CP and Modbus?
