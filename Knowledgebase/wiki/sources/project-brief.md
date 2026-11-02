---
title: Project brief (user)
type: source
sources: ["project-brief.md"]
updated: 2026-11-02
---

# Project brief

Raw: `Knowledgebase/raw/project-brief.md` (verbatim user prompt, 2026-11-02). Details will follow during development; this is scope only.

## Key facts
- **Goal:** Matter thermostat interfacing the Daikin EKWHCTRL1 and the fancoil PCB INN-FR-B32 via Modbus; visible to Matter controllers/hubs (Aeotec SmartThings hub, Apple TV 4K, any Matter hub); Matter device **over Wi-Fi**. → [[concepts/project-goal]]
- **Hardware:** ESP32-S3-WROOM-1-N16R8-M ([[entities/esp32-s3-wroom-1]]); MAX485-M RS485 transceiver ([[entities/max485]]); existing bus has one EKWHCTRL1 ([[entities/ekwhctrl1]]) and one fancoil board ([[entities/inn-fr-b32]]), directly connected. The new device is added to the bus **without interrupting** the existing communication. → [[concepts/bus-topology]]
- **Dev environment:** ESP-IDF; all modules only via the IDF component manager and only from the ESP component registry. → [[concepts/dev-environment]]
- **Out of scope for now:** SW layers / deep architecture (to be brainstormed in later sessions with `/brainstorming`), scheduling (later).
- **Next step requested:** agent definition (`.claude/agents/`); new skills/rules may be identified in parallel. → [[concepts/project-goal]]
