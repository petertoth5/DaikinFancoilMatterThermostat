# RESUME

Update after EVERY step. Keep short.

## Project
Matter-over-Wi-Fi thermostat on ESP32-S3-WROOM-1-N16R8-M + MAX485-M breakout (bench + on-site phase-1 run; MAX3485 3.3 V module for deployment, D23 amended), connected to the Daikin EKWHCTRL1 **"A A B B -" Modbus RTU port** (nothing else there). Node = **sole Modbus master** (FC03 polling; FC06 on Matter commands in phase 3). Phasing: 1 on-site read probe -> 2 Matter read-only mirror -> 3 Matter writes; **amended (OS6, W1–W14):** console-only writes in a separately built phase-1 **write image** (task 019) for an on-site control test; read image and Matter image stay write-free until phase 3. Knowledge: `Knowledgebase/wiki/` (agent-maintained, start at `index.md`; design and decisions in `concepts/firmware-architecture.md`), sources `Knowledgebase/raw/` (read-only for agents). Rules: root `CLAUDE.md`. Agents: `.claude/agents/README.md`. Tasks: `tasks/README.md`, spec `tasks/019-probe-console-writes.md`.

## Last step (2026-11-28)
- `wiki-keeper` sync + lint (S1 exception by user request): task 019 code status recorded in the wiki (firmware-architecture s1 roadmap to production, s3 write-image rules A6 / B1–B4, s6 rows, s7 019/019a/019b + verified facts, s8 items); stale statements on project-goal, bus-topology, ekwhctrl1, dev-environment, max485 marked superseded; index + log updated. Lint: no broken links, orphans or missing index entries.
- Before: 019a `2d8ec44` (A6 fix-up), amendments `0075304`, 019b `f65f465` (B1–B4), RESUME `26e9704`. Verification: host tests 201/0/0; read image 0x39eb0, no write symbols; write image 0x3c120, ELF write mode ok; `firmware-reviewer` GO; `build-verifier` a–g PASS.
- After this step everything is committed and pushed to `origin/main` (check `git status -sb`; untracked `.vscode/`, `ProjectDefinitionPrompt.txt` stay uncommitted).

## Status / gates
- **Task 019 code complete, acceptance pending** the bench run B.W.1–B.W.8 with the write image. **The write image is not on the board yet** (the read image from the 2026-11-27 bench run is). Trip image = write image.
- Expected write-image header (after `mb log on`; with the console-race fix it should also appear at boot, B.W.1 confirms): `# dmt-probe v1 addr=1 baud=9600 fw=<git short hash of HEAD at build time> writes=console`. The `fw=` value follows git HEAD at build time and changes with every commit; record whatever is printed (`BW.fw`).
- Phase 1 done; bench section B done 2026-11-27 (R7 caveat). Phase 2a code done (015 `9dbace6`, 016 `03dacf5`, 017 `6b10c93`); open on device: D6 factory reset + 017 D5, D7 Apple Home, O7 + D4 IM status (chip-tool), O9 idle 2 min, NVS free space.
- Spec 018 (2b-A) paused; to be revised after the trip into one combined 2b+3 plan (roadmap: firmware-architecture s1). 2b-B / 2b-C wait for the probe log in `raw/` + `/capture`.
- Dev env: ESP-IDF v6.0.2 (`. ~/esp/esp-idf-v6.0.2/export.sh`); USB via usbipd-win -> `/dev/ttyACM0` (wiki `concepts/dev-environment.md`).
- Tooling note: subagents' read-only Bash calls are sometimes blocked after the RTK hook rewrite; fall back to the Read tool.
- Working mode S1–S5: wiki once per task at acceptance; full `firmware-reviewer` only for bus-touching code; fewer, bigger tasks; orchestrator batches questions once per task.

## Hard rules for bench and site (OS1–OS6 + checklist A)
- Never touch "+ A B -" or CP. Connect/disconnect A/B only with the node unpowered and mains off. No flashing, BOOT or held RST with A/B wired; no flashing on site (W4).
- OS1 site run with the current MAX485-M (MAX3485 = deployment). OS2 D8 unchanged (single 10k DE pull-down), ~0.3 s boot-window driver-enable risk accepted. OS3 boot latch: `activity` 1–7 -> USB power-cycle, max 2 retries; ≥ 8 or 3rd latch -> abort. OS4 RO direct to GPIO4 + 20k pull-up, module VCC 3.3 V, never 5 V. OS5 never run an image without the early DE hold (the 2a Matter image) with A/B wired; after the site run disconnect A/B first, then re-flash Matter. OS6 control test (checklist E) only after D.10, armed only from E.2, gate E.0.
- STOP rules: checklist B.16/B.W/E (BADFRAME or `gap_bytes` > 0 -> stop, architect; B.W.7 stack < 1024 B -> stop).

## Next
1. **Bench (user), A/B open, module on 3.3 V, RO direct.** In `firmware/` (see `firmware/README.md`):
   `idf.py -B build_write -D SDKCONFIG=build_write/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.write" set-target esp32s3` (first time only), then the same with `build`, then `idf.py -B build_write -p /dev/ttyACM0 flash`. Serial console as in the 2026-11-27 run. Run checklist `docs/phase1-probe/checklist.md` **B.W.1–B.W.8** (B.W.2 repeats B.16). Fill the `BW.*` rows of `docs/phase1-probe/measurement-log-template.md`; save the console capture next to `docs/phase1-probe/bench-2026-11-27.log` (previous bench log). The image stays on the board for the trip.
2. `wiki-keeper` records the 019 acceptance (B.W results).
3. Trip: C, D.1–D.10 (disarmed), E control test, D.11; read 247 on site to confirm the bit numbering. If B.W does not pass, the trip stays read-only.
4. After the trip: user saves the filled log + full serial output to `Knowledgebase/raw/` (e.g. `phase1-probe-<date>.md/.log`; agents never write raw), then `/capture`, then the combined 2b+3 plan (roadmap, firmware-architecture s1).
5. Later on device: phase-2a leftovers (Status). Deployment: MAX3485 module per buying criteria (wiki s8), BOM "to decide".

## Start prompt (copy/paste into a fresh session in this repo; orchestrator is the default agent, else `claude --agent orchestrator`)
```
Read RESUME.md and CLAUDE.md, then continue the DaikinFancoilMatterThermostat project from "Next". Use the wiki (Knowledgebase/wiki/index.md, design in concepts/firmware-architecture.md) as the source of truth. Decided: D1-D23, 2a-1..10, 2b-P1..P10, OS1-OS6, W1-W14, spec 019 with A1-A10 and B1-B4. Task 019 is code complete (019a 2d8ec44, 019b f65f465; build-verifier a-g PASS, firmware-reviewer GO). First ask me for the bench B.W.1-B.W.8 results with the write image (header ends "fw=<git short hash at build time> writes=console"), then have wiki-keeper record the 019 acceptance and prepare the trip (checklist C, D.1-D.10, E, D.11; docs/phase1-probe/checklist.md, tasks/019-probe-console-writes.md). Respect the hard rules in RESUME.md. Update RESUME.md after every step.
```
