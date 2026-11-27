# RESUME

Update after EVERY step. Keep short.

## Project
Matter-over-Wi-Fi thermostat on ESP32-S3-WROOM-1-N16R8-M + MAX485-M breakout (bench + on-site phase-1 run, D23 amended 2026-11-27; MAX3485 3.3 V module for deployment), connecting to the Daikin EKWHCTRL1 **"A A B B -" Modbus RTU port** (nothing else connected there). Node = **sole Modbus master** (FC03 polling, FC06 only on Matter commands); staged phasing: 1 on-site read probe -> 2 Matter read-only mirror -> 3 Matter writes (write code compiled out until 3; **amended 2026-11-27, OS6 + W1–W14:** console-only writes in a separately built phase-1 write variant, task 019; Matter image write-free until phase 3). User decisions 2026-11-08. Knowledge in `Knowledgebase/wiki/` (agent-maintained, start at `index.md`), sources in `Knowledgebase/raw/`. Rules: root `CLAUDE.md`. Agents: `.claude/agents/README.md`.

## Last step (2026-11-28)
- **019b committed `f65f465`** after the B1-B4 review fixes (019a `2d8ec44`, amendments `0075304`). **Task 019 code complete.** Final `build-verifier` a-g all PASS: host tests 201/0/0 (mutation caught by m6b); read image 0x39eb0, no write symbols; write image 0x3c120, ELF write mode ok; Matter `check_no_bus` self-test 46 (was 38) and rejects the write ELF; Matter build ok; diffs limited; every checklist console string found in the sources. `firmware-reviewer` GO earlier.
- Wiki not updated: task 019 goes into the wiki at acceptance, after the bench B.W run (S1).

