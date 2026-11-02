# RESUME

Update after EVERY step. Keep short.

## Project
Matter-over-Wi-Fi thermostat on ESP32-S3-WROOM-1-N16R8-M + MAX485-M, joining the Daikin EKWHCTRL1 <-> INN-FR-B32 Modbus RTU bus without disturbing it. Knowledge in `Knowledgebase/wiki/` (agent-maintained, start at `index.md`), sources in `Knowledgebase/raw/`. Rules: root `CLAUDE.md`.

## Last step
Added `orchestrator` agent (opus, entry point; start with `claude --agent orchestrator`) and updated `firmware-dev` (readable, testable code, tests ship with code) and `architect` (test strategy in task specs). Default agent set in `.claude/settings.json` (`"agent": "orchestrator"`). Committed and pushed.

## Status
- Rules: ESP-IDF, esp32s3, components ONLY via IDF component manager from the ESP registry.
- Flashing path (usbipd vs Windows) deferred: no board attached.
- Out of scope now: SW architecture (brainstorm later), scheduling.
- Open: INN-FR-B32 location conflict (brief: in fancoil; manuals: in wall panel); +AB- vs +BA- terminal label; bus master/tap point and MAX485-M module details (datasheet covers the bare chip only; `concepts/bus-topology`, `entities/max485`).

## Next
1. Commit/push the implementation agents. Review their prompts with the user; `hw-bus-reviewer` stays undecided.
2. Answer bus-topology questions (INN-FR-B32 location, who is master today, tap point, MAX485-M module details) - required gate before any task spec; optionally `datasheet-reader` verifies STAT bits (manual PDF p5).
3. Run `architect` to draft the architecture (incl. second-master arbitration and Matter mapping); user approves; `wiki-keeper` records it; then task specs.

## Start prompt (copy/paste into a fresh session; run as `claude --agent orchestrator`)
```
Read RESUME.md and CLAUDE.md, then continue the DaikinFancoilMatterThermostat project from "Next". Use the wiki (Knowledgebase/wiki/index.md) as the source of truth, ask me about open questions, and update RESUME.md after every step.
```
