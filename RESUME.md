# RESUME

Update after EVERY step. Keep short.

## Project
Matter-over-Wi-Fi thermostat on ESP32-S3-WROOM-1-N16R8-M + MAX485-M breakout, connecting to the Daikin EKWHCTRL1 **"A A B B -" Modbus RTU port** (nothing else connected there). Node = **sole Modbus master** (FC03 polling, FC06 only on Matter commands); staged phasing: 1 on-site read probe -> 2 Matter read-only mirror -> 3 Matter writes (write code compiled out until 3). User decisions 2026-11-08. Knowledge in `Knowledgebase/wiki/` (agent-maintained, start at `index.md`), sources in `Knowledgebase/raw/`. Rules: root `CLAUDE.md`. Agents: `.claude/agents/README.md`.

## Last step (2026-11-24)
Task 013 (lint + ELF check without esp-modbus) **accepted "as is" by the user, 2026-11-24** (L1–L9 follow-up verified: build-verifier PASS, firmware-reviewer APPROVE WITH MINORS). Not committed. Wiki updated: firmware-architecture sections 3, 6 ("Task 013 acceptance"), 7; project-goal; index; log.

## Status / gates
- **Gate OPEN for phase 1.** Done and committed: 001, 009, 010, 012. Done, not committed: 013 (accepted as is). Order: lint follow-up -> 014 -> 006 -> 008; 011 any time; all sonnet; independent firmware-reviewer review for 014. Stage 2/3 specs only after the probe log is in `raw/` and captured.
- Status: `origin/main` = `32709f7`. Uncommitted = 013 work (`firmware/`, `firmware/tools/check_elf_symbols.py`), plus this step's `RESUME.md` and wiki edits. Untracked `.vscode/` and `ProjectDefinitionPrompt.txt` stay uncommitted.
- Open decision (user, 2026-11-24, decided, NOT implemented): ban remaining ROM/register UART TX routes everywhere (R4): `esp_rom_output_tx_one_char`, `esp_rom_uart_putc` (`esp_rom_(output|uart)_\w*` TX names), `UART[0-2]->`, `REG_WRITE`/`WRITE_PERI_REG` on `UART_FIFO\w*_REG`, `soc/uart_reg.h` include. Before 014.
- Open minors of 013 (accepted as is; planned into the lint follow-up): (1) `++a[b[1]].fc` nested brackets not detected (document); (3) "matching is linear" docstring false for unterminated/repeated `/*`; (4) R5 `_second_argument` quadratic on 3000x `uart_set_mode(` (mention); (5) self-test 1.0 s per-case guard may fail on loaded machine (5 s?).
- Host-test re-run for 013 not done (permission classifier); no C code changed in 013; firmware-dev reported 118/0/0.
- Process note: `firmware-dev` must not edit RESUME.md (it did so during task 009; flagged by firmware-reviewer).
- Open minors: (a) closed by 013 (compound-operator self-test cases); (b) task 009 test comment on `(0xFFFF,1)` says "not a wrap" but with the `>=` guard it is rejected by the wrap guard — cosmetic, low priority; (c) host build prints 4 CMake `component_validation` warnings because `firmware/test/host/main/CMakeLists.txt` globs component `test_host` sources into main (pre-existing pattern from 009, cosmetic).
- Known, not a bug: `mb poll off` then `mb poll on` from OFFLINE restarts polling immediately (U4 consequence, bounded by the 5-failure re-entry); no change made.
- User actions: `sudo apt install libbsd-dev` (linux host target, until then use `export CPATH="/home/szpikko/esp-host-stubs:$CPATH"`); on-site items per `concepts/bus-topology` section 8 and task 008: MAX485-M U1/supply/RO level, GPIO pins, termination/bias, panel low-voltage isolation.
- Dev env: ESP-IDF v6.0.2 (`. ~/esp/esp-idf-v6.0.2/export.sh`); host target via `idf.py --preview set-target linux` (`--preview` needed); esp-clang/clang-tidy not installed.

## Next
1. Commit 013 (orchestrator, user-approved; commit only on request, message per the orchestrator's report).
2. Lint follow-up: `architect` amends spec 013 or writes a tiny spec (ROM/register TX ban + minors 1, 3, 4, 5) -> `firmware-dev` (sonnet) -> build-verifier (incl. host tests) + firmware-reviewer -> accept.
3. Then 014 (amended: reply-length adapter; R8 consequence L9: no `fc`/`func`/`command` names or `"fc=%d"` strings, FC hex constants decimal or named; bus-touching, DE checklist), 006, 011, 008 per `tasks/README.md`.
4. User runs the on-site probe (008); log to `Knowledgebase/raw/` -> `/capture`.
5. `architect` writes phase-2 specs.

## Start prompt (copy/paste into a fresh session in this repo; orchestrator is the default agent, else run `claude --agent orchestrator`)
```
Read RESUME.md and CLAUDE.md, then continue the DaikinFancoilMatterThermostat project from "Next". Use the wiki (Knowledgebase/wiki/index.md) as the source of truth, ask me any open decisions (D1-D22 of v0.3 are decided, see Knowledgebase/wiki/concepts/firmware-architecture.md), and update RESUME.md after every step.
```
