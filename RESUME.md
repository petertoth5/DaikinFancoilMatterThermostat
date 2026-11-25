# RESUME

Update after EVERY step. Keep short.

## Project
Matter-over-Wi-Fi thermostat on ESP32-S3-WROOM-1-N16R8-M + MAX485-M breakout, connecting to the Daikin EKWHCTRL1 **"A A B B -" Modbus RTU port** (nothing else connected there). Node = **sole Modbus master** (FC03 polling, FC06 only on Matter commands); staged phasing: 1 on-site read probe -> 2 Matter read-only mirror -> 3 Matter writes (write code compiled out until 3). User decisions 2026-11-08. Knowledge in `Knowledgebase/wiki/` (agent-maintained, start at `index.md`), sources in `Knowledgebase/raw/`. Rules: root `CLAUDE.md`. Agents: `.claude/agents/README.md`.

## Last step (2026-11-25)
- First on-device run of the phase-2a Matter image (user + orchestrator log reading): hardware confirmed (16 MB flash, 8 MB octal PSRAM); commissioned via HA Android app into python-matter-server; 015 D1–D5 and 016 O1–O6, O8, O9 pass. Recorded in wiki (firmware-architecture sections 7/8, dev-environment, esp32-s3-wroom-1).
- **New finding:** boot log `wifi:mode : sta + softAP`, visible unknown network, `CONFIG_ENABLE_WIFI_AP` not set. Architect drafting fix spec 017.
## Working mode (user decisions S1–S5, 2026-11-24)
- S1: wiki updated once per task, at acceptance (decisions and verified facts only); step progress goes to this file and git. Update this file after every step.
- S2: keep the wiki slim (done 2026-11-24: diary in `concepts/firmware-history.md`).
- S3: full independent `firmware-reviewer` only for bus-touching code (bus timing, polling, writes, UART/DE). Other code (Matter, Wi-Fi, UI): `build-verifier` + one review at feature end. Text-only fixes: no re-review loop.
- S4: fewer, bigger tasks, short specs (one per feature); `firmware-dev` on sonnet, haiku for small edits.
- S5: orchestrator batches open questions once per task; recommended defaults for minor items.

## Status / gates
- **Phase 1 done**; probe image built, **not flashed**; bench run pending by the user.
- **Phase 2a:** code 015 `9dbace6`, 016 `03dacf5` (pushed); on device 2026-11-25 mostly pass (firmware-architecture section 7 "Phase-2a on-device run"). Open: softAP fix (017); D6 factory reset; D7 Apple Home; O7 SetpointRaiseLower + D4 IM status code (chip-tool or `--log-level debug`); O9 idle 2 min (64 s seen); NVS free space. Provisional (a)/(b)/STAT items decided with the probe log (section 8).
- Phase 2b (real mapping, bus, Q-HW2) waits for the probe log in `raw/` + `/capture`; settle DE re-check method first.
- Dev env: ESP-IDF v6.0.2 (`. ~/esp/esp-idf-v6.0.2/export.sh`); flash via usbipd-win -> `/dev/ttyACM0`. Untracked `.vscode/`, `ProjectDefinitionPrompt.txt` stay uncommitted.
- Tooling note: subagents' read-only Bash calls are sometimes blocked after the RTK hook rewrite; fall back to the Read tool.

## Next
1. User approves spec 017 (softAP) -> `firmware-dev` fix -> build-verify -> re-flash, confirm STA only.
2. Later on device: D6 factory reset, Apple Home (D7), chip-tool O7 + D4 IM status code, O9 idle 2 min.
3. Phase-1 bench checklist section B (`docs/phase1-probe/checklist.md`), then on-site C/D.
4. Probe log -> `Knowledgebase/raw/` + `/capture` -> phase 2b.

## Start prompt (copy/paste into a fresh session in this repo; orchestrator is the default agent, else run `claude --agent orchestrator`)
```
Read RESUME.md and CLAUDE.md, then continue the DaikinFancoilMatterThermostat project from "Next". Use the wiki (Knowledgebase/wiki/index.md) as the source of truth, ask me any open decisions (D1-D22 of v0.3 and phase-2a 2a-1..10 are decided, see Knowledgebase/wiki/concepts/firmware-architecture.md), and update RESUME.md after every step.
```
