# RESUME

Update after EVERY step. Keep short.

## Project
Matter-over-Wi-Fi thermostat on ESP32-S3-WROOM-1-N16R8-M + MAX485-M breakout, connecting to the Daikin EKWHCTRL1 **"A A B B -" Modbus RTU port** (nothing else connected there). Node = **sole Modbus master** (FC03 polling, FC06 only on Matter commands); staged phasing: 1 on-site read probe -> 2 Matter read-only mirror -> 3 Matter writes (write code compiled out until 3). User decisions 2026-11-08. Knowledge in `Knowledgebase/wiki/` (agent-maintained, start at `index.md`), sources in `Knowledgebase/raw/`. Rules: root `CLAUDE.md`. Agents: `.claude/agents/README.md`.

## Last step (2026-11-25)
- Task 016 (sim_source, mapping, matter_bridge, sim_console, host tests `firmware/matter/test/host` 62/0/0) implemented, feature-end review 015+016 (0 blocker, 2 major, 8 minor) fixed, delta-reviewed, re-verified. Image 42.5 % of factory, check_no_bus pass, phase-1 regression pass (lint 372, host 139/0/0). **016 not committed** (015 = `9dbace6`).
- Wiki updated (firmware-architecture sections 3, 7, 8; dev-environment): CSO NONVOLATILE question closed (no wear), libbsd-dev installed (CPATH stub no longer needed).
## Working mode (user decisions S1–S5, 2026-11-24)
- S1: wiki updated once per task, at acceptance (decisions and verified facts only); step progress goes to this file and git. Update this file after every step.
- S2: keep the wiki slim (done 2026-11-24: diary in `concepts/firmware-history.md`).
- S3: full independent `firmware-reviewer` only for bus-touching code (bus timing, polling, writes, UART/DE). Other code (Matter, Wi-Fi, UI): `build-verifier` + one review at feature end. Text-only fixes: no re-review loop.
- S4: fewer, bigger tasks, short specs (one per feature); `firmware-dev` on sonnet, haiku for small edits.
- S5: orchestrator batches open questions once per task; recommended defaults for minor items.

## Status / gates
- **Phase 1 done**; probe image built, **not flashed**; bench run pending by the user.
- **Phase 2a code done:** 015 committed, 016 verified, awaiting user commit. Provisional items (a) standby exit with stale 233, (b) 247 bit 5 with stale 245/246, STAT both/neither bits: decide with the probe log (firmware-architecture section 8).
- Open on device: 015 D1–D7, 016 O1–O9, NVS free space (`nvs_get_stats`); N16R8 flash/PSRAM settings inferred.
- User action: prepare `python-matter-server` + IPv6 on the NAS.
- Phase 2b (real mapping, bus, Q-HW2) waits for the probe log in `raw/` + `/capture`; settle DE re-check method first. 2b notes in firmware-architecture section 7 "Task 016 verified facts".
- Dev env: ESP-IDF v6.0.2 (`. ~/esp/esp-idf-v6.0.2/export.sh`). Untracked `.vscode/`, `ProjectDefinitionPrompt.txt` stay uncommitted.
- Tooling note: subagents' read-only Bash calls are sometimes blocked after the RTK hook rewrite; fall back to the Read tool.

## Next
1. User commits 016 (code + wiki + RESUME).
2. User bench run (`docs/phase1-probe/checklist.md` section B); Matter checks D1–D7 / O1–O9 once a board is attached.
3. On-site C/D; probe log -> `Knowledgebase/raw/` + `/capture` -> phase 2b.

## Start prompt (copy/paste into a fresh session in this repo; orchestrator is the default agent, else run `claude --agent orchestrator`)
```
Read RESUME.md and CLAUDE.md, then continue the DaikinFancoilMatterThermostat project from "Next". Use the wiki (Knowledgebase/wiki/index.md) as the source of truth, ask me any open decisions (D1-D22 of v0.3 and phase-2a 2a-1..10 are decided, see Knowledgebase/wiki/concepts/firmware-architecture.md), and update RESUME.md after every step.
```
