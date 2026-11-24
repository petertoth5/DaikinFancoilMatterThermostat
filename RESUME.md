# RESUME

Update after EVERY step. Keep short.

## Project
Matter-over-Wi-Fi thermostat on ESP32-S3-WROOM-1-N16R8-M + MAX485-M breakout, connecting to the Daikin EKWHCTRL1 **"A A B B -" Modbus RTU port** (nothing else connected there). Node = **sole Modbus master** (FC03 polling, FC06 only on Matter commands); staged phasing: 1 on-site read probe -> 2 Matter read-only mirror -> 3 Matter writes (write code compiled out until 3). User decisions 2026-11-08. Knowledge in `Knowledgebase/wiki/` (agent-maintained, start at `index.md`), sources in `Knowledgebase/raw/`. Rules: root `CLAUDE.md`. Agents: `.claude/agents/README.md`.

## Last step (2026-11-24)
User decisions recorded in wiki (`concepts/firmware-architecture` section 6 "Task 002 spike decisions", sections 2/3/7/8): E2 confirmed, **E1b (esp-modbus, invalid reply = BAD_FRAME) kept as unbuilt fallback** behind the `mb_link.h` seam (revert = separate user decision); gap bytes counted+discarded; `mb_link_get_counters` in `mb status`; exc 0x00 = BAD_FRAME; no fake-port `mb_link` test; FREERTOS_HZ stays 100; no slave simulator; agent defs corrected to sole master. Architect wrote specs **012** (`mb_rtu`), **013** (lint/ELF), **014** (`rs485_port` + `mb_link`); 002 retired; 001/006/008/010/README amended.
Earlier: task 010 committed and pushed (`53e2c49`, `6623c43`).

## Status / gates
- **Gate OPEN for phase 1.** Done: 001, 009, 010. Order: 012 -> 013 -> 014 -> 006 -> 008; 011 any time; all sonnet; independent firmware-reviewer review for 012/013/014. Stage 2/3 specs only after the probe log is in `raw/` and captured.
- Status: uncommitted = `docs/`, `tasks/` (012-014 new, 001/002/006/008/010/README changed), wiki (`log.md`, `index.md`, `concepts/firmware-architecture.md`, `dev-environment.md`, `project-goal.md`), RESUME.md, `.claude/agents/architect.md` + `orchestrator.md`.
- Process note: `firmware-dev` must not edit RESUME.md (it did so during task 009; flagged by firmware-reviewer).
- Open minors: (a) lint self-test still lacks explicit cases for `^= -= *= /= %= >>=` (scheduled in 013); (b) task 009 test comment on `(0xFFFF,1)` says "not a wrap" but with the `>=` guard it is rejected by the wrap guard — cosmetic, low priority; (c) host build prints 4 CMake `component_validation` warnings because `firmware/test/host/main/CMakeLists.txt` globs component `test_host` sources into main (pre-existing pattern from 009, cosmetic).
- Known, not a bug: `mb poll off` then `mb poll on` from OFFLINE restarts polling immediately (U4 consequence, bounded by the 5-failure re-entry); no change made.
- User actions: `sudo apt install libbsd-dev` (linux host target, until then use `export CPATH="/home/szpikko/esp-host-stubs:$CPATH"`); on-site items per `concepts/bus-topology` section 8 and task 008: MAX485-M U1/supply/RO level, GPIO pins, termination/bias, panel low-voltage isolation.
- Dev env: ESP-IDF v6.0.2 (`. ~/esp/esp-idf-v6.0.2/export.sh`); host target via `idf.py --preview set-target linux` (`--preview` needed); esp-clang/clang-tidy not installed.

## Next
1. On user request commit specs/wiki/docs/agents; then implement 012 (firmware-dev, sonnet) -> build-verifier + firmware-reviewer.
2. Then 013, 014 (bus-touching, DE checklist), 006, 011, 008 per `tasks/README.md`.
3. User runs the on-site probe (008); log to `Knowledgebase/raw/` -> `/capture`.
4. `architect` writes phase-2 specs.

## Start prompt (copy/paste into a fresh session in this repo; orchestrator is the default agent, else run `claude --agent orchestrator`)
```
Read RESUME.md and CLAUDE.md, then continue the DaikinFancoilMatterThermostat project from "Next". Use the wiki (Knowledgebase/wiki/index.md) as the source of truth, ask me any open decisions (D1-D22 of v0.3 are decided, see Knowledgebase/wiki/concepts/firmware-architecture.md), and update RESUME.md after every step.
```