## Earlier (2026-11-27)
- **019a committed `2d8ec44`** (host tests 200/0/0, A6 fix-up included); wiki fixes + spec 019 amendments A1-A10 committed `0075304`.
- **019b implemented by `firmware-dev` (uncommitted):** `mb_link_write`, `bus_write` glue, `mb write` console (arm/disarm/sp/standby/fan/season/restore), write image via `sdkconfig.defaults.write` into `firmware/build_write` (run `set-target esp32s3` first, see `firmware/README.md`), A6 disarm hooks, P4 console-race reorder in phase-1 `main.c`, checklist B.W/E + log template, README.
- **Reviews:** `build-verifier` all pass except old acceptance 7(d) (Matter never had a write FATAL); `firmware-reviewer` GO, no blocker/major.
- **Architect amendments B1-B4 in spec 019:** Matter-side guard = `check_no_bus.py` forbids `write_guard_`/`bus_write_` prefixes (new 7(d)); fix list 1-10 (checklist E.0.3/E.3 limit checks, line-7 hedge, gap_bytes after write_TIMEOUT, E.9 log-dropped check, ABORTED reason fix + test, op_addr indent, sdkconfig.defaults comment). **`firmware-dev` (haiku) applying the fixes now.**
- Later wiki pass: record the A6 rule and B1-B4 under the console-write design (firmware-architecture).
- **Spec 019 approved by the user with A1-A10**; decisions committed `c6c2a32`, specs 018 (paused) + 019 committed `a3ea455`. **019a in progress** (`firmware-dev`). User declined a bench Modbus slave (second adapter): first real FC06 frame goes out on site (risk accepted; pre-trip check = host tests with the fake slave).
- Wiki fixed after the `datasheet-reader` re-check of N420384A (reg 247 FLAG 0/4/5/7 exact wording, FLAG 7 = power + SET+; 231 Note 2, 233 printed "0/3 (inv)/5 (est)", 201 legend; FC 03/06 quote, no FC16). Open on site: confirm 247 bit numbering by reading 247; out-of-range 231 write behaviour unknown. Not committed.
- **Console-write design approved (user W1–W14):** W1 console-only writes in a separate phase-1 write variant, Matter write-free until 3; W2 gate exception (019 before the probe log; run-time gate fresh-OK + ONLINE, checklist E.0); W3 `CONFIG_DMT_BUS_WRITE` + overlay `sdkconfig.defaults.write`, `build_write/` (FATAL removed in `firmware/main`, kept in `firmware/matter`; lint R7/R8 + ELF two modes in spec 019); W4 ONE image for the trip (flashed on the bench, disarmed for C/D, armed for E, no flashing on site; principle: fewest images/flashes and steps to production); W5 write set 231 / 201 bit 7 / 201 bits 0-2 RMW / 233 (0/3/5), 008 never; W6 arm 120 s (30-600), restore set at first arm per boot/address; W7 no auto resend, failure disarms, latch after 3 failures until reboot; W8 >= 5 s spacing, <= 40 FC06 per arm, no per-boot cap; W9 refuse arm on 247 flags 1,2,3,6,7,8, flags 4/5 clamp, 247 + 201 bit 4 never written; W10 log PRE/WRITE/VERIFY + W + A lines; W11 one task 019 incl. P4 race backport, 018 Part B dropped; W12 season test last, <= 60 s; W13 restore 201 -> 233 -> 231; W14 E.0 = D.3 OK for 201/231/233/202/203/247/105 (+245/246), D.7 done, D.8 >= 10 min < 1 %, no BADFRAME last 10 min. Wiki synced (firmware-architecture s1/s3/s6/s7/s8, index, log). Not committed.
- **OS6 (user):** on the upcoming site visit (same trip, MAX485-M per OS1-OS5), after the read probe, test **control** via **console commands in the probe image**: setpoint, on/standby, fan speed, season/mode. Matter/HA control path later (2b + phase 3). Brings phase-3 writes forward for the probe image. Architect drafts the design (write scope, safety, console commands, checklist section E) for user approval; **no spec yet**. **Spec 018 (2b-A) PAUSED**, revise afterwards (possibly incl. writes). Wiki synced (firmware-architecture s1/s3/s6 OS6/s7, index, log). Not committed.
- Spec 018 file `tasks/018-bus-in-matter-readonly.md` exists (paused, uncommitted). A1-A8 labels are in the architect's report, not in the spec (tick item = S7 in spec 018).
- Bench run B recorded and committed (`357ae5e`). Bench open questions resolved: second LATCHED boot = static RX check boot; isolation boot activity 0.
- **On-site prep decisions (user):** OS1 site run with the current MAX485-M (D23 "bench only" overridden; MAX3485 = deployment); OS2 D8 unchanged, single 10k DE pull-down, ~0.3 s boot-window driver-enable risk **accepted** (unbounded while RST held / download mode); OS3 relaxed latch rule (activity 1-7 -> USB power-cycle, max 2 retries; >= 8 or 3rd latch -> abort); OS4 RO direct to GPIO4 + 20k pull-up, never 5 V VCC, bench re-check B.16 first.
- **Phase-2b plan approved** (2b-P1..P10 defaults; tasks 2b-A / 2b-B / 2b-C; 2b-A before the probe log, bench only A/B open).
- Wiki synced (firmware-architecture s4/s6/s7/s8, max485, bus-topology, max485-module-photo, project-goal, index, log); `docs/phase1-probe/checklist.md` and `docs/BOM.md` amended. Not committed.
- **OS5 hard rule (user):** never run an image without the early DE hold (now: the 2a Matter image, never drives GPIO6) with A/B wired; only phase-1 or the future 2b-A BUS image may. After the site run: disconnect A/B first, then re-flash Matter. Recorded in wiki s6/s8 and checklist A.6, D.11.
- S5 defaults applied: `B16.*` / `D2a.ab_idle` rows in `measurement-log-template.md`; B.16 latch -> reboot, max 2 repeats, log each; BADFRAME or gap_bytes > 0 -> STOP, architect.
- **Spec 018 (2b-A)** written by the architect (`tasks/018-bus-in-matter-readonly.md`, assumptions A1-A8), **awaiting user approval**; `tasks/README.md` row for 018 still missing (architect, pending). A4 (1000 Hz tick -> end-of-frame window ~5-6 ms vs ~20-30 ms, >= t3.5 4.0 ms) recorded as open 2b-B item (wiki s8).
- Note: `docs/phase1-probe/bench-2026-11-27.log` was still being appended by the user's open monitor session after commit `357ae5e`.
- Open: board 5 V pin issue still unverified.

## Working mode (user decisions S1–S5, 2026-11-24)
- S1: wiki updated once per task, at acceptance (decisions and verified facts only); step progress goes to this file and git. Update this file after every step.
- S2: keep the wiki slim (done 2026-11-24: diary in `concepts/firmware-history.md`).
- S3: full independent `firmware-reviewer` only for bus-touching code (bus timing, polling, writes, UART/DE). Other code (Matter, Wi-Fi, UI): `build-verifier` + one review at feature end. Text-only fixes: no re-review loop.
- S4: fewer, bigger tasks, short specs (one per feature); `firmware-dev` on sonnet, haiku for small edits.
- S5: orchestrator batches open questions once per task; recommended defaults for minor items.

