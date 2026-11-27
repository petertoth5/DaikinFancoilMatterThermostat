# RESUME

Update after EVERY step. Keep short.

## Project
Matter-over-Wi-Fi thermostat on ESP32-S3-WROOM-1-N16R8-M + MAX485-M breakout (bench + on-site phase-1 run, D23 amended 2026-11-27; MAX3485 3.3 V module for deployment), connecting to the Daikin EKWHCTRL1 **"A A B B -" Modbus RTU port** (nothing else connected there). Node = **sole Modbus master** (FC03 polling, FC06 only on Matter commands); staged phasing: 1 on-site read probe -> 2 Matter read-only mirror -> 3 Matter writes (write code compiled out until 3; **amended 2026-11-27, OS6 + W1–W14:** console-only writes in a separately built phase-1 write variant, task 019; Matter image write-free until phase 3). User decisions 2026-11-08. Knowledge in `Knowledgebase/wiki/` (agent-maintained, start at `index.md`), sources in `Knowledgebase/raw/`. Rules: root `CLAUDE.md`. Agents: `.claude/agents/README.md`.

## Last step (2026-11-27)
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
- **Phase 1 done; bench run done 2026-11-27** (R7 caveat: B.4.5/B.7 not conclusive). Phase-1 image is on the board. **On site unblocked with the MAX485-M** (D23 override, OS1-OS4) after the B.16 bench re-check with RO direct.
- **Phase 2a:** code 015 `9dbace6`, 016 `03dacf5` (pushed); on device 2026-11-25 mostly pass (firmware-architecture section 7 "Phase-2a on-device run"). softAP fixed (017 `6b10c93`). Open: D6 factory reset + 017 D5 (guard line on the boot after reset); D7 Apple Home; O7 SetpointRaiseLower + D4 IM status code (chip-tool or `--log-level debug`); O9 idle 2 min (64 s seen); NVS free space. Provisional (a)/(b)/STAT items decided with the probe log (section 8).
- **Phase 2b plan approved 2026-11-27:** 2b-A (bus stack + shared console + race fix, backport to phase-1 `main.c`) = spec 018, written, **paused 2026-11-27 (OS6)**, Part B (P4 backport) moved to 019 (W11), revise after the trip into a combined 2b+3 plan. 2b-B (real mapping + Fan EP2 read-only) and 2b-C (re-tune + Q-HW2 DE check via MCPWM capture) wait for the probe log in `raw/` + `/capture` and bus-topology items 1, 1b, 3, 4, 5.
- Dev env: ESP-IDF v6.0.2 (`. ~/esp/esp-idf-v6.0.2/export.sh`); flash via usbipd-win -> `/dev/ttyACM0`. Untracked `.vscode/`, `ProjectDefinitionPrompt.txt` stay uncommitted.
- Tooling note: subagents' read-only Bash calls are sometimes blocked after the RTK hook rewrite; fall back to the Read tool.

## Next
1. User rewires RO direct (USB unplugged; remove the diode; keep the 20k pull-up) and runs **B.16** on the bench (RO direct; one boot with polling: all TIMEOUT, no BADFRAME, activity 0, gap_bytes 0; latch -> reboot, max 2 repeats), capture `docs/phase1-probe/bench-2026-11-27b.log`.
2. `architect` writes **spec 019** (in progress; phase-1 write variant W1-W14 + P4 race backport; checklist sections B.W / E / E.0; 018 row + 019 row in `tasks/README.md`) -> `firmware-dev` (sonnet) -> `build-verifier` + full `firmware-reviewer` -> bench **B.W + B.16 with the write image** -> trip: C/D read probe (write image, disarmed) + E control test. If 019 is not built and reviewed before the trip, the trip stays read-only.
3. After the trip: probe log + capture to `Knowledgebase/raw/` (user; agents never write raw) + `/capture`, then the fewest-steps roadmap to production (revise 018 into a combined 2b+3 plan; 2b-B also decides the 1000 Hz tick / end-of-frame window item).
4. Later on device (user, ~30 min, board only): Apple Home (D7), chip-tool O7 + D4 IM status code, O9 idle 2 min, O3 timing, reset-task stack, NVS free space; factory reset D6 + 017 D5 last (re-pair HA after). **Re-flash the Matter image after the site run: disconnect A/B first** (OS5) (phase-1 image on the board now; flashing without erase should keep the HA pairing, inferred).
5. Deployment decisions open (BOM "to decide"): node power, isolation, enclosure, cable, mounting, bias, termination; MAX3485 module per the buying criteria (D23).

## Start prompt (copy/paste into a fresh session in this repo; orchestrator is the default agent, else run `claude --agent orchestrator`)
```
Read RESUME.md and CLAUDE.md, then continue the DaikinFancoilMatterThermostat project from "Next". Use the wiki (Knowledgebase/wiki/index.md) as the source of truth, ask me any open decisions (D1-D23 of v0.3, phase-2a 2a-1..10, 2b-P1..P10 and OS1-OS6 and W1-W14 are decided; console-write decisions W1-W14 are approved, spec 019 is in progress, see Knowledgebase/wiki/concepts/firmware-architecture.md), and update RESUME.md after every step.
```
