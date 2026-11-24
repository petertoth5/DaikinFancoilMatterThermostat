# Wiki Index

Catalog of all wiki pages. Maintained by the agent; see root `CLAUDE.md`.

## Sources
- [n420384a-english](sources/n420384a-english.md) — Daikin N420384A Modbus RTU manual (file 1; +AB-, 500 m / 50 units limits)
- [n420384a-modbus-rtu](sources/n420384a-modbus-rtu.md) — same manual, file 2 (+BA-, S1 grid switch; differences listed)
- [max485-datasheet](sources/max485-datasheet.md) — MAX485 family chip datasheet: 5 V supply, pinout, DE/RE, RO level
- [max485-module-photo](sources/max485-module-photo.md) — user photo of the MAX485-M breakout: RO/RE/DE/DI header, U1 marking illegible; R7 "121" vs user: no on-board termination
- [project-brief](sources/project-brief.md) — user brief: goal, hardware, dev-env rules

## Concepts
- [project-goal](concepts/project-goal.md) — Matter-over-Wi-Fi thermostat scope; architecture v0.3 approved 2026-11-08; gate open for stage-1 (probe) specs only; specs ready 2026-11-09; task 001 done 2026-11-09, 009 done 2026-11-24, 010 done (accepted and committed 2026-11-24); 002 stopped at S3, user chose E2 (own FC03-only RTU master), E2 re-confirmed with E1b (esp-modbus) as documented fallback; specs 012/013/014 ready, 002 retired; order 012 -> 013 -> 014 -> 006 -> 008, 011 any time
- [bus-topology](concepts/bus-topology.md) — node <-> EKWHCTRL1 "A A B B -" point-to-point, node sole master, nothing else connected (user 2026-11-08); EKWHCTRL1 slave (inferred); phase-1 test questions; v0.3 firmware plan pointer (D1 hard rule, gate exception)
- [dev-environment](concepts/dev-environment.md) — ESP-IDF + component-registry-only rule, host state, approved components (esp_matter ^1.6.1; esp-modbus removed 2026-11-24, E2; own project code compliant with registry-only rule); host-target facts (libbsd-dev pending, `--preview set-target linux`, no clang-tidy)
- [modbus-rs485-link](concepts/modbus-rs485-link.md) — 9600 8N1, FC 03/06 (port not named), cable/120 Ω rules ambiguous as to port, EKRTCTRL1 +AB-/+BA- legend conflict
- [register-map](concepts/register-map.md) — full register table, star list (INN-FR-B32 only) checked, 230/234 star inconsistency, quirks
- [flag-registers](concepts/flag-registers.md) — PRG, OUT, STAT, ALR_STAT bit tables (visually verified), label conflicts; 300 s timeout probably not about "A A B B -"
- [web-restriction-flags](concepts/web-restriction-flags.md) — reg 247 supervisor flags, persistence (FLAG 7 cleared), FLAG 1 "247.0" typo, troubleshooting
- [firmware-architecture](concepts/firmware-architecture.md) — v0.3 (approved 2026-11-08): sole master on "A A B B -", phases probe -> read-only mirror -> writes, components, bus-safety rules, wiring, Matter mapping, D1–D22 + stage-1 detail decisions (2026-11-09: auto polling after silence check, USB power, lint/health/Kconfig details, boot-window commands, `mb silence` leaves polling off, `mb log` default on; lint R1–R8 per task 001), task-010 detail decisions U1–U6 (2026-11-24: manual read while OFFLINE, only exc 02 = unsupported incl. RETRY, `mb poll on` no-op unless STOPPED, HEALTH exc 02 on 000 = failed read, old-address completion after `mb addr` only stats/gap; spec 010 amended), task-002 spike decisions (2026-11-24: S3 hit, E2 own FC03-only RTU master `mb_rtu` + `rs485_port`, D7/Q9 superseded, lint/ELF without esp-modbus, DE = RTS, set_mode before set_pin, DE re-drive after delete; spike/review discrepancy on the debug-log side channel recorded; later: E1b kept as unbuilt fallback behind the `mb_link.h` seam, gap bytes counted and discarded, `mb_link_get_counters` in `mb status`, exc code 0x00 = BAD_FRAME, no fake-port `mb_link` host test, FREERTOS_HZ stays 100 / no slave simulator, agent definitions corrected to sole master), task impact (001, 009, 010 done; 002 retired, replaced by specs 012/013/014; exc 02 scope not HEALTH), new risks (own TX-path code, answering slave's inter-byte timing, 10–20 ms frame-end window); v0.2 as superseded history
- [device-addressing](concepts/device-addressing.md) — address 1–255 (reg 200), keypad [+]/[—] procedure, "leave 001 free"; port not stated

## Entities
- [ekrtctrl1](entities/ekrtctrl1.md) — wall control, board connections
- [ekwhctrl1](entities/ekwhctrl1.md) — wall control: "+ A B -" = fancoil link, "A A B B -" = Modbus RTU port (6-way block shared with CP); port facts, slave inferred; node taps "A A B B -"
- [ekwhctrl0](entities/ekwhctrl0.md) — kit installed with EKWHCTRL1
- [inn-fr-b32](entities/inn-fr-b32.md) — fancoil control PCB (N540022B in figures) with the "+AB-" Modbus terminal
- [esp32-s3-wroom-1](entities/esp32-s3-wroom-1.md) — target MCU module
- [max485](entities/max485.md) — RS485 breakout module + chip facts, termination/level-shift concerns