## Status / gates
- **Task 019 code complete** (019a `2d8ec44`, 019b `f65f465`); acceptance pending the bench B.W run. **Trip image = write image** (expected boot header `fw=f65f465 ... writes=console`); it is not on the board yet.
- **Phase 1 done; bench run done 2026-11-27** (R7 caveat: B.4.5/B.7 not conclusive). Phase-1 image is on the board. **On site unblocked with the MAX485-M** (D23 override, OS1-OS4) after the B.16 bench re-check with RO direct.
- **Phase 2a:** code 015 `9dbace6`, 016 `03dacf5` (pushed); on device 2026-11-25 mostly pass (firmware-architecture section 7 "Phase-2a on-device run"). softAP fixed (017 `6b10c93`). Open: D6 factory reset + 017 D5 (guard line on the boot after reset); D7 Apple Home; O7 SetpointRaiseLower + D4 IM status code (chip-tool or `--log-level debug`); O9 idle 2 min (64 s seen); NVS free space. Provisional (a)/(b)/STAT items decided with the probe log (section 8).
- **Phase 2b plan approved 2026-11-27:** 2b-A (bus stack + shared console + race fix, backport to phase-1 `main.c`) = spec 018, written, **paused 2026-11-27 (OS6)**, Part B (P4 backport) moved to 019 (W11), revise after the trip into a combined 2b+3 plan. 2b-B (real mapping + Fan EP2 read-only) and 2b-C (re-tune + Q-HW2 DE check via MCPWM capture) wait for the probe log in `raw/` + `/capture` and bus-topology items 1, 1b, 3, 4, 5.
- Dev env: ESP-IDF v6.0.2 (`. ~/esp/esp-idf-v6.0.2/export.sh`); flash via usbipd-win -> `/dev/ttyACM0`. Untracked `.vscode/`, `ProjectDefinitionPrompt.txt` stay uncommitted.
- Tooling note: subagents' read-only Bash calls are sometimes blocked after the RTK hook rewrite; fall back to the Read tool.

## Next
1. **Bench (user):** build + flash the write image with A/B open (module on 3.3 V, RO direct; commands in `firmware/README.md`):
   `idf.py -B build_write -D SDKCONFIG=build_write/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.write" build`, then `idf.py -B build_write -p /dev/ttyACM0 flash`. Check header `fw=f65f465 ... writes=console`. Run checklist **B.W.1-B.W.8** (B.W.2 repeats B.16, so B.16 with the old image is optional; B.W.7 stack >= 1024 B). Image stays on the board for the trip.
2. `wiki-keeper` records task 019 at acceptance (incl. A6 rule and B1-B4 under the console-write design, firmware-architecture).
3. Trip: C, D.1-D.10 (disarmed), E control test, D.11; OS1-OS6 rules; disconnect A/B before re-flashing Matter (OS5). On site: read 247 to confirm the bit numbering. If B.W does not pass, the trip stays read-only.
4. After the trip: probe log + capture to `Knowledgebase/raw/` (user; agents never write raw) + `/capture`, then the fewest-steps roadmap to production (combined 2b+3 plan revising 018; 2b-B decides the 1000 Hz tick item).
5. Later on device (user, board only): Apple Home (D7), chip-tool O7 + D4 IM status code, O9 idle 2 min, O3 timing, reset-task stack, NVS free space; factory reset D6 + 017 D5 last.
6. Deployment decisions open (BOM "to decide"); MAX3485 module per the buying criteria (D23).

## Start prompt (copy/paste into a fresh session in this repo; orchestrator is the default agent, else run `claude --agent orchestrator`)
```
Read RESUME.md and CLAUDE.md, then continue the DaikinFancoilMatterThermostat project from "Next". Use the wiki (Knowledgebase/wiki/index.md) as the source of truth, ask me any open decisions (D1-D23 of v0.3, phase-2a 2a-1..10, 2b-P1..P10 and OS1-OS6 and W1-W14 are decided; console-write decisions W1-W14 and spec 019 (A1-A10, B1-B4) are approved, task 019 is code complete (019a 2d8ec44, 019b f65f465, build-verifier a-g PASS): first ask me for the bench B.W.1-B.W.8 results with the write image (header fw=f65f465 ... writes=console), then have wiki-keeper record 019 at acceptance and prepare the trip (C, D.1-D.10, E, D.11), see tasks/019-probe-console-writes.md and Knowledgebase/wiki/concepts/firmware-architecture.md), and update RESUME.md after every step.
```
