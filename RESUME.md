# RESUME

Update after EVERY step. Keep short.

## Project
Matter-over-Wi-Fi thermostat on ESP32-S3-WROOM-1-N16R8-M + MAX485-M breakout, joining the Daikin EKWHCTRL1 <-> fancoil Modbus RTU bus. Role (user, 2026-11-07): **passive monitor by default, master (FC06 writes/polls) only on Matter-originated commands**. Knowledge in `Knowledgebase/wiki/` (agent-maintained, start at `index.md`), sources in `Knowledgebase/raw/`. Rules: root `CLAUDE.md`. Agents: `.claude/agents/README.md`.

## Last step (2026-11-07)
Architecture v0.2 **approved by the user** and recorded in the wiki: `Knowledgebase/wiki/concepts/firmware-architecture.md` (phases, layers, zero-TX rules, Matter mapping, P3 outline, D1-D15). P1 specs `tasks/001`-`008` + `tasks/README.md` written (spec ready). Session ended with the user saying "save all progress": everything is committed and pushed to origin/main (wiki architecture `abcd70f`, P1 specs `7519040`, plus this RESUME update). Untracked `.vscode/settings.json` (machine-specific CMake path) deliberately not committed.

## Status / gates
- Independent design review of v0.1 and the decisions are integrated in `concepts/firmware-architecture`; spec order and dependencies in `tasks/README.md`.
- Dev env: ESP-IDF v6.0.2 (`. ~/esp/esp-idf-v6.0.2/export.sh`); `firmware/` skeleton builds (registry components only).
- Only P1 (listen-only) is released. P2 specs after P1; P3 (guarded TX) only after the P1 data.
- Open user points: OK for the extra zero-TX lint names (`uart_set_rts/dtr/loop_back`, `_uart_set_pin4/6`, esp-modbus controller headers); F1/F2 if task 002 cannot get `mb_crc16` into host tests; RX GPIO default 4 = placeholder; hardware open items (termination, "+"/"-" voltages, U1/supply/RO level, A/B mapping).
- Flashing path (usbipd vs Windows) deferred; scheduling out of scope.

## Next
1. Before task 001 (it implements the lint), orchestrator first asks the user: keep the extra zero-TX lint names (`uart_set_rts/dtr/loop_back`, `_uart_set_pin4/6`, esp-modbus controller headers)? Then `firmware-dev` (model per spec: sonnet) implements 001 -> `build-verifier` + `firmware-reviewer` in parallel -> findings back to `firmware-dev` -> user.
2. Then 002 ... 008 in the order of `tasks/README.md`.
3. User runs the 008 bring-up on site; logs/dumps to `Knowledgebase/raw/` -> `/capture`.
4. Revisit D15 (STAT availability) with the P1 data; then `architect` writes the P2 specs.

## Start prompt (copy/paste into a fresh session in this repo; orchestrator is the default agent, else run `claude --agent orchestrator`)
```
Read RESUME.md and CLAUDE.md, then continue the DaikinFancoilMatterThermostat project from "Next". Use the wiki (Knowledgebase/wiki/index.md) as the source of truth, ask me any open decisions (D1-D15 are decided, see Knowledgebase/wiki/concepts/firmware-architecture.md), and update RESUME.md after every step.
```
