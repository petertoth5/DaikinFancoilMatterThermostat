---
title: Project goal and scope
type: concept
sources: ["project-brief.md"]
updated: 2026-11-02
---

# Project goal and scope

Build a **Matter thermostat over Wi-Fi** on an ESP32-S3 that talks Modbus RTU to the Daikin EKWHCTRL1 wall control and the INN-FR-B32 fancoil board ([[sources/project-brief]]).

- Must be discoverable by any Matter controller/hub (examples named: Aeotec SmartThings hub, Apple TV 4K).
- Hardware: [[entities/esp32-s3-wroom-1]], [[entities/max485]]; bus: [[concepts/bus-topology]].
- Tooling: [[concepts/dev-environment]].
- **In scope now:** dev environment, agent definition (next step). **Deferred:** SW architecture/layers (brainstorm later), scheduling.
- Daikin protocol reference: [[concepts/modbus-rs485-link]], [[concepts/register-map]].

## Decisions (user, session 2026-11-02; not in raw)
- Matter device type and Modbus<->Matter attribute mapping are decided by the `architect` agent and must be approved by the user.
- Gate: no firmware task specs before the architecture is approved and the bus-topology questions are answered.
- Workflow, agents and roles: see `.claude/agents/README.md` (orchestrator is the default agent).

## Open questions (gaps, to ask the user)
See "Open questions" in [[concepts/bus-topology]] and [[concepts/dev-environment]].
