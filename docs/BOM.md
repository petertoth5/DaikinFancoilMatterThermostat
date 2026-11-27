# Bill of materials (whole process + final deployment)

As of 2026-11-27 (sections 1, 2, 4 updated for the user decisions of 2026-11-27). Refs: ekw = wiki `entities/ekwhctrl1`, max485 = `entities/max485`, esp = `entities/esp32-s3-wroom-1`, topo = `concepts/bus-topology`, arch sN = `concepts/firmware-architecture` section N, dev = `concepts/dev-environment`, chk X = `docs/phase1-probe/checklist.md` section X, [setup 2026-11-25] = user setup of today's session (not in raw).

## 1. Development and bench (phase 1, chk B)
- [ ] ESP32-S3-WROOM-1-N16R8 board (have) [esp]
- [ ] USB cable to the board's UART port (CH343, `/dev/ttyACM0`) [dev]
- [ ] MAX485-M breakout module (have) [max485]
- [ ] Jumper wires / breadboard (TX->DI, RO->RX, DE+/RE->DE GPIO, GND, VCC) [arch s4, chk B.1]
- [ ] 10 kOhm resistor DE -> GND (if not on the module) [arch s4, chk B.1/B.2]
- ~~RO -> RX level shift, e.g. divider 10 kOhm + 20 kOhm~~ (rejected, arch s4)
- [x] Bench RX 2026-11-27: BZX55C5V1 (used forward as a diode) + 2 x 10 kOhm pull-up to 3.3 V (bench only; superseded by RO direct at 3.3 V) [arch s4]
- [ ] Module VCC from the ESP board 3V3 pin [arch s4]
- [ ] Multimeter (DC volts, ohms, continuity) [chk B.2, B.4, B.6]
- [ ] Optional: second RS485 or USB-UART adapter for the latched run [chk B.14]
- [ ] Laptop: WSL2 + usbipd-win, ESP-IDF v6.0.2, serial terminal with log saving [dev, chk B.3]

## 2. On site, phase-1 probe (chk A, C, D)
- [ ] Node from section 1, fully wired, flashed with the probe image fw `6654669-dirty` (phase-1 code b8ad00d) [chk header]
- [ ] Module: the current MAX485-M (user decision 2026-11-27, D23 bench-only rule overridden), VCC from the ESP 3V3 pin, R7 120 Ohm fitted [arch s4, s6 OS1, chk header]
- [ ] 3 x 10 kOhm: 1 DE pull-down + 2 in series as the RX pull-up (20 kOhm to 3.3 V); RO direct to GPIO4 (no diode) [arch s4, s6 OS4]
- [ ] Laptop with serial console, saving the complete output (also powers the node via USB) [chk D.2, arch s4 "normal USB power"]
- [ ] USB cable (long enough to reach the panel) (inferred)
- [ ] Multimeter: continuity A-A / B-B, resistance A-B and "-"-CP, DC volts "-"/A/B vs node GND [chk C.2-C.4]
- [ ] Short leads from the node to one A, one B and "-" of the "A A B B -" block; no star [ekw, arch s4, chk D.1]
- ~~Your separate 120 Ohm resistor~~ NOT used on site (R7 is fitted on the MAX485-M) [arch s4, chk C.3, D.1]
- [ ] Small screwdriver for the terminal block (inferred)
- [ ] Phone/camera to photograph both terminal blocks before and after [chk C.1, D.11]
- [ ] Printed checklist + measurement log template [chk header]
- Rules, not items: never touch "+ A B -" or CP [chk A.2]; node GND to "-" only after "-", A, B measured within -7..+12 V of node GND [arch s4, chk A.3]

## 3. Matter / controllers (in place, for completeness)
- [x] 2.4 GHz Wi-Fi: ASUS RT-AC88U, IPv6 Native, Roaming assistant off, AP isolation off [dev, setup 2026-11-25]
- [x] Synology NAS with `python-matter-server` container (host network; back up `/data`) [dev, setup 2026-11-25]
- [x] Home Assistant Core (Docker on the NAS) [dev]
- [x] Android phone with HA app + Google Play Services [dev, setup 2026-11-25]
- [ ] Optional: Apple TV / iPhone for Apple Home (D7 pending) [dev, arch s8]
- [ ] Optional: chip-tool host (O7, D4 IM status code) [arch s8]
- [ ] Later on site: SmartThings hub acceptance (phase 2b/3) [dev]

## 4. Final deployment
- [ ] ESP32-S3-WROOM-1-N16R8 board + **MAX3485 (3.3 V) module** per the buying criteria (D23, arch s8) [esp, max485, arch s4, s8]
- [ ] 10 kOhm DE pull-down; RO direct to GPIO4; module VCC from the ESP 3V3 pin [arch s8 "Wiring"]
- [ ] 120 Ohm termination: only if the phase-1 measurement / error rate calls for it [arch s4, topo item 3]
- [ ] Power supply for the node: **to decide (open question)**; only phase-1 power is decided (USB) [arch s4]
- [ ] Isolation: whether the panel's low-voltage side is isolated from mains is **unknown** [arch s4, s8]; need for isolation / USB isolator **to decide (open question)** (isolator was declined for phase 1) [arch s4]
- [ ] Enclosure: **to decide (open question)** (not in wiki)
- [ ] Bus cable to "A A B B -" (type, length, twisted pair, shield): **to decide (open question)**; wiki only says short leads, no star [arch s4]
- [ ] Mounting location / method: **to decide (open question)** (not in wiki)
- [ ] Bias resistors on the line: **to decide** after phase 1 (module R5/R6/R8 unknown) [arch s4, max485]
- [ ] Access to BOOT button (factory reset = hold GPIO0 5 s) [arch s7] (access requirement inferred)
- [ ] Wi-Fi coverage at the panel (2.4 GHz; board ran at -81 dBm on one AP) [dev] (coverage need inferred)
