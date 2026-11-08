# RESUME

Update after EVERY step. Keep short.

## Project
Matter-over-Wi-Fi thermostat on ESP32-S3-WROOM-1-N16R8-M + MAX485-M breakout, connecting to the Daikin EKWHCTRL1 **"A A B B -" Modbus RTU port** (nothing else connected there). Node = **sole Modbus master** (FC03 polling, FC06 only on Matter commands); staged phasing: 1 on-site read probe -> 2 Matter read-only mirror -> 3 Matter writes (write code compiled out until 3). User decisions 2026-11-08. Knowledge in `Knowledgebase/wiki/` (agent-maintained, start at `index.md`), sources in `Knowledgebase/raw/`. Rules: root `CLAUDE.md`. Agents: `.claude/agents/README.md`.

## Last step (2026-11-09)
Task 001 (skeleton, Kconfig `DMT_*`, bus-access lint R1-R8 + self-test on every build, write-gate, `-Werror`, linux host-test runner) **accepted** (`firmware-reviewer` after two rounds; `build-verifier` all checks pass) and committed and pushed in the session-end commit, together with the spec 001 amendment and the wiki/RESUME .md updates. Untracked `.vscode/` and `ProjectDefinitionPrompt.txt` deliberately not committed.

## Status / gates
- **Gate OPEN for phase 1.** 001 done; specs 009, 010, 002, 006, 011, 008 ready; 003/004/005/007 retired. Stage 2/3 specs only after the probe log is in `raw/` and captured.
- Open minor: lint self-test lacks explicit cases for `^= -= *= /= %= >>=` (lint catches them); small follow-up with task 009 or later.
- User actions: `sudo apt install libbsd-dev` (linux host target); on-site items: MAX485-M U1/supply/RO level, GPIO pins, termination/bias, panel low-voltage isolation (wiki `concepts/bus-topology`, architecture section 8).
- Dev env: ESP-IDF v6.0.2 (`. ~/esp/esp-idf-v6.0.2/export.sh`); host target via `idf.py --preview set-target linux`; esp-clang/clang-tidy not installed. Flashing path deferred.

## Next
1. Task 009 `daikin_regs` (`firmware-dev`, sonnet) -> `build-verifier` + `firmware-reviewer` in parallel -> findings -> user.
2. Then 010, 002, 006, 011, 008 per `tasks/README.md`.
3. User runs the on-site probe (008); log to `Knowledgebase/raw/` -> `/capture`.
4. `architect` writes phase-2 specs.

## Start prompt (copy/paste into a fresh session in this repo; orchestrator is the default agent, else run `claude --agent orchestrator`)
```
Read RESUME.md and CLAUDE.md, then continue the DaikinFancoilMatterThermostat project from "Next". Use the wiki (Knowledgebase/wiki/index.md) as the source of truth, ask me any open decisions (D1-D22 of v0.3 are decided, see Knowledgebase/wiki/concepts/firmware-architecture.md), and update RESUME.md after every step.
```
