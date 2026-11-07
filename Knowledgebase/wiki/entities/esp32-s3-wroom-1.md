---
title: ESP32-S3-WROOM-1-N16R8-M
type: entity
sources: ["project-brief.md", "MAX485-M_EN_10035508.pdf"]
updated: 2026-11-07
---

# ESP32-S3-WROOM-1-N16R8-M

Target board/module for the Matter thermostat ([[sources/project-brief]]). Dev environment: ESP-IDF target `esp32s3` ([[concepts/dev-environment]]). Connects to the bus via [[entities/max485]]; see [[concepts/bus-topology]].

Variant details (N16R8 = 16 MB flash / 8 MB PSRAM, inferred from the part number; no datasheet in raw yet — add one to `raw/`).

GPIO reservations planned in the firmware (external, unconfirmed without a module datasheet): [[concepts/firmware-architecture]].

Interface note: MAX485 RO is a 5 V-level output ([[sources/max485-datasheet]]); the ESP32-S3 GPIO level is 3.3 V (inferred), so the RX path needs level shifting.
