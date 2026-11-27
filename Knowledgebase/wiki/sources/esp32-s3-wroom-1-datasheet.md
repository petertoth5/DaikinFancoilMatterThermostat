---
title: ESP32-S3-WROOM-1 / WROOM-1U module datasheet v1.4 (Espressif, via Hestore)
type: source
sources: ["ESP32-S3-DEV-N16R8-US_EN_10048120.pdf"]
updated: 2026-11-28
---

# ESP32-S3-WROOM-1 / WROOM-1U datasheet v1.4

Raw: `ESP32-S3-DEV-N16R8-US_EN_10048120.pdf` — 53 PDF pages: page 1 is a Hestore cover, the rest is the Espressif "ESP32-S3-WROOM-1 / ESP32-S3-WROOM-1U Datasheet Version 1.4" (revision history PDF p51: v1.4, 2024-11-14). **PDF page = printed page + 1.** Page numbers below are **PDF pages**. It is the **module** datasheet only; it does not describe the dev board (that is [[sources/yd-esp32-s3-coreboard-schematic]], same Hestore number 10048120) and it is not the ESP32-S3 **series** (chip) datasheet.

How read: `datasheet-reader` read it visually (page images), 2026-11-28; the wiki agent spot-checked PDF p13 (Table 3 end + footnotes) as an image on 2026-11-28.

A sibling raw file `ESP32-S3-DEV-N16R8-US_EN_10048120.pdf:Zone.Identifier` is a Windows download marker, not a source.

## Ordering code / variant (PDF p4, Table 1)
- **ESP32-S3-WROOM-1-N16R8**: 16 MB **Quad SPI** flash + 8 MB **Octal SPI** PSRAM, ambient −40…65 °C, 18.0 × 25.5 × 3.1 mm.
- Footnotes: PSRAM is in the chip package (fn5); flash max 80 MHz by default (fn3); flash > 100 000 program/erase cycles, > 20 years retention (fn4); 85 °C operation possible with PSRAM ECC enabled, usable PSRAM then reduced by 1/16.
- The suffix **"-M"** used in the project brief does not appear in this document (see [[entities/esp32-s3-wroom-1]]).

## Pins (PDF p12–13, Table 3)
- 41 pins (pin 41 = EPAD, GND). TXD0 = GPIO43, RXD0 = GPIO44. IO19 = USB_D−, IO20 = USB_D+.
- Footnote b (PDF p13, image-checked): on modules with Octal SPI PSRAM (ESP32-S3R8 / R16V) **IO35, IO36, IO37 are connected to the PSRAM and not available** for other uses.
- GPIO26–34 are not module pins. That the flash uses GPIO26–32 is *inferred* from PDF p22 ("Interface 4a multiplexed with GPIO26~GPIO32") and the module schematic (PDF p41).
- GPIO39–42 default functions are JTAG MTCK / MTDO / MTDI / MTMS (PDF p13).
- GPIO4 = RTC_GPIO4 / TOUCH4 / ADC1_CH3; GPIO5 = TOUCH5 / ADC1_CH4; GPIO6 = TOUCH6 / ADC1_CH5; no strapping or USB function on these three.

## Strapping and boot (PDF p14–16, p41)
- Strapping pins (Table 4, PDF p14): GPIO0 weak pull-up; GPIO3 floating (no internal pulls, PDF p16 §4.4); GPIO45 weak pull-down; GPIO46 weak pull-down.
- Boot mode set by GPIO0 + GPIO46 (Table 6, PDF p15); strapping hold time t_H min 3 ms (Table 5).
- On PSRAM modules VDD_SPI is fixed by eFuse, GPIO45 has no effect (PDF p41).
- ROM boot messages go to UART0 and USB Serial/JTAG by default (PDF p16 §4.3).
- EN: recommended RC (10 kΩ / 1 µF) on CHIP_PU (peripheral schematic, PDF p43).

## Electrical (PDF p30–31)
- Absolute maximum: only VDD33 −0.3…3.6 V and T_STORE −40…105 °C are given. **No I/O absolute maximum and no 5 V-tolerance statement.**
- Recommended (Table 10): VDD33 3.0 / 3.3 / 3.6 V; current delivered by the external supply min 0.5 A.
- DC characteristics (Table 11, 3.3 V, 25 °C): V_IH 0.75 × VDD … VDD + 0.3 V; V_IL −0.3 … 0.25 × VDD (≈ 0.825 V); V_OH ≥ 0.8 × VDD; V_OL ≤ 0.1 × VDD; I_OH 40 mA typ (PAD_DRIVER = 3); I_OL 28 mA typ; I_IH / I_IL 50 nA max; C_IN 2 pF; R_PU / R_PD 45 kΩ typ.
- GPIOs are **not 5 V tolerant** *(inferred from V_IH max = VDD + 0.3 ≈ 3.6 V)*.
- Wi-Fi TX peak current up to 355 mA (Table 12, 802.11b 1 Mbps at 20.5 dBm).
- PCB antenna keep-out: PDF p11, Fig. 8 on PDF p44.

## Not stated
- Reset / pre-boot pull state of GPIO4/5/6 (needs the ESP32-S3 series datasheet, not in raw).

## Related
[[entities/esp32-s3-wroom-1]], [[entities/yd-esp32-s3-coreboard]], [[concepts/firmware-architecture]] (section 4 GPIO rules, section 8).
