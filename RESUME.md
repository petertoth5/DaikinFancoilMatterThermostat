# RESUME

Update after EVERY step. Keep short.

## Project
Matter-over-Wi-Fi thermostat on ESP32-S3-WROOM-1-N16R8-M + MAX485-M breakout, connecting to the Daikin EKWHCTRL1 **"A A B B -" Modbus RTU port** (nothing else connected there). Node = **sole Modbus master** (FC03 polling, FC06 only on Matter commands); staged phasing: 1 on-site read probe -> 2 Matter read-only mirror -> 3 Matter writes (write code compiled out until 3). User decisions 2026-11-08. Knowledge in `Knowledgebase/wiki/` (agent-maintained, start at `index.md`), sources in `Knowledgebase/raw/`. Rules: root `CLAUDE.md`. Agents: `.claude/agents/README.md`.

## Last step (2026-11-24)
Task 010 (`reg_model` + `poll_sched` + `txn_log`) **accepted** after the U1–U6 amendment and reviewer minors. Verified: `build-verifier` PASS (esp32s3 fresh build 0 warnings; host tests 100 Tests, 0 Failures, 0 Ignored, exit 0; bus-access lint + 82-case self-test clean; registry-only deps; clang-tidy skipped, not installed); `firmware-reviewer` APPROVE. Wiki updated: `concepts/firmware-architecture` (section 3 exc-02 scope, section 7), `concepts/project-goal`, `index.md`, `log.md` (2026-11-24). **Not committed.**

## Status / gates
- **Gate OPEN for phase 1.** Done: 001, 009, 010. Next: 002, then 006, 011, 008 per `tasks/README.md`; 003/004/005/007 retired. Stage 2/3 specs only after the probe log is in `raw/` and captured.
- Process note: `firmware-dev` must not edit RESUME.md (it did so during task 009; flagged by firmware-reviewer).
- Open minors: (a) lint self-test still lacks explicit cases for `^= -= *= /= %= >>=` (follow-up with task 009/later); (b) task 009 test comment on `(0xFFFF,1)` says "not a wrap" but with the `>=` guard it is rejected by the wrap guard — cosmetic, low priority; (c) host build prints 4 CMake `component_validation` warnings because `firmware/test/host/main/CMakeLists.txt` globs component `test_host` sources into main (pre-existing pattern from 009, cosmetic).
- Known, not a bug: `mb poll off` then `mb poll on` from OFFLINE restarts polling immediately (U4 consequence, bounded by the 5-failure re-entry); no change made.
- User actions: `sudo apt install libbsd-dev` (linux host target, until then use `export CPATH="/home/szpikko/esp-host-stubs:$CPATH"`); on-site items per `concepts/bus-topology` section 8 and task 008: MAX485-M U1/supply/RO level, GPIO pins, termination/bias, panel low-voltage isolation.
- Dev env: ESP-IDF v6.0.2 (`. ~/esp/esp-idf-v6.0.2/export.sh`); host target via `idf.py --preview set-target linux` (`--preview` needed); esp-clang/clang-tidy not installed.

## Next
1. User: commit task 010 (firmware + spec/wiki/RESUME) on request.
2. Task 002 esp-modbus master spike / `mb_link` (`firmware-dev` sonnet) -> `build-verifier` + `firmware-reviewer` (bus-touching). Spec `tasks/002-esp-modbus-master-spike-mb-link.md` has escalations S3/S5/S6/S12 that stop for a user decision.
3. Then 006, 011, 008 per `tasks/README.md`.
4. User runs the on-site probe (008); log to `Knowledgebase/raw/` -> `/capture`.
5. `architect` writes phase-2 specs.

## Start prompt (copy/paste into a fresh session in this repo; orchestrator is the default agent, else run `claude --agent orchestrator`)
```
Read RESUME.md and CLAUDE.md, then continue the DaikinFancoilMatterThermostat project from "Next". Use the wiki (Knowledgebase/wiki/index.md) as the source of truth, ask me any open decisions (D1-D22 of v0.3 are decided, see Knowledgebase/wiki/concepts/firmware-architecture.md), and update RESUME.md after every step.
```
