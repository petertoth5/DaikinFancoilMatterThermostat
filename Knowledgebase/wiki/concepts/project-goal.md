---
title: Project goal and scope
type: concept
sources: ["project-brief.md"]
updated: 2026-11-07
---

# Project goal and scope

Build a **Matter thermostat over Wi-Fi** on an ESP32-S3 that talks Modbus RTU to the Daikin EKWHCTRL1 wall control and the INN-FR-B32 fancoil board ([[sources/project-brief]]).

- Must be discoverable by any Matter controller/hub (examples named: Aeotec SmartThings hub, Apple TV 4K).
- Hardware: [[entities/esp32-s3-wroom-1]], [[entities/max485]]; bus: [[concepts/bus-topology]].
- Tooling: [[concepts/dev-environment]].
- **Done:** dev environment ([[concepts/dev-environment]]), agent definition (`.claude/agents/`), firmware architecture (approved by the user 2026-11-07: phases P1 listen-only -> P2 Matter read-only mirror -> P3 guarded TX, see [[concepts/firmware-architecture]]). **Deferred:** scheduling.
- **Tasks:** `tasks/README.md`; P1 specs 001–008 are "spec ready" (user decision 2026-11-07, not in raw).
- **Node role:** passive monitor; master only on Matter-originated commands (user statement, 2026-11-07), see [[concepts/bus-topology]].
- Daikin protocol reference: [[concepts/modbus-rs485-link]], [[concepts/register-map]].

## Decisions (user, session 2026-11-02; not in raw)
- Matter device type and Modbus<->Matter attribute mapping are decided by the `architect` agent and must be approved by the user.
- Gate: no firmware task specs before the architecture is approved and the bus-topology questions are answered. *(Partly lifted 2026-11-07: architecture approved; P1 specs released as a gate exception (D2); P3 only after the P1 data, see [[concepts/firmware-architecture]].)*
- Workflow, agents and roles: see `.claude/agents/README.md` (orchestrator is the default agent).

## Open questions (gaps, to ask the user)
See "Open questions" in [[concepts/bus-topology]] and [[concepts/dev-environment]].
