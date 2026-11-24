# RESUME

Update after EVERY step. Keep short.

## Project
Matter-over-Wi-Fi thermostat on ESP32-S3-WROOM-1-N16R8-M + MAX485-M breakout, connecting to the Daikin EKWHCTRL1 **"A A B B -" Modbus RTU port** (nothing else connected there). Node = **sole Modbus master** (FC03 polling, FC06 only on Matter commands); staged phasing: 1 on-site read probe -> 2 Matter read-only mirror -> 3 Matter writes (write code compiled out until 3). User decisions 2026-11-08. Knowledge in `Knowledgebase/wiki/` (agent-maintained, start at `index.md`), sources in `Knowledgebase/raw/`. Rules: root `CLAUDE.md`. Agents: `.claude/agents/README.md`.

## Last step (2026-11-24)
- Task 008 accepted by the user: specs 008 + 014 rule 4 amended (leftover "10–20 ms" texts corrected), docs updated (S1–S9, extra `[TRANSMITS]` tags), `firmware-reviewer` re-review APPROVE WITH MINORS, B.4.3/D.9 minors fixed. Docs committed as `4be04e3`.
- Wiki updated (firmware-architecture section 6 "Task 008 acceptance", sections 7, 8; project-goal, index, log). J2/J7 wording questions closed.
- Specs + wiki commit and push follow (`17825cb`, `4be04e3` not yet pushed).

## Status / gates
- **All phase-1 tasks done:** 001, 009, 010, 012, 013, 014, 006, 011, 008. Probe image built, **not flashed**.
- Open: phase-2 DE re-check method (firmware-architecture section 8, settle before phase-2 specs); 008 docs minors (section 6 "Task 008 docs review decisions"); 011 minors m2–m7 (m5 matters when the 008 log becomes a fixture); 013 doc minors (row A14).
- To verify on site: `bus_task` 4 KB stack (high-water after boot and after `mb silence 1`).
- Stage 2/3 specs only after the probe log is in `raw/` and captured.
- Dev env: ESP-IDF v6.0.2 (`. ~/esp/esp-idf-v6.0.2/export.sh`); host target via `idf.py --preview set-target linux`; `libbsd-dev` user action pending (or `export CPATH="/home/szpikko/esp-host-stubs:$CPATH"`).
- Untracked `.vscode/` and `ProjectDefinitionPrompt.txt` stay uncommitted.
- Tooling note: subagents' read-only Bash calls are sometimes blocked after the RTK hook rewrites them (`rtk` prefix); agents fall back to the Read tool.

## Next
1. User flashes the probe image and runs checklist section B (`docs/phase1-probe/checklist.md`) on the bench, recording into `measurement-log-template.md` (B.6.b hard stop: run from STOPPED, read during the sweep).
2. On-site sections C/D (`dur_ms` + derived bound ~`dur_ms` - 34 ms; `bus_task` stack high-water).
3. Probe log into `Knowledgebase/raw/`, then `/capture`.
4. `architect` writes phase-2 specs; settle the phase-2 DE re-check method first.

## Start prompt (copy/paste into a fresh session in this repo; orchestrator is the default agent, else run `claude --agent orchestrator`)
```
Read RESUME.md and CLAUDE.md, then continue the DaikinFancoilMatterThermostat project from "Next". Use the wiki (Knowledgebase/wiki/index.md) as the source of truth, ask me any open decisions (D1-D22 of v0.3 are decided, see Knowledgebase/wiki/concepts/firmware-architecture.md), and update RESUME.md after every step.
```
