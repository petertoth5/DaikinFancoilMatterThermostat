# RESUME

Update after EVERY step. Keep short.

## Project
Matter-over-Wi-Fi thermostat on ESP32-S3-WROOM-1-N16R8-M + MAX485-M breakout, joining the Daikin EKWHCTRL1 <-> fancoil Modbus RTU bus. Role (user, 2026-11-07): **passive monitor by default, master (FC06 writes/polls) only on Matter-originated commands**. Knowledge in `Knowledgebase/wiki/` (agent-maintained, start at `index.md`), sources in `Knowledgebase/raw/`. Rules: root `CLAUDE.md`. Agents: `.claude/agents/README.md`.

## Last step (2026-11-07)
Lint fixes applied (H1, H2, M1–M7, L1–L4) from a `datasheet-reader` visual check of the Daikin manuals: EKWHCTRL1 "+ A B -" = serial connection for fancoil, "A A B B -" = Modbus RTU port (wiki had them reversed; user wiring agrees with the manual); INN-FR-B32 = fancoil PCB (location resolved). Wiki check: 0 broken links, 0 orphans. Committed and pushed as `d356b1a` to origin/main (git clean).

## Status / gates
- Dev env: ESP-IDF v6.0.2 (`. ~/esp/esp-idf-v6.0.2/export.sh`); `firmware/` skeleton builds (registry components only).
- No task specs/firmware before the architecture is approved by the user.
- Gate: user decides whether the remaining open items block the architecture draft (they mostly affect hardware/wiring).
- Open items (wiki `concepts/bus-topology`, `entities/ekwhctrl1`, `entities/max485`, `concepts/flag-registers`): is the "+ A B -" fancoil link Modbus RTU 9600 8N1 (sniff); who is master there; tap point "+ A B -" (user's wiring) vs unused EKWHCTRL1 Modbus port "A A B B -" (design decision); existing bus termination + 120 Ω placement (measure on-site); Daikin A/B vs MAX485 A/B mapping; module U1 marking, supply voltage, bias resistors, RO 5 V level shift, R7 role; OUT/STAT readability; ALR_STAT label conflicts (bit 8, bit 11); p3 "CP" label on "A A B B -".
- Flashing path (usbipd vs Windows) deferred; scheduling out of scope.

## Next
1. Step 2: `architect` brainstorms the architecture: passive monitor + Matter-triggered master, arbitration/bus-idle detection, 300 s master-timeout note, tap point "+ A B -" vs EKWHCTRL1 Modbus port "A A B B -" (user decision), Modbus<->Matter mapping, `espressif/esp_matter`, test strategy. Open hardware items become explicit assumptions. User approval required before task specs -> `wiki-keeper` records -> task specs in `tasks/`.
2. Implement per spec: `firmware-dev` -> `build-verifier` + `firmware-reviewer`.

## Start prompt (copy/paste into a fresh session in this repo; orchestrator is the default agent, else run `claude --agent orchestrator`)
```
Read RESUME.md and CLAUDE.md, then continue the DaikinFancoilMatterThermostat project from "Next". Use the wiki (Knowledgebase/wiki/index.md) as the source of truth, ask me the open bus-topology questions, and update RESUME.md after every step.
```
