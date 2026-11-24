# RESUME

Update after EVERY step. Keep short.

## Project
Matter-over-Wi-Fi thermostat on ESP32-S3-WROOM-1-N16R8-M + MAX485-M breakout, connecting to the Daikin EKWHCTRL1 **"A A B B -" Modbus RTU port** (nothing else connected there). Node = **sole Modbus master** (FC03 polling, FC06 only on Matter commands); staged phasing: 1 on-site read probe -> 2 Matter read-only mirror -> 3 Matter writes (write code compiled out until 3). User decisions 2026-11-08. Knowledge in `Knowledgebase/wiki/` (agent-maintained, start at `index.md`), sources in `Knowledgebase/raw/`. Rules: root `CLAUDE.md`. Agents: `.claude/agents/README.md`.

## Last step (2026-11-24)
Task 014 (`rs485_port` + `mb_link`) accepted by the user after the M1–M4 fixes (re-checked by `build-verifier` PASS and `firmware-reviewer` APPROVE). Firmware committed and pushed as `987fbd1`. Wiki: firmware-architecture sections 6 ("Task 014 review decisions", "Task 014 acceptance"), 7, 8; project-goal; index; log (`## [2026-11-24] sync | task 014 accepted`).

## Status / gates
- **Gate OPEN for phase 1.** Done, committed and pushed: 001, 009, 010, 012, 013 (+ follow-up `9ca4517`), 014 (`987fbd1`).
- **Open questions for the user (firmware-architecture section 8, answer before task 008 is finalised):**
  - Q-HW1: oscilloscope or logic analyser on the bench or on site? Needed for 008 B.6 (DE) and D.8b (slave turnaround); otherwise logged "not measured".
  - Q-HW2: the phase-1 image cannot generate flash/NVS load, so 008 B.6 "DE with concurrent flash/NVS" is "not done in phase 1, repeat at phase-2 bring-up" unless a test console command is added to 006.
- Open doc minors from the 013 follow-up (firmware-architecture section 6 row A14, not fixed): (a) FIFO rule bypassable with `;` in a comment or string in the `REG_*` argument list; (b) R5 `_second_argument` on 5000x `uart_set_mode(` takes over 60 s; (c) optional self-test for an unterminated `/*`.
- Stage 2/3 specs only after the probe log is in `raw/` and captured.
- Dev env: ESP-IDF v6.0.2 (`. ~/esp/esp-idf-v6.0.2/export.sh`); host target via `idf.py --preview set-target linux`; `libbsd-dev` user action pending (or `export CPATH="/home/szpikko/esp-host-stubs:$CPATH"`).
- Untracked `.vscode/` and `ProjectDefinitionPrompt.txt` stay uncommitted.

## Next
1. 006 (bus_task, probe console, `app_main`; amended spec: mute IDF `uart` log to WARN, failed boot hold-receive fatal, no read before a clean silence check). firmware-dev (sonnet) -> build-verifier -> firmware-reviewer.
2. 011 (fake-slave host tests, `tasks/README.md`); can run in parallel with 1.
3. 008 (on-site probe checklist and log template); needs answers to Q-HW1 and Q-HW2 first.
4. On-site probe by the user (DE scope at boot, open, per transaction, concurrent flash/NVS, silence check, close; slave turnaround; no-slave -> TIMEOUT); log to `Knowledgebase/raw/`, then `/capture`.
5. `architect` writes phase-2 specs after the probe.

## Start prompt (copy/paste into a fresh session in this repo; orchestrator is the default agent, else run `claude --agent orchestrator`)
```
Read RESUME.md and CLAUDE.md, then continue the DaikinFancoilMatterThermostat project from "Next". Use the wiki (Knowledgebase/wiki/index.md) as the source of truth, ask me any open decisions (D1-D22 of v0.3 are decided, see Knowledgebase/wiki/concepts/firmware-architecture.md), and update RESUME.md after every step.
```
