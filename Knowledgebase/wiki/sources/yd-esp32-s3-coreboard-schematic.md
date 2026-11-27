---
title: YD-ESP32-S3-COREBOARD schematic (VCC-GND studio, via Hestore)
type: source
sources: ["ESP32-S3-DEV-N16R8-US_EN_10048120 (1).pdf"]
updated: 2026-11-28
---

# YD-ESP32-S3-COREBOARD schematic

Raw: `ESP32-S3-DEV-N16R8-US_EN_10048120 (1).pdf` — 2 PDF pages: p1 Hestore cover, **p2 one-sheet board schematic** "YD-ESP32-S3-COREBOARD", 源地 工作室 / VCC-GND studio, VCC-GND.COM, dated 2022/6/1, revision "*". No text besides the schematic.

How read: `datasheet-reader` read it visually (page image), 2026-11-28; the wiki agent spot-checked p2 as an image on 2026-11-28 (J1/J2 headers, D1–D3, U3, USB-OTG / IN-OUT links).

A sibling raw file `ESP32-S3-DEV-N16R8-US_EN_10048120 (1).pdf:Zone.Identifier` is a Windows download marker, not a source.

**Board identity:** that the user's board is this YD board is *inferred* from the shared Hestore number 10048120 (same number as [[sources/esp32-s3-wroom-1-datasheet]]); not confirmed on the board. The module U4 is labelled only "ESP32-S3-WROOM-1" (no flash/PSRAM suffix).

## Key facts (p2)
- **U2 CH343P USB-UART:** VDD5 and VBUS on net VBUSB (USB1 VBUS), VIO on VDD33; TXD -> U0RXD, RXD -> U0TXD (crossed); D+/D− to USB1; RTS/DTR drive the auto-program circuit (Q1/Q2 SS8050 + 10 kΩ) to CHIP_PU and GPIO0. The CH343P is powered only from USB1.
- **USB1 = UART port** (CH343P): VBUS -> VBUSB -> D1 "D26" -> 5V net; CC 5.1 kΩ.
- **USB2 = native USB** (GPIO20 = D+, GPIO19 = D−): VBUS -> D2 "D26" -> 5V net; D2 bridged by a 0 Ω "USB-OTG" link; CC 5.1 kΩ.
- **5V net** fed by D1, D2 and D3 (all cathodes on 5V). D3 comes from header **J1 pin 21** and is bridged by a 0 Ω "IN-OUT" link. Population of "USB-OTG" and "IN-OUT" is not stated.
- **U3 AMS1117-3.3** from 5V (C4 10 µF in, C3 22 µF out) -> VDD33: module, J1 pins 1–2, CH343P VIO, LED pull-ups, WS2812. PWR red LED with 680 Ω on VDD33.
- **J1-21 ("5V" pin) is an input only, via D3**: the board's 5V net cannot drive it unless "IN-OUT" is fitted *(inferred from the diode direction)*.
- **Header J1:** 1 VDD33, 2 VDD33, 3 CHIP_PU, 4 GPIO4, 5 GPIO5, 6 GPIO6, 7 GPIO7, 8 GPIO15, 9 GPIO16, 10 GPIO17, 11 GPIO18, 12 GPIO8, 13 GPIO3, 14 GPIO46, 15 GPIO9, 16 GPIO10, 17 GPIO11, 18 GPIO12, 19 GPIO13, 20 GPIO14, 21 5V in (via D3 / IN-OUT), 22 GND.
- **Header J2:** 1 GND, 2 U0TXD, 3 U0RXD, 4 GPIO1, 5 GPIO2, 6 GPIO42, 7 GPIO41, 8 GPIO40, 9 GPIO39, 10 GPIO38, 11 GPIO37, 12 GPIO36, 13 GPIO35, 14 GPIO0, 15 GPIO45, 16 GPIO48, 17 GPIO47, 18 GPIO21, 19 GPIO20, 20 GPIO19, 21 GND, 22 GND. Physical side / orientation of J1 and J2 not stated.
- GPIO35/36/37 are broken out on J2 although unusable on an R8 module ([[sources/esp32-s3-wroom-1-datasheet]] PDF p13 fn b).
- **RGB LED** WS2812B (XL-5050RGBC) on GPIO48 via 0 Ω "RGB".
- **BOOT** switch GPIO0 -> GND + 100 nF, no external pull-up. **RST** switch CHIP_PU -> GND + 100 nF; EN 10 kΩ to VDD33 + 1 µF (matches the module datasheet recommendation, [[sources/esp32-s3-wroom-1-datasheet]] PDF p43).
- TX1 / RX1 blue LEDs on U0TXD / U0RXD via 680 Ω to VDD33.
- No other pull resistors on GPIOs; **GPIO4/5/6 connect only to J1 pins 4/5/6**.
- The printed auto-program truth table has an "IO2" column although GPIO2 is not connected to the circuit (drawing quirk).

## Not stated
D26 diode type / forward drop, AMS1117 current rating, 3V3 pin current budget, J1-21 input voltage range, connector silkscreen labels and positions, board dimensions / mounting holes, population of the 0 Ω links.

## Related
[[entities/yd-esp32-s3-coreboard]], [[entities/esp32-s3-wroom-1]], [[concepts/firmware-architecture]] (sections 4 and 8).
