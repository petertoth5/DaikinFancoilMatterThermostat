# RESUME

Update after EVERY step. Keep short.

## Project
Matter-over-Wi-Fi thermostat on ESP32-S3-WROOM-1-N16R8-M + MAX485-M, joining the Daikin EKWHCTRL1 <-> INN-FR-B32 Modbus RTU bus without disturbing it. Knowledge in `Knowledgebase/wiki/` (agent-maintained, start at `index.md`), sources in `Knowledgebase/raw/`. Rules: root `CLAUDE.md`.

## Last step
Dev env ready: ESP-IDF v6.0.2 at `~/esp/esp-idf-v6.0.2` (activate: `. ~/esp/esp-idf-v6.0.2/export.sh`), broken old install removed, skeleton `firmware/` builds for esp32s3. Wiki `concepts/dev-environment` updated. Then `/capture` of `raw/MAX485-M_EN_10035508.pdf` (chip datasheet: 5 V supply, RO 5 V-level vs ESP32-S3 3.3 V, pinout) into `sources/max485-datasheet`, `entities/max485`, `concepts/bus-topology`. `/sync` and `/lint` run, lint fixes applied; committed and pushed.

## Status
- Rules: ESP-IDF, esp32s3, components ONLY via IDF component manager from the ESP registry.
- Flashing path (usbipd vs Windows) deferred: no board attached.
- Out of scope now: SW architecture (brainstorm later), scheduling.
- Open: INN-FR-B32 location conflict (brief: in fancoil; manuals: in wall panel); +AB- vs +BA- terminal label; bus master/tap point and MAX485-M module details (datasheet covers the bare chip only; `concepts/bus-topology`, `entities/max485`).

## Next
1. **Agent definition** in `.claude/agents/` (user's requested next step); identify new skills/rules in parallel.
2. Later: `/brainstorming` for SW architecture (add `espressif/esp_matter` then); scheduling.

## Start prompt (copy/paste into a fresh session)
```
Read RESUME.md and CLAUDE.md, then continue the DaikinFancoilMatterThermostat project from "Next". Use the wiki (Knowledgebase/wiki/index.md) as the source of truth, ask me about open questions, and update RESUME.md after every step.
```
