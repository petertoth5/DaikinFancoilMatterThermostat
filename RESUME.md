# RESUME

Update after EVERY step. Keep short.

## Project
Matter-over-Wi-Fi thermostat on ESP32-S3-WROOM-1-N16R8-M + MAX485-M breakout (bench only; MAX3485 3.3 V module for on site, D23), connecting to the Daikin EKWHCTRL1 **"A A B B -" Modbus RTU port** (nothing else connected there). Node = **sole Modbus master** (FC03 polling, FC06 only on Matter commands); staged phasing: 1 on-site read probe -> 2 Matter read-only mirror -> 3 Matter writes (write code compiled out until 3). User decisions 2026-11-08. Knowledge in `Knowledgebase/wiki/` (agent-maintained, start at `index.md`), sources in `Knowledgebase/raw/`. Rules: root `CLAUDE.md`. Agents: `.claude/agents/README.md`.

## Last step (2026-11-27)
- **Bench section B complete** (user, module MAX485-M on 3.3 V, R7 kept per decision R2). App `6654669-dirty` (= phase-1 code `5dd15ff`/`b8ad00d`); capture `docs/phase1-probe/bench-2026-11-27.log` (untracked). All B items PASS or "not measured/not done" as designed, except **B.4.5 / B.7 "passed on this run, not conclusive with R7 fitted"** (one first-boot LATCHED `activity 1` on the ~10 mV idle line). B.10 stack min free 2768 B; B.14 not done (no adapter). Static RX check PASS. Detail: wiki `concepts/firmware-architecture.md` section 7 "Phase-1 bench run", section 8.
- Decisions (user): R2; console-init race (header / first silence line lost on UART0, console only) fixed with phase 2b, small spec by the architect then, workaround `mb log on`; bench VCC 3.3 V (5 V pin "seems not working", unverified). MAX3485 buying criteria, arrival tests and wiring (architect): wiki firmware-architecture section 8.
- Wiki synced (firmware-architecture s4/s6/s7/s8, max485, bus-topology, esp32-s3-wroom-1, project-goal, index, log). Not committed.
- Open questions: capture shows a **second LATCHED boot** (lines ~96-163) not in the notes (static-check boot?); isolation-boot activity 0 not explicitly confirmed; board 5 V pin dead or only fed from the other USB port.
- Pending non-wiki edits (when the MAX3485 module is chosen): `docs/BOM.md` (module, divider line, 3.3 V); `docs/phase1-probe/checklist.md` (header commit, B.1.1 RO per module, B.2 pull-up measurements, new powered DE check; architect).

## Working mode (user decisions S1–S5, 2026-11-24)
- S1: wiki updated once per task, at acceptance (decisions and verified facts only); step progress goes to this file and git. Update this file after every step.
- S2: keep the wiki slim (done 2026-11-24: diary in `concepts/firmware-history.md`).
- S3: full independent `firmware-reviewer` only for bus-touching code (bus timing, polling, writes, UART/DE). Other code (Matter, Wi-Fi, UI): `build-verifier` + one review at feature end. Text-only fixes: no re-review loop.
- S4: fewer, bigger tasks, short specs (one per feature); `firmware-dev` on sonnet, haiku for small edits.
- S5: orchestrator batches open questions once per task; recommended defaults for minor items.

## Status / gates
- **Phase 1 done; bench run done 2026-11-27** (R7 caveat: B.4.5/B.7 not conclusive). Phase-1 image is on the board. **On site blocked** until the MAX3485 module is bought, passes the arrival tests and the bench re-run B.1-B.7 (+B.6).
- **Phase 2a:** code 015 `9dbace6`, 016 `03dacf5` (pushed); on device 2026-11-25 mostly pass (firmware-architecture section 7 "Phase-2a on-device run"). softAP fixed (017 `6b10c93`). Open: D6 factory reset + 017 D5 (guard line on the boot after reset); D7 Apple Home; O7 SetpointRaiseLower + D4 IM status code (chip-tool or `--log-level debug`); O9 idle 2 min (64 s seen); NVS free space. Provisional (a)/(b)/STAT items decided with the probe log (section 8).
- Phase 2b (real mapping, bus, Q-HW2) waits for the probe log in `raw/` + `/capture`; settle DE re-check method first.
- Dev env: ESP-IDF v6.0.2 (`. ~/esp/esp-idf-v6.0.2/export.sh`); flash via usbipd-win -> `/dev/ttyACM0`. Untracked `.vscode/`, `ProjectDefinitionPrompt.txt` stay uncommitted.
- Tooling note: subagents' read-only Bash calls are sometimes blocked after the RTK hook rewrite; fall back to the Read tool.

## Next
1. **Buy MAX3485 module** per the criteria (wiki firmware-architecture section 8) -> arrival tests (unpowered + powered DE ~0 V, A-B idle) -> do the pending BOM/checklist edits -> bench re-run B.1-B.7 incl. B.4.5 (+B.6) with RO/DI direct, DE+/RE to GPIO6 + 10k to GND, VCC 3V3.
2. On site (C/D) -> probe log -> `Knowledgebase/raw/` + `/capture` -> phase 2b (also settles provisional 233/STAT, standby/limit gaps, bus timing; console-race fix spec; DE re-check method first).
3. Later on device (user, ~30 min, board only): Apple Home (D7), chip-tool O7 + D4 IM status code, O9 idle 2 min, O3 timing, reset-task stack, NVS free space; factory reset D6 + 017 D5 last (re-pair HA after). Needs re-flashing the Matter image first (phase-1 image on the board now; flashing Matter without erase should keep the HA pairing, inferred).
4. Deployment decisions open (BOM "to decide"): node power, isolation, enclosure, cable, mounting, bias, termination (module now MAX3485, D23).

## Start prompt (copy/paste into a fresh session in this repo; orchestrator is the default agent, else run `claude --agent orchestrator`)
```
Read RESUME.md and CLAUDE.md, then continue the DaikinFancoilMatterThermostat project from "Next". Use the wiki (Knowledgebase/wiki/index.md) as the source of truth, ask me any open decisions (D1-D23 of v0.3 and phase-2a 2a-1..10 are decided, see Knowledgebase/wiki/concepts/firmware-architecture.md), and update RESUME.md after every step.
```
