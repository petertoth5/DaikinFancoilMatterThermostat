# RESUME

Update after EVERY step. Keep short.

## Project
Matter-over-Wi-Fi thermostat on ESP32-S3-WROOM-1-N16R8-M + MAX485-M, joining the Daikin EKWHCTRL1 <-> INN-FR-B32 Modbus RTU bus without disturbing it. Knowledge in `Knowledgebase/wiki/` (agent-maintained, start at `index.md`), sources in `Knowledgebase/raw/`. Rules: root `CLAUDE.md`.

## Last step
Knowledge agents implemented in `.claude/agents/`: `wiki-keeper` (sole wiki writer) and `datasheet-reader` (read-only PDF fact extraction); `agents/README.md` updated. Previous work committed/pushed (ac91d77); committed and pushed.

## Status
- Rules: ESP-IDF, esp32s3, components ONLY via IDF component manager from the ESP registry.
- Flashing path (usbipd vs Windows) deferred: no board attached.
- Out of scope now: SW architecture (brainstorm later), scheduling.
- Open: INN-FR-B32 location conflict (brief: in fancoil; manuals: in wall panel); +AB- vs +BA- terminal label; bus master/tap point and MAX485-M module details (datasheet covers the bare chip only; `concepts/bus-topology`, `entities/max485`).

## Next
1. Brainstorm and refine the development/architecture agents (candidates: hw-bus-reviewer, registry-guard, firmware-dev, build-verifier; maybe matter-architect), plus related skills/rules (registry-only rule, `idf-env` skill).
2. Answer open questions (INN-FR-B32 location, bus master/tap point, MAX485-M module details); optionally have `datasheet-reader` verify STAT bit positions (manual PDF p5).
3. Later: `/brainstorming` for SW architecture (add `espressif/esp_matter` then); scheduling.

## Start prompt (copy/paste into a fresh session)
```
Read RESUME.md and CLAUDE.md, then continue the DaikinFancoilMatterThermostat project from "Next". Use the wiki (Knowledgebase/wiki/index.md) as the source of truth, ask me about open questions, and update RESUME.md after every step.
```
