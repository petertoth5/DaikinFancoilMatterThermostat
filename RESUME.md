# RESUME

Update after EVERY step. Keep short.

## Project
Matter-over-Wi-Fi thermostat on ESP32-S3-WROOM-1-N16R8-M + MAX485-M breakout, connecting to the Daikin EKWHCTRL1 **"A A B B -" Modbus RTU port** (nothing else connected there). Node = **sole Modbus master** (FC03 polling, FC06 only on Matter commands); staged phasing: 1 on-site read probe -> 2 Matter read-only mirror -> 3 Matter writes (write code compiled out until 3). User decisions 2026-11-08. Knowledge in `Knowledgebase/wiki/` (agent-maintained, start at `index.md`), sources in `Knowledgebase/raw/`. Rules: root `CLAUDE.md`. Agents: `.claude/agents/README.md`.

## Last step (2026-11-24)
- Task 011 gap major fixed (gap enforced in `fake_link` on every request, asserted per scenario; new e2e 11a/11b, extra scope beyond spec 011, spec not amended). `build-verifier` PASS (host 139/0/0, 0 warnings); `firmware-reviewer` re-review APPROVE (new minor m7: 11b loop has no iteration cap).
- User accepted 011 as is; code committed as `17825cb` (push after the docs commit).
- User approved spec 008; `firmware-dev` (sonnet) is writing `docs/phase1-probe/checklist.md` and `docs/phase1-probe/measurement-log-template.md`.
- Wiki updated (firmware-architecture section 6 "Task 011 acceptance", full 8-deviation list, "Spec 008 approval"; sections 7/8; project-goal, index, log). Docs not committed.

## Status / gates
- **Gate OPEN for phase 1.** Done, committed and pushed: 001, 009, 010, 012, 013 (+ follow-up `9ca4517`), 014 (`987fbd1`, docs `4752afd`), 006 (`b8ad00d`). Probe image builds; not flashed yet.
- **011 done**, committed `17825cb` (not yet pushed; push after the docs commit).
- **008 spec approved 2026-11-24; docs in progress** (`firmware-dev`), then `firmware-reviewer`.
- Open: phase-2 DE re-check method without instrument (section 8, before phase-2 specs); basis of the "~25 ms" in F2 not recorded in wiki (ask architect); 011 minors m2–m7 (m5 matters when the 008 log becomes a fixture: `FAKE_REPLAY_MAX` 512, replay keys on addr/reg/qty); 013 doc minors (firmware-architecture section 6 row A14).
- To verify on site: `bus_task` 4 KB stack (high-water after boot and after `mb silence 1`).
- Stage 2/3 specs only after the probe log is in `raw/` and captured.
- Dev env: ESP-IDF v6.0.2 (`. ~/esp/esp-idf-v6.0.2/export.sh`); host target via `idf.py --preview set-target linux`; `libbsd-dev` user action pending (or `export CPATH="/home/szpikko/esp-host-stubs:$CPATH"`).
- Untracked `.vscode/` and `ProjectDefinitionPrompt.txt` stay uncommitted.
- Tooling note: subagents' read-only Bash calls are sometimes blocked after the RTK hook rewrites them (`rtk` prefix); agents fall back to the Read tool.

## Next
1. 008 docs (`firmware-dev`) -> `firmware-reviewer` -> user acceptance -> commit.
2. Flash the probe image; bench items (DE/turnaround "not measured"; hard stop on averaged DE near 3.3 V during `mb probe`).
3. On-site probe (`dur_ms` + derived bound; `bus_task` stack high-water).
4. Probe log into `Knowledgebase/raw/`, then `/capture`; then `architect` writes phase-2 specs.

## Start prompt (copy/paste into a fresh session in this repo; orchestrator is the default agent, else run `claude --agent orchestrator`)
```
Read RESUME.md and CLAUDE.md, then continue the DaikinFancoilMatterThermostat project from "Next". Use the wiki (Knowledgebase/wiki/index.md) as the source of truth, ask me any open decisions (D1-D22 of v0.3 are decided, see Knowledgebase/wiki/concepts/firmware-architecture.md), and update RESUME.md after every step.
```
