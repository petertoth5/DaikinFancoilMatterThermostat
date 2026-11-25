# RESUME

Update after EVERY step. Keep short.

## Project
Matter-over-Wi-Fi thermostat on ESP32-S3-WROOM-1-N16R8-M + MAX485-M breakout, connecting to the Daikin EKWHCTRL1 **"A A B B -" Modbus RTU port** (nothing else connected there). Node = **sole Modbus master** (FC03 polling, FC06 only on Matter commands); staged phasing: 1 on-site read probe -> 2 Matter read-only mirror -> 3 Matter writes (write code compiled out until 3). User decisions 2026-11-08. Knowledge in `Knowledgebase/wiki/` (agent-maintained, start at `index.md`), sources in `Knowledgebase/raw/`. Rules: root `CLAUDE.md`. Agents: `.claude/agents/README.md`.

## Last step (2026-11-25)
- Task 015 (Matter skeleton, `firmware/matter/`) implemented + build-verified (orchestrator + `build-verifier`); **not reviewed, not committed**. Step 0 passed (esp_matter 1.6.1 registry-only on IDF v6.0.2, offline from lock). Build: `idf.py reconfigure` then `ninja -C build -j2` (~26 min clean). Image 1.73 MB = 41 % of 4 MB factory partition. Bus guard `tools/check_no_bus.py` proven.
- Decision **2a-10** (user, "Accept + limit"): esp_matter NONVOLATILE setpoints/SystemMode kept; 016 updates only on change + `attribute::set_deferred_persistence`; architect adds flash-wear estimate to spec 016. Recorded in wiki (firmware-architecture sections 3, 6, 7, 8).

## Working mode (user decisions S1–S5, 2026-11-24)
- S1: wiki updated once per task, at acceptance (decisions and verified facts only); step progress goes to this file and git. Update this file after every step.
- S2: keep the wiki slim (done 2026-11-24: diary in `concepts/firmware-history.md`).
- S3: full independent `firmware-reviewer` only for bus-touching code (bus timing, polling, writes, UART/DE). Other code (Matter, Wi-Fi, UI): `build-verifier` + one review at feature end. Text-only fixes: no re-review loop.
- S4: fewer, bigger tasks, short specs (one per feature); `firmware-dev` on sonnet, haiku for small edits.
- S5: orchestrator batches open questions once per task; recommended defaults for minor items.

## Status / gates
- **Phase 1 done**; probe image built, **not flashed**; bench run pending by the user.
- **Phase 2a:** 015 (2a-A skeleton) build-verified; 016 (2a-B sim model + provisional read-only EP1 mapping + sim console) spec being amended by the `architect`. Plan: firmware-architecture section 6 "Phase-2a plan" 2a-1..10.
- Open 015: on-device D1–D7 (QR print, BLE commissioning, attribute reads, write rejection, rejoin after reboot, factory reset GPIO0 5 s, HA + Apple Home); N16R8 flash/octal PSRAM settings inferred until hardware boot.
- User action: `sudo apt install libbsd-dev` (phase-1 host tests 139/0/0 could not re-run); prepare `python-matter-server` + IPv6 on the NAS.
- Phase 2b (real mapping, bus, Q-HW2) waits for the probe log in `raw/` + `/capture`; settle DE re-check method first.
- Dev env: ESP-IDF v6.0.2 (`. ~/esp/esp-idf-v6.0.2/export.sh`). Untracked `.vscode/`, `ProjectDefinitionPrompt.txt` stay uncommitted.
- Tooling note: subagents' read-only Bash calls are sometimes blocked after the RTK hook rewrite; fall back to the Read tool.

## Next
1. `architect` finishes spec 016 amendment (2a-10, flash-wear estimate) -> `firmware-dev` (sonnet) implements 016 -> `build-verifier` -> one `firmware-reviewer` pass over 015+016 -> user commit + acceptance (wiki update per S1).
2. In parallel: user bench run (`docs/phase1-probe/checklist.md` section B), on-device D1–D7 once a board is attached.
3. On-site C/D; probe log -> `Knowledgebase/raw/` + `/capture` -> phase 2b.

## Start prompt (copy/paste into a fresh session in this repo; orchestrator is the default agent, else run `claude --agent orchestrator`)
```
Read RESUME.md and CLAUDE.md, then continue the DaikinFancoilMatterThermostat project from "Next". Use the wiki (Knowledgebase/wiki/index.md) as the source of truth, ask me any open decisions (D1-D22 of v0.3 and phase-2a 2a-1..10 are decided, see Knowledgebase/wiki/concepts/firmware-architecture.md), and update RESUME.md after every step.
```
