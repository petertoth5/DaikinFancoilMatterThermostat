# RESUME

Update after EVERY step. Keep short.

## Project
Matter-over-Wi-Fi thermostat on ESP32-S3-WROOM-1-N16R8-M + MAX485-M breakout, connecting to the Daikin EKWHCTRL1 **"A A B B -" Modbus RTU port** (nothing else connected there). Node = **sole Modbus master** (FC03 polling, FC06 only on Matter commands); staged phasing: 1 on-site read probe -> 2 Matter read-only mirror -> 3 Matter writes (write code compiled out until 3). User decisions 2026-11-08. Knowledge in `Knowledgebase/wiki/` (agent-maintained, start at `index.md`), sources in `Knowledgebase/raw/`. Rules: root `CLAUDE.md`. Agents: `.claude/agents/README.md`.

## Last step (2026-11-24)
Task 009 `daikin_regs` complete and accepted. `firmware-dev` (sonnet) implemented the pure C99 component at `firmware/components/daikin_regs/` (header API per spec, 32-row register table with write_mask data, host unit tests in `test_host/`); added to `firmware/test/host/main/CMakeLists.txt` REQUIRES. During implementation, firmware-dev found and fixed two pre-existing skeleton bugs from task 001: (1) bounds-macro struct ordering was wrong (every row's bounds were bogus), (2) `daikin_regs_read_allowed` had a uint16 overflow-check bug; the matching positional errors in the test's expected table were corrected independently of the implementation. `build-verifier`: PASS on all steps (target build zero warnings; host tests 26/26, 0 failures, 0 ignored, exit 0; bus-access lint 82-case self-test + tree scan clean; dependencies registry-only). `firmware-reviewer`: APPROVE with one MINOR on overflow-check threshold precision (`daikin_regs.c:121`) and one process note that RESUME.md had been edited by firmware-dev. MINOR follow-up applied same session (firmware-dev, haiku): threshold changed to `>= 0x10000u` (`>=`, not `>`, to keep the loop safe at the boundary — justified deviation from the reviewer's suggestion); re-verified (build zero warnings, 26/26 host tests pass, lint clean). Nothing committed; nothing pushed.

## Status / gates
- **Gate OPEN for phase 1.** 001 done, 009 done; specs 010, 002, 006, 011, 008 next in order per `tasks/README.md`; 003/004/005/007 retired. Stage 2/3 specs only after the probe log is in `raw/` and captured.
- Open minors: (a) lint self-test still lacks explicit cases for `^= -= *= /= %= >>=` (follow-up with task 009/later); (b) task 009 test comment on `(0xFFFF,1)` says "not a wrap" but with the `>=` guard it is rejected by the wrap guard — cosmetic, low priority.
- User actions: `sudo apt install libbsd-dev` (linux host target, until then use `export CPATH="/home/szpikko/esp-host-stubs:$CPATH"`); on-site items per `concepts/bus-topology` section 8 and task 008: MAX485-M U1/supply/RO level, GPIO pins, termination/bias, panel low-voltage isolation.
- Dev env: ESP-IDF v6.0.2 (`. ~/esp/esp-idf-v6.0.2/export.sh`); host target via `idf.py --preview set-target linux`; esp-clang/clang-tidy not installed.

## Next
1. Task 010 `reg_model` + `poll_sched` (`firmware-dev`, model per spec) -> `build-verifier` + `firmware-reviewer` in parallel.
2. Then 002, 006, 011, 008 per `tasks/README.md`.
3. User runs the on-site probe (008); log to `Knowledgebase/raw/` -> `/capture`.
4. `architect` writes phase-2 specs.

## Start prompt (copy/paste into a fresh session in this repo; orchestrator is the default agent, else run `claude --agent orchestrator`)
```
Read RESUME.md and CLAUDE.md, then continue the DaikinFancoilMatterThermostat project from "Next". Use the wiki (Knowledgebase/wiki/index.md) as the source of truth, ask me any open decisions (D1-D22 of v0.3 are decided, see Knowledgebase/wiki/concepts/firmware-architecture.md), and update RESUME.md after every step.
```
