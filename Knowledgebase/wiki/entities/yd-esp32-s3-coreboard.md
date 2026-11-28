---
title: YD-ESP32-S3-COREBOARD (dev board carrying the ESP32-S3-WROOM-1)
type: entity
sources: ["ESP32-S3-DEV-N16R8-US_EN_10048120 (1).pdf", "ESP32-S3-DEV-N16R8-US_EN_10048120.pdf", "project-brief.md"]
updated: 2026-11-28
---

# YD-ESP32-S3-COREBOARD

Dev board (VCC-GND studio, schematic dated 2022/6/1, revision "*") carrying the [[entities/esp32-s3-wroom-1]] module ([[sources/yd-esp32-s3-coreboard-schematic]] p2). **Identity inferred:** the user's board was bought as Hestore 10048120 and both raw files carry that number; the board itself has not been checked against this schematic. The project brief calls the board "ESP32-S3-WROOM-1-N16R8-M" ([[sources/project-brief]]); "-M" is not in either raw document (see [[entities/esp32-s3-wroom-1]]).

**Printed board name (user observation 2026-11-28, physical board, not in raw):** the PCB reads **"ESP32-S3-N16R8"**. It does **not** say "YD-ESP32-S3-COREBOARD" and has **no "-M" suffix**. This neither confirms nor contradicts the schematic match *(neutral)*. The identity stays **inferred**, resting on the shared Hestore number 10048120 plus the observations consistent with the schematic: empty "IN-OUT" bridge, USB-C labels "COM" / "USB", CH343 console on `/dev/ttyACM0` (see Connectors and Power path).

## Connectors (schematic p2)
- **USB1 = UART port** via CH343P (U2) -> UART0 (U0TXD/U0RXD = GPIO43/44); RTS/DTR auto-program circuit to CHIP_PU and GPIO0. The CH343P is powered only from USB1. This is the port used on the bench and for `/dev/ttyACM0` (bench power "PC USB on the CH343 UART port only", user 2026-11-27, [[concepts/firmware-architecture]] section 4).
- **USB2 = native USB** of the ESP32-S3 (GPIO19 D−, GPIO20 D+).
- Which physical connector carries which silkscreen label is **not stated** in the schematic. **User observation 2026-11-28 (physical board, not in raw):** the two USB-C connectors are labelled **"COM"** and **"USB"**. Mapping **COM = USB1 (CH343P UART)**, **USB = USB2 (native, GPIO19/20)** *(inferred from the labels and the schematic)*; consistent with `/dev/ttyACM0` being the CH343 console used so far ([[concepts/dev-environment]]).
- **J1 / J2** 22-pin headers; full pin lists in [[sources/yd-esp32-s3-coreboard-schematic]]. Relevant: J1-1/2 VDD33 (3V3 out), J1-3 CHIP_PU, **J1-4/5/6 = GPIO4/5/6** (bus RX / TX / DE), J1-21 "5V" (input, see below), J1-22 GND; J2-2/3 U0TXD/U0RXD, J2-14 GPIO0, J2-16 GPIO48, J2-19/20 GPIO20/19, J2-1/21/22 GND.

## Power path (schematic p2)
- USB1 VBUS -> D1 "D26" -> 5V net; USB2 VBUS -> D2 "D26" -> 5V net (D2 bridged by 0 Ω "USB-OTG"); J1-21 -> D3 "D26" -> 5V net (D3 bridged by 0 Ω "IN-OUT"). All diode cathodes on 5V.
- 5V -> U3 AMS1117-3.3 (10 µF in, 22 µF out) -> VDD33 (module, J1-1/2, CH343P VIO, LEDs, WS2812). Red PWR LED with 680 Ω on VDD33.
- **J1-21 is an input only**: with "IN-OUT" open, D3 blocks the board's 5V net from the pin, so the pin shows no 5 V when the board is powered from USB *(inferred from the diode direction)*.
- **5 V pin observation explained (inferred):** the user's report (bench 2026-11-27) that the 5 V pin "seems not working" while powered from the UART USB port fits an unpopulated "IN-OUT" link. **Correction:** the earlier wiki hypothesis "the 5 V pin is only fed from the other USB connector" is wrong per the schematic: neither USB port feeds J1-21 through D3. ~~To confirm on the board: is the "IN-OUT" 0 Ω pad empty?~~ **Confirmed by user observation 2026-11-28 (physical board, not in raw):** the "IN-OUT" bridge on the front of the board is **empty** (user: "seems to be an empty bridge there", labelled "inout"). Together with the schematic this explains the observation: J1-21 is input-only behind D3 and is not fed from either USB port. (Still *inferred*: that the user's board is this YD-ESP32-S3-COREBOARD.)
- **Consequence:** the header 5 V pin cannot power the MAX485 module unless the "IN-OUT" bridge is closed (soldering; the user cannot solder, 2026-11-28). The module stays on the 3V3 pin, as already decided (OS4, [[concepts/firmware-architecture]] section 4).

## On-board peripherals (schematic p2)
- WS2812B RGB LED on **GPIO48** via 0 Ω "RGB".
- BOOT switch GPIO0 -> GND + 100 nF (no external pull-up; the module's weak pull-up applies, [[sources/esp32-s3-wroom-1-datasheet]] PDF p14). RST switch CHIP_PU -> GND + 100 nF; EN 10 kΩ / 1 µF.
- TX1 / RX1 blue LEDs on U0TXD / U0RXD (680 Ω to VDD33).
- No other GPIO pull resistors; GPIO4/5/6 go only to J1-4/5/6, so the bus defaults TX 5 / RX 4 / DE 6 have no on-board load. Their reset state (high-Z, no internal pulls) is external, user-reported and unverified (2026-11-28), see [[entities/esp32-s3-wroom-1]].

## Hazards
- GPIO35/36/37 are on J2-13/12/11 but are wired to the octal PSRAM on an R8 module and must not be used ([[sources/esp32-s3-wroom-1-datasheet]] PDF p13 fn b).
- GPIOs are not 5 V tolerant *(inferred from V_IH max, [[sources/esp32-s3-wroom-1-datasheet]] PDF p31)*.

## Open questions
- ~~Is the "IN-OUT" 0 Ω link populated?~~ Answered 2026-11-28: empty (user). Is "USB-OTG" populated? (not asked)
- ~~Which physical connector is USB1 vs USB2, and their labels?~~ Answered 2026-11-28: "COM" / "USB" (user); mapping inferred, see Connectors.
- Board identity and revision (schematic revision "*") are not confirmed on the board. ~~The board name printed on the PCB is unknown to the user (2026-11-28, low priority).~~ Answered 2026-11-28: "ESP32-S3-N16R8" (user); no "YD-ESP32-S3-COREBOARD", no "-M". Identity remains inferred (see top).
- Not stated: D26 diode type / drop, AMS1117 current, 3V3 pin budget, J1-21 input voltage range, board dimensions / mounting holes.
