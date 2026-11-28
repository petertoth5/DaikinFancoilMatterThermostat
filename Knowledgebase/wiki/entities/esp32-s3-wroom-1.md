---
title: ESP32-S3-WROOM-1-N16R8 module (brief: "ESP32-S3-WROOM-1-N16R8-M")
type: entity
sources: ["project-brief.md", "MAX485-M_EN_10035508.pdf", "ESP32-S3-DEV-N16R8-US_EN_10048120.pdf", "ESP32-S3-DEV-N16R8-US_EN_10048120 (1).pdf"]
updated: 2026-11-28
---

# ESP32-S3-WROOM-1-N16R8 module

Target MCU module for the Matter thermostat ([[sources/project-brief]]), on the dev board [[entities/yd-esp32-s3-coreboard]] (identity *inferred*). Datasheet: [[sources/esp32-s3-wroom-1-datasheet]] (Espressif v1.4). Dev environment: ESP-IDF target `esp32s3` ([[concepts/dev-environment]]). Connects to the bus via [[entities/max485]]; see [[concepts/bus-topology]].

## Name: "-M" suffix (conflict / unknown)
- The project brief names the board "ESP32-S3-WROOM-1-N16R8-M" ([[sources/project-brief]]).
- The module datasheet's ordering code is **ESP32-S3-WROOM-1-N16R8**; "-M" appears in neither raw document ([[sources/esp32-s3-wroom-1-datasheet]] PDF p4 Table 1; [[sources/yd-esp32-s3-coreboard-schematic]] p2, U4 labelled "ESP32-S3-WROOM-1").
- "-M" is probably part of the dev-board / shop name *(inferred)*; not resolved. The page file name is kept.
- Data point (user observation 2026-11-28, physical board, not in raw): the name printed on the PCB is **"ESP32-S3-N16R8"**, without "-M" (and not "YD-ESP32-S3-COREBOARD", see [[entities/yd-esp32-s3-coreboard]]). The brief's "-M" therefore appears neither on the board print nor in raw; conflict left open.

## Variant facts (datasheet, PDF p4 Table 1 and footnotes)
- 16 MB Quad SPI flash + 8 MB Octal SPI PSRAM (in-package); flash max 80 MHz by default; flash > 100 000 P/E cycles, > 20 years retention.
- Ambient **−40…65 °C** (85 °C only with PSRAM ECC, usable PSRAM reduced by 1/16). 18.0 × 25.5 × 3.1 mm, PCB antenna (keep-out PDF p11 / p44).
- Supply VDD33 3.0–3.6 V (3.3 typ), external supply ≥ 0.5 A; Wi-Fi TX peaks up to 355 mA (PDF p30–31).

**Confirmed on the user's board** (bench observation 2026-11-25, user + orchestrator log reading of the boot log; not in raw): ESP32-S3 (QFN56) rev v0.2, Wi-Fi + BT5 LE; `SPI Flash Size : 16MB` (DIO, 80 MHz, chip "boya"); octal PSRAM AP Memory 64 Mbit 3 V, `Found 8MB PSRAM device`, memory test OK, 8192K pool added; console UART0 on GPIO 43/44; CPU 160 MHz (default). The firmware's flash / PSRAM settings match the hardware ([[concepts/firmware-architecture]] section 7 "Phase-2a on-device run"). Matches the datasheet's N16R8 (16 MB + 8 MB octal). DIO in the boot log vs the datasheet's Quad SPI flash is **not a conflict**: Quad is the flash's capability, DIO the configured access mode *(inferred)*.

## GPIO facts relevant to the project (datasheet)
- Strapping: GPIO0 (weak pull-up), GPIO3 (floating), GPIO45, GPIO46 (weak pull-down); boot mode = GPIO0 + GPIO46; GPIO45 has no effect on PSRAM modules (PDF p14–16, p41).
- USB: GPIO19 D−, GPIO20 D+. UART0: GPIO43 TXD0, GPIO44 RXD0; ROM boot messages on UART0 and USB Serial/JTAG (PDF p12–13, p16).
- Not usable: GPIO35/36/37 (octal PSRAM, PDF p13 fn b); GPIO26–34 not module pins (flash on 26–32 *inferred*, PDF p22 / p41).
- GPIO39–42 default to JTAG (MTCK / MTDO / MTDI / MTMS) (PDF p13).
- GPIO4/5/6: RTC/touch/ADC1 functions only, no strapping or USB role (PDF p12).
- Logic levels at 3.3 V: V_IH ≥ 0.75 × VDD, **max VDD + 0.3 V (≈ 3.6 V)**, V_IL ≤ 0.25 × VDD; internal pulls 45 kΩ typ (PDF p31 Table 11). No 5 V-tolerance statement; not 5 V tolerant *(inferred)*.
- **Not stated:** reset / pre-boot pull state of GPIO4/5/6 (series datasheet not in raw). External, user-reported, unverified (user's internet search, 2026-11-28, not in raw): GPIO4/5/6 are high-Z floating during and right after reset (input buffer, output driver, internal pulls disabled); consistent with the D8 / OS2 assumption, still open until the series datasheet is in raw ([[concepts/firmware-architecture]] section 8).

**Phase-1 bench run 2026-11-27** (user, not in raw): bus on UART1 (TX GPIO5, RX GPIO4, DE GPIO6), console UART0 GPIO 43/44 via the board's CH343 USB-UART; the board's 5 V pin "seems not working" (user), so the RS485 module was powered from the 3.3 V pin ([[concepts/firmware-architecture]] sections 4 and 7). **Explained 2026-11-28 (inferred, from the board schematic):** the header "5V" pin J1-21 is an input behind diode D3 and only connects to the board's 5 V net if the 0 Ω "IN-OUT" link is fitted; the earlier note "unverified whether it is only fed from the other USB connector" is superseded (neither USB port feeds it). **Confirmed 2026-11-28 (user observation of the physical board, not in raw):** the "IN-OUT" bridge is empty, so the 5 V pin is not fed from either USB port; the module stays on 3.3 V. Details: [[entities/yd-esp32-s3-coreboard]].

GPIO reservations in the firmware: [[concepts/firmware-architecture]] section 4.

Interface note: MAX485 RO is a 5 V-level output when the module runs at 5 V ([[sources/max485-datasheet]]); 5 V on GPIO4 would exceed V_IH max ≈ 3.6 V ([[sources/esp32-s3-wroom-1-datasheet]] PDF p31), so a 5 V-fed module needs a level shift on RX.
