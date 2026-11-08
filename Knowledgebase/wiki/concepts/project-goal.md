---
title: Project goal and scope
type: concept
sources: ["project-brief.md"]
updated: 2026-11-09
---

# Project goal and scope

Build a **Matter thermostat over Wi-Fi** on an ESP32-S3 that talks Modbus RTU to the Daikin EKWHCTRL1 wall control and the INN-FR-B32 fancoil board ([[sources/project-brief]]).

- Must be discoverable by any Matter controller/hub (examples named: Aeotec SmartThings hub, Apple TV 4K).
- Hardware: [[entities/esp32-s3-wroom-1]], [[entities/max485]]; bus: [[concepts/bus-topology]].
- Tooling: [[concepts/dev-environment]].
- **Done:** dev environment ([[concepts/dev-environment]]), agent definition (`.claude/agents/`), firmware architecture **v0.3, approved by the user 2026-11-08** (Q1–Q10 all answered with the architect's recommendation; session, not in raw): phases 1 on-site read probe -> 2 Matter read-only mirror -> 3 Matter writes behind `CONFIG_DMT_BUS_WRITE`, D1–D22, see [[concepts/firmware-architecture]]. *(v0.2 of 2026-11-07, P1 listen-only on "+ A B -", is superseded; kept as a history section there.)* **Deferred:** scheduling.
- **Redesign done (user decisions 2026-11-08, not in raw):** tap point = EKWHCTRL1 "A A B B -" Modbus RTU port (nothing else connected there); recorded as architecture v0.3 ([[concepts/firmware-architecture]], [[concepts/bus-topology]]).
- **Phasing (user, 2026-11-08, not in raw):** phase 1 on-site read probe (slave address, A/B polarity, which registers answer) -> phase 2 Matter read-only mirror -> phase 3 Matter writes with readback and limits; write code compiled out until phase 3.
- **Tasks:** `tasks/README.md`. 2026-11-08: the `architect` is writing the **stage-1 (probe) specs** per v0.3 section 7 (001, 009, 010, 002, 006, 011, 008; 003/004/005/007 retired). The v0.2 P1 specs 001–008 (2026-11-07) are superseded by that rework. **2026-11-09:** stage-1 specs written and adjusted to the user's stage-1 detail decisions (auto polling after the silence check, normal USB power, nine architect details accepted; [[concepts/firmware-architecture]] section 6); all seven are "spec ready" per `tasks/README.md`. **2026-11-09 (session implementation fact, not in raw):** task **001 done** (skeleton, Kconfig, bus-access lint, host-test runner; accepted by `firmware-reviewer` after two rounds, `build-verifier` all checks pass), committed at session end; next task **009** ([[concepts/firmware-architecture]] section 7).
- **Node role (user, 2026-11-08, not in raw):** **sole Modbus master** on "A A B B -": periodic FC03 polling mirrors panel/fancoil state into Matter; FC06 writes only on Matter commands. *(Supersedes 2026-11-07: "passive monitor; master only on Matter-originated commands".)* See [[concepts/bus-topology]].
- Daikin protocol reference: [[concepts/modbus-rs485-link]], [[concepts/register-map]].

## Decisions (user, session 2026-11-02; not in raw)
- Matter device type and Modbus<->Matter attribute mapping are decided by the `architect` agent and must be approved by the user.
- Gate: no firmware task specs before the architecture is approved and the bus-topology questions are answered. *(Partly lifted 2026-11-07: architecture approved; P1 specs released as a gate exception (D2); P3 only after the P1 data, see [[concepts/firmware-architecture]]. Re-closed 2026-11-08 until v0.3 is approved, per `RESUME.md` status.)* **2026-11-08: v0.3 approved; gate OPEN for stage 1 (phase-1 probe specs) only, as a gate exception (D2, Q8) before the on-site bus-topology items are answered; stage-2/3 specs only after the probe log** (user decision, session, not in raw).
- Workflow, agents and roles: see `.claude/agents/README.md` (orchestrator is the default agent).

## Open questions (gaps, to ask the user)
See "Open questions" in [[concepts/bus-topology]] and [[concepts/dev-environment]].
