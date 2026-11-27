---
title: ESP32-S3-WROOM-1-N16R8-M
type: entity
sources: ["project-brief.md", "MAX485-M_EN_10035508.pdf"]
updated: 2026-11-27
---

# ESP32-S3-WROOM-1-N16R8-M

Target board/module for the Matter thermostat ([[sources/project-brief]]). Dev environment: ESP-IDF target `esp32s3` ([[concepts/dev-environment]]). Connects to the bus via [[entities/max485]]; see [[concepts/bus-topology]].

Variant details (N16R8 = 16 MB flash / 8 MB PSRAM, inferred from the part number; no datasheet in raw yet — add one to `raw/`).

**Confirmed on the user's board** (bench observation 2026-11-25, user + orchestrator log reading of the boot log; not in raw): ESP32-S3 (QFN56) rev v0.2, Wi-Fi + BT5 LE; `SPI Flash Size : 16MB` (DIO, 80 MHz, chip "boya"); octal PSRAM AP Memory 64 Mbit 3 V, `Found 8MB PSRAM device`, memory test OK, 8192K pool added; console UART0 on GPIO 43/44; CPU 160 MHz (default). The firmware's flash / PSRAM settings match the hardware ([[concepts/firmware-architecture]] section 7 "Phase-2a on-device run").

**Phase-1 bench run 2026-11-27** (user, not in raw): bus on UART1 (TX GPIO5, RX GPIO4, DE GPIO6), console UART0 GPIO 43/44 via the board's CH343 USB-UART; the board's 5 V pin "seems not working" (user; unverified whether it is only fed from the other USB connector), so the RS485 module was powered from the 3.3 V pin ([[concepts/firmware-architecture]] sections 4 and 7).

GPIO reservations planned in the firmware (external, unconfirmed without a module datasheet): [[concepts/firmware-architecture]].

Interface note: MAX485 RO is a 5 V-level output ([[sources/max485-datasheet]]); the ESP32-S3 GPIO level is 3.3 V (inferred), so the RX path needs level shifting.
