# RESUME

Update after EVERY step. Keep short.

## Project
Matter-over-Wi-Fi thermostat on ESP32-S3-WROOM-1-N16R8-M + MAX485-M breakout, connecting to the Daikin EKWHCTRL1 **"A A B B -" Modbus RTU port** (nothing else connected there). Node = **sole Modbus master** (FC03 polling, FC06 only on Matter commands); staged phasing: 1 on-site read probe -> 2 Matter read-only mirror -> 3 Matter writes (write code compiled out until 3). User decisions 2026-11-08. Knowledge in `Knowledgebase/wiki/` (agent-maintained, start at `index.md`), sources in `Knowledgebase/raw/`. Rules: root `CLAUDE.md`. Agents: `.claude/agents/README.md`.

## Last step (2026-11-27)
- Earlier: task 017 (STA-only Wi-Fi, `6b10c93`) and `docs/BOM.md` (`5dd15ff`) done.
- Phase-1 image check (`build-verifier`): old `firmware/build` binary was stale in metadata only (`9dbace6-dirty`, built 2026-11-25); phase-1 sources unchanged since `b8ad00d` (only `firmware/test` changed). Rebuilt: app `5dd15ff` (clean), md5 `31023b78b6caf9613a2ae6657dc37efa`, 0x39d90 B (77 % free), 0 warnings; bus lint 0 + 372 self-test ok; ELF ok; host tests 139/0/0; clang-tidy not run (no Espressif clang). Kconfig: UART1, TX 5, RX 4, DE 6, 9600, addr 1, `CONFIG_DMT_BUS_WRITE` not set. Flash: `idf.py -C firmware -p /dev/ttyACM0 flash` (sdkconfig 2MB flash, fine on 16 MB).
- Record B3.commit = `5dd15ff` (phase-1 code identical to `b8ad00d`); checklist header "image b8ad00d" outdated (open minor fix).
- Phase 1 has no `nvs_flash` calls and writes only 0x0/0x8000/0x10000; Matter NVS 0x9000-0x15000 untouched, so HA pairing should survive (inferred).
- User decisions B.1: GPIO TX 5->DI, RX 4<-RO, DE 6->DE+/RE (10k pull-down). MAX485-M VCC from ESP board 5 V pin -> RO needs divider (e.g. 10k/20k) to RX. Power (user decision 2026-11-27): on the bench the ESP board is powered ONLY from the PC USB on the UART (CH343, `/dev/ttyACM0`) port; one cable gives power + console, as checklist A.5/A.6 assume. The USB power adapter is used only where no PC is attached (not on the bench). MAX485-M VCC from the board's 5 V pin stays.
## Working mode (user decisions S1–S5, 2026-11-24)
- S1: wiki updated once per task, at acceptance (decisions and verified facts only); step progress goes to this file and git. Update this file after every step.
- S2: keep the wiki slim (done 2026-11-24: diary in `concepts/firmware-history.md`).
- S3: full independent `firmware-reviewer` only for bus-touching code (bus timing, polling, writes, UART/DE). Other code (Matter, Wi-Fi, UI): `build-verifier` + one review at feature end. Text-only fixes: no re-review loop.
- S4: fewer, bigger tasks, short specs (one per feature); `firmware-dev` on sonnet, haiku for small edits.
- S5: orchestrator batches open questions once per task; recommended defaults for minor items.

## Status / gates
- **Phase 1 done**; probe image rebuilt `5dd15ff` (2026-11-27), **not flashed**; bench run pending by the user.
- **Phase 2a:** code 015 `9dbace6`, 016 `03dacf5` (pushed); on device 2026-11-25 mostly pass (firmware-architecture section 7 "Phase-2a on-device run"). softAP fixed (017 `6b10c93`). Open: D6 factory reset + 017 D5 (guard line on the boot after reset); D7 Apple Home; O7 SetpointRaiseLower + D4 IM status code (chip-tool or `--log-level debug`); O9 idle 2 min (64 s seen); NVS free space. Provisional (a)/(b)/STAT items decided with the probe log (section 8).
- Phase 2b (real mapping, bus, Q-HW2) waits for the probe log in `raw/` + `/capture`; settle DE re-check method first.
- Dev env: ESP-IDF v6.0.2 (`. ~/esp/esp-idf-v6.0.2/export.sh`); flash via usbipd-win -> `/dev/ttyACM0`. Untracked `.vscode/`, `ProjectDefinitionPrompt.txt` stay uncommitted.
- Tooling note: subagents' read-only Bash calls are sometimes blocked after the RTK hook rewrite; fall back to the Read tool.

## Next
1. Phase-1 bench run, checklist section B (`docs/phase1-probe/checklist.md`; parts in `docs/BOM.md`). Image check done (rebuilt `5dd15ff`, see Last step); MAX485 at 5 V decided. Remaining before B.1: wire per B.1 with the RO->RX divider (10k from RO, 20k to GND, tap to GPIO4). Then step-by-step B.1–B.15 with the user. Minor: fix checklist header "image b8ad00d". Flashing phase 1 replaces the Matter image; re-flashing Matter without erase keeps the HA pairing.
2. On site C/D -> probe log -> `Knowledgebase/raw/` + `/capture` -> phase 2b (also settles provisional 233/STAT, standby/limit gaps, bus timing).
3. Later on device (user, ~30 min, board only): Apple Home (D7), chip-tool O7 + D4 IM status code, O9 idle 2 min, O3 timing, reset-task stack, NVS free space; factory reset D6 + 017 D5 last (re-pair HA after).
4. Deployment decisions open (BOM "to decide"): node power, isolation, enclosure, cable, mounting, bias, termination.

## Start prompt (copy/paste into a fresh session in this repo; orchestrator is the default agent, else run `claude --agent orchestrator`)
```
Read RESUME.md and CLAUDE.md, then continue the DaikinFancoilMatterThermostat project from "Next". Use the wiki (Knowledgebase/wiki/index.md) as the source of truth, ask me any open decisions (D1-D22 of v0.3 and phase-2a 2a-1..10 are decided, see Knowledgebase/wiki/concepts/firmware-architecture.md), and update RESUME.md after every step.
```
