# RESUME

Update after EVERY step. Keep short.

## Project
Matter-over-Wi-Fi thermostat on ESP32-S3-WROOM-1-N16R8-M + MAX485-M, joining the Daikin EKWHCTRL1 <-> INN-FR-B32 Modbus RTU bus (new node = **second master**) without disturbing it. Knowledge in `Knowledgebase/wiki/` (agent-maintained, start at `index.md`), sources in `Knowledgebase/raw/`. Rules: root `CLAUDE.md`. Agents: `.claude/agents/README.md`.

## Done so far
- Wiki: 2 Daikin Modbus manuals, project brief, MAX485 chip datasheet ingested; sync + lint clean (fixes applied).
- Dev env: ESP-IDF v6.0.2 at `~/esp/esp-idf-v6.0.2` (activate: `. ~/esp/esp-idf-v6.0.2/export.sh`); skeleton `firmware/` builds for esp32s3 (components only from the ESP registry via IDF component manager).
- Agents in `.claude/agents/` (default agent = `orchestrator`, set in `.claude/settings.json`): `orchestrator` (opus, routes all work, can run single steps), `wiki-keeper` (sole wiki writer), `datasheet-reader` (read-only PDF facts), `architect` (opus; design needs user approval, then task specs in `tasks/`), `firmware-dev` (sonnet/haiku; readable, testable code + unit tests), `build-verifier` (haiku), `firmware-reviewer` (sonnet, independent). `hw-bus-reviewer` undecided.
- Everything committed and pushed to `origin/main` (git clean).

## Status / gates
- No task specs/firmware work before: architecture approved by user AND bus-topology questions answered.
- Not yet exercised: no agent has been run; no `tasks/` dir yet. Orchestrator default applies on a NEW session start.
- Flashing path (usbipd vs Windows) deferred: no board attached.
- Out of scope now: scheduling.
- Open questions (wiki `concepts/bus-topology`, `entities/max485`): INN-FR-B32 location (brief: in fancoil; manuals: in wall panel); who is master today and where the node taps in; MAX485-M bare chip or module (termination, 5 V vs 3.3 V RX level shift, DE/RE); terminal label +AB- vs +BA-; STAT bit positions unverified (manual PDF p5, `datasheet-reader`).

## Next
1. Orchestrator asks the user the bus-topology questions; `wiki-keeper` records answers (optional: `datasheet-reader` verifies STAT bits; user may add MAX485-M module doc and ESP32-S3 datasheet to `raw/` -> capture).
2. Brainstorm SW architecture (`/brainstorming`) via `architect`: layers, second-master arbitration, Modbus<->Matter mapping (add `espressif/esp_matter`), test strategy. User approves -> `wiki-keeper` records -> task specs.
3. Implement per spec: `firmware-dev` -> `build-verifier` + `firmware-reviewer`. Decide on `hw-bus-reviewer` later.

## Start prompt (copy/paste into a fresh session in this repo; orchestrator is the default agent, else run `claude --agent orchestrator`)
```
Read RESUME.md and CLAUDE.md, then continue the DaikinFancoilMatterThermostat project from "Next". Use the wiki (Knowledgebase/wiki/index.md) as the source of truth, ask me the open bus-topology questions, and update RESUME.md after every step.
```
