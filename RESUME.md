# RESUME

Update after EVERY step. Keep short.

## Project
Matter-over-Wi-Fi thermostat on ESP32-S3-WROOM-1-N16R8-M + MAX485-M breakout, connecting to the Daikin EKWHCTRL1 **"A A B B -" Modbus RTU port** (nothing else connected there). Node = **sole Modbus master** (FC03 polling, FC06 only on Matter commands); staged phasing: 1 on-site read probe -> 2 Matter read-only mirror -> 3 Matter writes (write code compiled out until 3). User decisions 2026-11-08. Knowledge in `Knowledgebase/wiki/` (agent-maintained, start at `index.md`), sources in `Knowledgebase/raw/`. Rules: root `CLAUDE.md`. Agents: `.claude/agents/README.md`.

## Last step (2026-11-09)
`wiki-keeper` recorded the user's answers in `Knowledgebase/wiki/concepts/firmware-architecture.md`: boot-window console commands run before the first poll; `mb silence <s>` leaves polling off until `mb poll on`; `mb log` default ON from power-up; lint section aligned with R1-R8 of `tasks/001-skeleton-kconfig-lint.md`. In progress: `architect` updating 006/008 for `mb log` default on; `firmware-dev` implementing task 001 (sonnet). Committed and pushed: 2026-11-08/09 wiki work + RESUME as 9087fd3 ("Record A A B B - redesign and approved architecture v0.3 in wiki"); phase-1 specs as ae7e3c9 ("Rewrite phase-1 task specs for the A A B B - sole-master design"; old `tasks/002-esp-modbus-crc-adapter-spike.md` removed). Still uncommitted: `firmware/` (task 001 in progress by `firmware-dev`); untracked `.vscode/`, `ProjectDefinitionPrompt.txt`, `libbsd-dev_0.12.1-1build1.1_amd64.deb` in repo root (origin unclear, not to be committed; user to decide).

## Status / gates
- **Gate OPEN for phase 1.** Specs 001, 009, 010, 002, 006, 011, 008 spec ready; 003/004/005/007 retired. Stage 2/3 specs only after the probe log is in `raw/` and captured.
- Open points: MAX485-M hardware (U1/supply/RO level); termination/bias (unbiased idle may trip the silence check -> back to architect); panel LV-side mains isolation not stated; items in wiki `concepts/bus-topology` and architecture section 8.
- Dev env: ESP-IDF v6.0.2 (`. ~/esp/esp-idf-v6.0.2/export.sh`); `firmware/` skeleton builds (registry components only).
- Flashing path (usbipd vs Windows) deferred; scheduling out of scope.

## Next
1. `build-verifier` + `firmware-reviewer` on 001 in parallel -> findings to `firmware-dev` -> user.
2. Then 009, 010, 002, 006, 011, 008 per `tasks/README.md`.
3. User runs the on-site probe (008); log to `Knowledgebase/raw/` -> `/capture`.
4. `architect` specs phase 2.

## Start prompt (copy/paste into a fresh session in this repo; orchestrator is the default agent, else run `claude --agent orchestrator`)
```
Read RESUME.md and CLAUDE.md, then continue the DaikinFancoilMatterThermostat project from "Next". Use the wiki (Knowledgebase/wiki/index.md) as the source of truth, ask me any open decisions (D1-D22 of v0.3 are decided, see Knowledgebase/wiki/concepts/firmware-architecture.md), and update RESUME.md after every step.
```
