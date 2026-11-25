# Wiki Log

Append-only. Format: `## [YYYY-MM-DD] <capture|sync|lint> | <subject>`

## [2026-11-01] init | wiki structure created
Raw files present, not yet ingested:
- EKWHCTRL0.EKWHCTRL1.EKRTCTRL1_Installation manual_N420384A_English.pdf
- EKWHCTRL1,EKRTCTRL1,EKWHCTRL0_Installation manual_N420384A ModBus RTU_English.pdf

## [2026-11-01] capture | both N420384A PDFs
Text extracted with pdftotext (figures/diagram bits not visually checked).
Created: sources/n420384a-english, sources/n420384a-modbus-rtu; concepts/{modbus-rs485-link,register-map,flag-registers,web-restriction-flags,device-addressing}; entities/{ekrtctrl1,ekwhctrl1,ekwhctrl0,inn-fr-b32}.
Open conflicts: +AB- vs +BA- polarity label; 500 m/50-unit limit only in file 1; S1 grid switch only in file 2.

## [2026-11-02] capture | project-brief.md
Created: sources/project-brief; concepts/{project-goal,bus-topology,dev-environment}; entities/{esp32-s3-wroom-1,max485}. Dev-env page also records local host inspection and registry facts (external, flagged).
Open: IDF version, broken ~/.espressif install, bus master/tap point, MAX485 module details.

## [2026-11-02] capture | dev environment
Updated concepts/dev-environment: ESP-IDF v6.0.2 installed, old install removed, firmware/ skeleton builds.

## [2026-11-02] capture | MAX485-M_EN_10035508.pdf
Read visually (18 PDF pages; p1 Hestore cover). Created sources/max485-datasheet. Updated entities/{max485,esp32-s3-wroom-1}, concepts/bus-topology, index.
Findings: 5 V supply; RO output 5 V level vs ESP32-S3 3.3 V (level shift, inferred); datasheet is chip-only, module board details still unknown.

## [2026-11-02] sync | no changes needed
4 raw files, 4 source pages; no raw file new, changed (mtime not newer than `updated`) or missing. All wiki pages are in index.md, no broken links, no orphans, every `sources:` entry resolves to an existing raw file.

## [2026-11-02] lint | fixes applied
Recorded INN-FR-B32 location conflict (brief: in fancoil vs manuals: in wall panels) on entities/inn-fr-b32 and concepts/bus-topology; marked bus-termination assumption (inferred); refreshed `updated` on 3 pages; completed `sources:` on esp32-s3-wroom-1 and bus-topology. No contradictions otherwise; external registry/local-install facts left as flagged.

## [2026-11-02] sync | user decisions recorded
Second-master decision (bus-topology), Matter-mapping approval and development gate (project-goal), both from user statements in the session, not raw.

## [2026-11-07] capture | 20261107_192127.jpg
User photo of the MAX485-M module, read visually by `datasheet-reader` (image). Created sources/max485-module-photo. Updated entities/max485 (it is a breakout board; RO/RE/DE/DI header, separate DE/RE; R7 "121" = 120 Ω likely termination; U1 marking and supply voltage undetermined), sources/max485-datasheet, concepts/bus-topology, index.
Open: U1 marking, right-header/terminal labels, resistor codes, underside, A–B resistance and bias measurement, D1.

## [2026-11-07] sync | user answers + visual verification of p4/p5
User answers (session statements, not raw) recorded on concepts/bus-topology, concepts/modbus-rs485-link, entities/ekwhctrl1, entities/inn-fr-b32: tap on EKWHCTRL1 "+ A B -" port; role = passive monitor, master only on Matter commands (refines 2026-11-02 second-master decision); no slave address needed; user-checked label "+ A B -" (source conflict kept, resolved for this installation); INN-FR-B32 location still unknown. New conflict: manual puts the fancoil link on EKWHCTRL1 "A A B B -", user says "+ A B -" (entities/ekwhctrl1, bus-topology).
concepts/flag-registers rewritten from `datasheet-reader` visual check (PDF images p4/p5, both files): STAT positions verified, full STAT/ALR_STAT tables, OUT/STAT INN-FR-B32-only, 300 s master timeout. New conflicts: ALR_STAT bit 8 "SW GRL" vs "GRID", bit 11 "2 AIR M5" vs "2 AIR M4". Source pages note p4 wording difference ("e" vs "and", pdftotext check).

## [2026-11-07] sync | user follow-up: termination, fancoil port
User statements (session 2026-11-07, not in raw). (1) MAX485-M has no on-board 120 Ω, user bought one separately: recorded as conflict with the photo inference "R7 = termination" on sources/max485-module-photo and entities/max485; user statement wins, R7 role/code reading unexplained; R7-removal warning dropped (entities/max485, concepts/bus-topology). (2) Existing bus termination unknown, to be measured on-site: new open item 3 on concepts/bus-topology (incl. placement of the separate 120 Ω). (3) Fancoil pair physically on EKWHCTRL1 "+ A B -": item 1a resolved for this installation by user observation, manual p3 statement kept (concepts/bus-topology, entities/ekwhctrl1). index summaries updated.

## [2026-11-07] sync | no changes needed
Full reconcile of raw/ (5 files) against wiki/sources/ (5 pages): every raw file has a source page, no raw mtime newer than its source page's `updated`, no vanished raw files. All front-matter `sources:` entries across 19 pages resolve to existing raw files. index.md lists all 19 pages; all [[...]] and relative links resolve (0 broken); no orphans (each page has at least one inbound link besides index.md). No pages edited.

## [2026-11-07] lint | fixes applied
User-approved fixes H1, H2, M1–M7, L1–L4. Evidence for H1/H2/M2/M7: `datasheet-reader` visual check of both N420384A PDFs (pages rendered as images at 200/400 dpi; PDF page = printed page).
- H1: entities/ekwhctrl1 rewritten: "+ A B -" = "Serial connection for fancoil" (legend B), "A A B B -" = "ModBus RTU Serial Connection" (legend A), p3. Earlier wiki had the roles reversed; the "source vs user" fancoil-port conflict is withdrawn on ekwhctrl1 and concepts/bus-topology (user wiring agrees with the manual). p3 lower diagram recorded (daisy chain "RS485", 120 Ω, no star; item-2 "CP" label on "A A B B -" kept as printed).
- H2: entities/inn-fr-b32 rewritten: fancoil control PCB in the appliance's electrical box, = N540022B in the p2/p3 figures (terminal-label comparison, inferred, high confidence). Location conflict (log 2026-11-02) resolved: manual agrees with the brief; PCB-label check optional. bus-topology item 0 and entities/ekrtctrl1 wording updated; index summaries updated.
- M1: bus-topology: 120 Ω termination required (Daikin p3; both ends per MAX485 datasheet p13); node terminates only at a line end (inferred).
- M2: modbus-rs485-link, bus-topology Q5, index: no EKWHCTRL1 polarity conflict ever existed; EKRTCTRL1 +AB-/+BA- legend conflict stays unresolved; new open item Daikin A/B vs MAX485 A/B mapping.
- M3: bus-topology: master-timeout legend quoted; STAT only on INN-FR-B32. M4: concepts/project-goal: dev env + agents done, node role added. M5: 2026-11-02 second-master bullet marked superseded. M6: register-map, sources/n420384a-modbus-rtu: "values identical; wording differences listed". M7: flag-registers ALR_STAT bit 11 legend quoted, visually verified (table "2 AIR M5" conflict kept).
- L1: inn-fr-b32 `sources:` + project-brief.md. L2: max485, sources/max485-datasheet: p13 nuance (characteristic impedance at both ends; 120 Ω from figures). L3: inn-fr-b32 "most motor-speed registers (210–215)". L4: index.md and log.md intentionally have no front matter (catalog/log convention).
- Q4 F1/F2 extra differences added to both N420384A source pages (EKRTCTRL1 legend-only +BA-, contact/consent output, Italian p2 bullet, "parità=nessuna"). modbus-rs485-link `sources:` + MAX485 datasheet.
New open questions: (a) is the "+ A B -" fancoil link Modbus RTU 9600 8N1 (likely, inferred; sniff); (b) who is master there; (c) tap point "+ A B -" vs the apparently unused EKWHCTRL1 Modbus port "A A B B -" (design decision, ask user whether anything is connected); (d) p3 item-2 "CP presence contact" label on "A A B B -"; Daikin A/B vs MAX485 A/B mapping.
Check after fixes: 19 pages, 0 broken links, 0 orphans, all pages in index, front matter complete, all `sources:` resolve to raw files.

## [2026-11-07] sync | firmware architecture approved
User decisions (session 2026-11-07, not in raw): accepted architecture v0.2 recorded as new concepts/firmware-architecture (architect base text + orchestrator corrections A–I from v0.2: single compile-time TX gate `CONFIG_APP_BUS_TX`, no runtime TX enable; Thermostat/Fan mapping; P3 outline; GPIO; commissioning; D1–D15). External facts (registry, esp-modbus v2.1.4 source, IDF, CHIP data_model, Modbus spec) labelled as such. Superseded: v0.1 "Kconfig + NVS" TX enable. Linked from index, concepts/project-goal (architecture done, Tasks pointer: P1 specs 001–008), concepts/bus-topology (firmware plan: items 1/1b/4/5 measured in P1, 3/3a in tasks/008), concepts/dev-environment (approved components), entities/esp32-s3-wroom-1 (GPIO). Open user points: extra zero-TX lint names, F1/F2, RX GPIO placeholder, hardware items.

## [2026-11-08] sync | tap point changed to A A B B -, redesign ordered
User decision (session 2026-11-08, not in raw; no raw files changed): the new node connects to the EKWHCTRL1 "A A B B -" ModBus RTU port, not the "+ A B -" fancoil link (user quote recorded on concepts/bus-topology). Agrees with the manual p3 legend A/B (visually verified earlier by `datasheet-reader`). Redesign ordered.
- concepts/bus-topology: banner + new 2026-11-08 section; 2026-11-07 tap-point answer (Q1) and "Firmware plan" paragraph marked superseded (text kept); role Q2 marked pending user re-confirmation; open items re-evaluated: 1, 1b reframed for "A A B B -", 2 and 4 to be re-evaluated in redesign, 3 now concerns the "A A B B -" segment, 5 reframed, 6 DECIDED, 7 flagged more relevant, new 8 (role); new question: anything else on "A A B B -"?
- entities/ekwhctrl1: "This installation" gets the 2026-11-08 decision; (c) resolved; (d) flagged; (a)/(b) to be re-evaluated.
- concepts/firmware-architecture: status note at top, v0.2 superseded for the tap point, v0.3 pending; body unchanged.
- concepts/project-goal: redesign in progress, P1 specs 001–008 on hold.
- index.md summaries updated for the four pages.

## [2026-11-08] sync | A A B B - port facts, sole-master role, staged phasing
No raw files added or changed. Inputs: user answers (session 2026-11-08, not in raw) and `datasheet-reader` visual check of both N420384A PDFs (all pages rendered as images, p3 at 600 dpi; PDF page = printed page).
- User: manual read = one of the two N420384A PDFs in raw; nothing connected to "A A B B -"; node = SOLE Modbus master on "A A B B -" (FC03 polling, FC06 only on Matter commands), supersedes the 2026-11-07 "passive monitor" role (text kept, marked); staged phasing read probe -> read-only mirror -> writes, write code compiled out until phase 3; "-AB+" = "+ A B -" (removed the "(inferred)" mark).
- concepts/bus-topology: new second-round section; items 1/1b/2/3/4/5/7 rewritten (arbitration no longer applies, inferred; termination/cabling ambiguity; CP block resolved, new "-" shared GND sub-question); items 6/8 answered; phase-1 [P1] questions; manual-facts summary.
- entities/ekwhctrl1: "A A B B -" facts section (6-way block, not-stated list, slave inferred with p1/p3/p6 quotes, daisy-chain inferred); 120 Ω only on fancoil chain; (c)/(d) resolved; new (e) phase-1 checks.
- concepts/modbus-rs485-link: serial settings quoted, port not named, FC03 quantity>1 not stated; cabling rules ambiguity added; "new third node on this bus" marked superseded.
- concepts/device-addressing: fixed "up/down" -> [+]/[—] keys; reg 200 note; p7 quote; port not stated.
- concepts/register-map: p4 scope quote, star list checked (no mismatch), 230/234 inconsistency, starred-via-panel inference, p4/p5/p6 quotes.
- concepts/flag-registers: 300 s timeout re-assessed (not evidence about "A A B B -", inferred low-medium); old "bus-design relevance" line marked superseded; PRG bits 8–15 quote.
- concepts/web-restriction-flags: persistence quote, FLAG 1 "(247.0)" typo in both files, FLAG 7 wording, setpoint quote.
- concepts/firmware-architecture: status note extended (role and phasing re-decided, v0.3 being drafted). concepts/project-goal: role, phasing, gate.
- sources/n420384a-english, sources/n420384a-modbus-rtu: re-check facts and F1/F2 differences (500 m/50 units omitted, 218/219 wording, "SET+ e SET-").
- index.md summaries updated.

## [2026-11-08] sync | firmware architecture v0.3 approved
No raw files added or changed. User decision (session 2026-11-08, not in raw): architecture v0.3 approved, Q1–Q10 all answered with the architect's recommendation (Q1 DE + /RE tied to RTS, 10 kΩ pull-down; Q2 233 = 0 without STAT -> last explicit season, default Heat, console flag; Q3 timing defaults; Q4 boot silence check; Q5 247 read-only; Q6 address Kconfig default 1 + RAM override, no scan; Q7 write caps; Q8 gate exception for stage-1 specs; Q9 esp-modbus ==2.1.4; Q10 D3/D4 re-approved).
- concepts/firmware-architecture: rewritten as v0.3 with sections 1–8 (phases and probe exit criteria 1–11, components and threading, bus-safety rules, hardware wiring, Matter mapping, decisions D1–D22, task impact, open items); v0.2 Matter mapping details, commissioning D11, GPIO avoid list, console rule and D14 merged in; v0.2 reduced to a "Superseded v0.2 (2026-11-07)" table. Conflict recorded: reg 200 range 1–255 (manual) vs Modbus 1..247 (external). Two gaps flagged as open (CONFIG_UART_ISR_IN_IRAM; 233 = 0 with STAT bits both/neither set).
- concepts/project-goal: v0.3 approved, stage-1 specs being written, gate open for stage 1 only.
- concepts/bus-topology: banner, new "Firmware plan v0.3" section (D1 hard rule, Q8 gate exception, probe items -> open items mapping, inferred); v0.2 plan section renamed.
- concepts/dev-environment: esp-modbus use per v0.3 (v0.2 use marked superseded).
- entities/ekwhctrl1: redesign pointer -> v0.3 approved.
- index.md summaries for firmware-architecture, project-goal, bus-topology, dev-environment.

## [2026-11-09] sync | stage-1 detail decisions
No raw files added or changed. User decisions (session 2026-11-09, not in raw) on the architect's stage-1 detail questions: polling starts automatically after a clean 5 s boot silence check (architect proposed waiting for a console command); `mb poll off/on` remain, probe/manual reads work while stopped without retry and without changing health; site power = normal USB (architect proposed battery laptop / USB isolator; declined), GND-to-"-" measurement rule unchanged. Accepted architect details: `mb addr <1-247>` (RAM-only, D19), `mb log on|off`; DEGRADED = 1–4 failures, OFFLINE at 5; exceptions other than 02 no retry, 60 s backoff, not counted toward OFFLINE; stale = 3 × group period (15 s / 180 s); stricter bus-access lint; no write file in phase 1 (gate on fails the build until phase 3); host fake slave at the `poll_sched` link-function seam; Kconfig safety-cap ranges; GPIO defaults TX 5 / RX 4 / DE 6; `CONFIG_UART_ISR_IN_IRAM=y` kept; new risk unbiased idle line tripping the silence check.
- concepts/firmware-architecture: new section 6 subsection "Stage-1 detail decisions (user, 2026-11-09)"; sections 1–4 adjusted where v0.3 text contradicted (console commands, fake-slave seam, ISR in IRAM, Kconfig caps, health/exception/stale rules, fallbacks, lint, write gate, GPIO defaults, site power); section 7 status (specs ready, 002 file renamed, old pointer to `git rm`); section 8: ISR gap resolved, panel LV isolation open, idle-bias risk, two lint details not restated (esp-modbus header location, `uart_set_loop_back`).
- concepts/project-goal: stage-1 specs ready, adjusted to the detail decisions.
- index.md summary for firmware-architecture.
- Noted, not edited (tasks/ is architect-owned): `tasks/README.md` rule 3 still says polling is off after boot until `mb probe` / `mb read` / `mb poll on`, contradicting the 2026-11-09 decision.

## [2026-11-09] sync | boot window, mb silence, mb log default
No raw files added or changed. User answers (session 2026-11-09, not in raw) plus the approved spec `tasks/001-skeleton-kconfig-lint.md` (read only, not edited).
- concepts/firmware-architecture: section 6 "Stage-1 detail decisions" gains three rows: console commands typed during the 5 s boot silence check run before the first poll (`mb poll off` / `mb addr N` stop or redirect the first frame), approved; `mb silence <s>` leaves polling off after a passed check until `mb poll on` (differs from boot), approved; `mb log` default ON from power-up (architect proposed off, user chose on). Section 2 console row and section 3 fallbacks updated accordingly; status note extended.
- concepts/firmware-architecture section 3 lint text rewritten as a summary of R1–R8 citing the task-001 spec; the two section-8 lint open questions resolved: R3 esp-modbus master/common headers only in `components/mb_link/src/` (`esp_modbus_slave.h` nowhere, R2); R4 `uart_write_bytes*`, `uart_tx_chars`, `uart_set_rts`, `uart_set_dtr`, `uart_set_loop_back`, `uart_set_line_inverse`, `_uart_set_pin*` banned everywhere.
- index.md summary for firmware-architecture.
- The 2026-11-09 note about `tasks/README.md` rule 3 (polling off after boot) is fixed by the architect (checked by wiki-keeper: rule 3 now says polling starts automatically after a clean boot check).

## [2026-11-24] sync | task 009 accepted
No raw files added or changed. Session implementation facts (verified build/test output, not in raw).
- Task 009 `daikin_regs` implemented by `firmware-dev` (sonnet) as a pure C99 component (`firmware/components/daikin_regs/`, 32-row table + write_mask, host tests in `test_host/`, added to host REQUIRES); accepted by `firmware-reviewer` (APPROVE, 1 MINOR on overflow-check precision) and `build-verifier` (target 0 warnings; 26/26 host tests; lint self-test + scan clean).
- Two pre-existing skeleton bugs from task 001 found and fixed in-session: wrong bounds-macro struct ordering (every row's bounds were bogus) and a uint16 overflow-check bug in `daikin_regs_read_allowed`; test expected table corrected independently.
- MINOR follow-up applied same session (firmware-dev, haiku): overflow threshold changed to `>= 0x10000u` (`>=`, justified deviation from reviewer's suggestion); re-verified (build/host-tests/lint clean).
- Nothing committed; nothing pushed. Working tree carries the edits.

## [2026-11-24] sync | task 009 committed and pushed
Two commits pushed to origin/main: firmware component + host REQUIRES edit; wiki + RESUME updates. Working tree clean (untracked `.vscode/` and `ProjectDefinitionPrompt.txt` intentionally not committed).

## [2026-11-09] sync | task 001 accepted, session end
No raw files added or changed. Session implementation facts (verified build/test output, not in raw): task 001 implemented by `firmware-dev`, accepted 2026-11-09 after two review rounds (`firmware-reviewer` accepted, `build-verifier` all checks pass); committed in the session-end commit with the amended spec 001.
- concepts/firmware-architecture section 7: 001 done, 009 next; open minor (self-test lacks explicit `^= -= *= /= %= >>=` cases).
- concepts/project-goal: tasks line, 001 done, 009 next.
- concepts/dev-environment: new "Host build facts" section (`idf.py --preview set-target linux`, `libbsd-dev` needed, user action pending; esp-clang/clang-tidy not installed; lint stdlib Python on every build); open question 3.
- index.md summaries for project-goal, dev-environment, firmware-architecture.

## [2026-11-24] sync | task 010 decisions U1-U4
No raw files added or changed. User decisions (session 2026-11-24, not in raw) and session implementation facts (verified build/test output, not in raw).
- Task 010 (`reg_model` + `poll_sched` + `txn_log`) implemented by `firmware-dev`; `build-verifier` PASS (esp32s3 target 0 warnings; host tests 85/0 failures/0 ignored; lint + 82-case self-test clean; deps registry-only; clang-tidy skipped, not installed); `firmware-reviewer` APPROVE WITH MINORS (no blocker/major). Not yet accepted, not committed.
- concepts/firmware-architecture: new section 6 subsection "Task 010 detail decisions (user, 2026-11-24)" with U1–U4; section 3 "Read errors and health" and "Fallbacks" adjusted (manual read while OFFLINE, other exceptions not unsupported in `reg_model`, exc 02 on RETRY = unsupported, `mb poll on` no-op unless STOPPED, `mb addr` reset); section 7 update (009 done, 010 status); header addendum.
- concepts/project-goal: tasks line (009 done, 010 status).
- index.md summaries for project-goal and firmware-architecture.

## [2026-11-24] sync | task 010 decisions U5-U6, spec 010 amended
No raw files added or changed. User decisions (session 2026-11-24, not in raw): U5 and U6 are architect proposals confirmed by the user. Spec `tasks/010-reg-model-poll-sched.md` amended by the architect (read only here, not edited by the wiki-keeper).
- concepts/firmware-architecture: header addendum and section 6 "Task 010 detail decisions" intro updated (U1–U6, spec amendment done incl. reviewer minors: `reg_model_set_exception`, txn_log enum bounds check, mixed-kind gap soak test); rows U5 (HEALTH exc 02 on 000 = failed read, not unsupported) and U6 (completion for old address after `mb addr` = statistics and `last_end` only; `bus_task` calls `set_addr` only between `poll_step` calls) added; section 7 status: amendment done, firmware-dev (sonnet) follow-up in progress, then build-verifier + firmware-reviewer re-check.
- index.md summary for firmware-architecture updated (U1–U6).
- Not changed, flagged: concepts/firmware-architecture section 3 "Read errors and health" still says "Exception 02 -> register unsupported" without the HEALTH exclusion of U5 (U3/U5 keep HEALTH out of the unsupported rule); concepts/project-goal and its index summary still say the spec amendment is pending.

## [2026-11-24] sync | task 010 accepted
No raw files added or changed. Session implementation facts (verified build/test output, not in raw; nothing committed).
- Task 010 (`reg_model` + `poll_sched` + `txn_log`) amended and re-worked by `firmware-dev` (sonnet) after the spec amendment U1–U6 and reviewer minors: `reg_model_set_exception`, `poll_sched` start no-op unless STOPPED, RETRY in rule 6, rule 12 address mismatch, txn_log enum bounds, tests for U1–U6, rule 12, sweep end and a 2 h mixed-kind gap soak.
- `build-verifier` re-check PASS: esp32s3 fresh build 0 warnings; host tests 100 Tests, 0 Failures, 0 Ignored, exit 0; bus-access lint and 82-case self-test clean; no `esp_`/FreeRTOS includes; dependencies registry-only; clang-tidy SKIPPED (not installed).
- `firmware-reviewer` re-review: APPROVE, no findings of substance. FYI, no change: `mb poll off` then `mb poll on` from OFFLINE restarts polling immediately (consequence of U4, bounded by the 5-failure re-entry).
- Task 010 accepted 2026-11-24.
- Open hygiene minor: host build prints 4 CMake `component_validation` warnings because `firmware/test/host/main/CMakeLists.txt` globs component `test_host` sources into main (pre-existing pattern from 009, cosmetic). Known: `idf.py set-target linux` needs `--preview`.
- concepts/firmware-architecture: section 3 "Read errors and health" exception-02 line now states its scope (POLL/RETRY/SWEEP/MANUAL qty 1, not HEALTH; U3, U5); section 7 status: 010 done, 002 next.
- concepts/project-goal: tasks line (001, 009, 010 done; 002 next); index summary no longer "amendment pending".
- index.md summaries for project-goal and firmware-architecture updated.

## [2026-11-24] sync | task 010 committed and pushed
Two commits pushed to origin/main: `53e2c49` (firmware: reg_model, poll_sched, txn_log, host REQUIRES) and `6623c43` (spec 010 U1-U6 amendment, tasks/README, wiki, RESUME). Working tree clean (untracked `.vscode/` and `ProjectDefinitionPrompt.txt` intentionally not committed).

## [2026-11-24] sync | task 002 S3 decision E2
No raw files added or changed. User decisions (session 2026-11-24, not in raw) and task-002 spike facts (project evidence: `docs/spikes/002-esp-modbus.md`, source reading of esp-modbus 2.1.4 / IDF v6.0.2, verified by `firmware-reviewer` and the orchestrator; not a raw source).
- Spike stopped at S3: exception, CRC/malformed and CRC-valid wrong-byte-count replies all -> `ESP_ERR_INVALID_RESPONSE`, exception code dropped; timeout distinguishable; frames <= 3 bytes dropped -> timeout. S5: RTS drives DE (low idle, high TX); pin order set_mode before set_pin; DE re-drive after driver delete. Other esp-modbus findings kept as history. 2.1.4 = latest registry version (fetched 2026-11-24, external).
- User: E2 (own FC03-only RTU master: pure `mb_rtu`, `rs485_port` owns UART, `mb_link` API unchanged); E1a/E1b/E3 rejected; D7 and Q9 reversed; registry-only rule: own code compliant; lint/ELF without esp-modbus (`uart_write_bytes` only in `rs485_port/src/`, FC bytes only in `mb_rtu`, ELF forbids `mbc_`/`mbm_`/`mbs_` and `mb_link_write`); 002 split into three tasks (architect writing specs); DE safety rules added to inspection checklists.
- concepts/firmware-architecture: header addendum, Tags (project evidence), Q9 note; section 2 (components `mb_rtu`/`mb_link`/`rs485_port`, Modbus layer E2 with superseded D7 text kept, dependencies, threading); section 3 (silence check wording, E2 lint/ELF change, ELF note); section 4 (DE = RTS resolved, pin order, re-drive); section 6 (D7/D7b/D17 rows, new subsection "Task 002 spike decisions", D19 conflict under own heading); section 7 (002 stopped, three replacement tasks); section 8 (esp-modbus item closed, two new risks).
- concepts/dev-environment: esp-modbus removed from approved components (history kept), registry-only interpretation for own code, open question 2 note.
- concepts/project-goal: tasks line. index.md summaries for project-goal, dev-environment, firmware-architecture.
- Not changed: entities/max485, concepts/modbus-rs485-link (neither states the DE/RTS mechanism).
- Discrepancy recorded on firmware-architecture section 6: spike says "no side channel" for the exception code; review names a received-frame debug log (`mb_master.c:610-611`) as a fragile side channel.

## [2026-11-24] sync | E1b fallback, 012-014 spec answers
No raw files added or changed. User decisions (session 2026-11-24, not in raw) and session facts (architect specs, not committed).
- E2 confirmed after a pros/cons review; E1b (esp-modbus ==2.1.4, every invalid reply = BAD_FRAME, exc_code 0, retry once, counts toward OFFLINE) kept as documented, unbuilt fallback; revert = separate user decision; `mb_link.h` is the single seam; losses under E1b listed (D21 exc 02 detection, U2/U3/U5 codes, probe items 4/5); revert reference `docs/spikes/002-esp-modbus.md`. Earlier same-day text listing E1b as rejected corrected (history noted).
- Answers to spec questions: gap bytes counted and discarded, request still sent; `mb_link_get_counters` (gap bytes, faults, max inter-byte interval) in `mb status`, used by 008 D.8a; `mb_link` host test with fake `rs485_port` rejected (code inspection instead); exception code 0x00 -> BAD_FRAME; rejected CONFIG_FREERTOS_HZ=1000 (10-20 ms frame-end window known limit) and a bench slave-simulator task.
- `.claude/agents/architect.md` and `orchestrator.md` corrected by the orchestrator (user-approved) to "sole master on the EKWHCTRL1 'A A B B -' port" (D1).
- Specs 012 (`mb_rtu`), 013 (lint/ELF, before 014), 014 (`rs485_port` + `mb_link`) ready; 002 retired note; 006/008/010/001/README amended by the architect. Order 012 -> 013 -> 014 -> 006 -> 008, 011 any time; all sonnet; independent firmware-reviewer review for 012/013/014. Lint compound-operator minor scheduled in 013.
- concepts/firmware-architecture: header addendum; section 2 (mb_link counters, Modbus layer: E1b fallback, task files); section 3 (gap bytes, FREERTOS_HZ, exc 0x00, lint/ELF task 013); section 6 (E2 row, new rows E1b..Task order); section 7 (status, table 002 retired + 012/013/014, order); section 8 (inter-byte timing risk corrected from INN-FR-B32 to the answering slave on "A A B B -", orchestrator wording error; E1b fallback note).
- concepts/project-goal: tasks line. index.md summaries for project-goal and firmware-architecture.

## [2026-11-24] sync | measured flag; specs 012-014 committed
User decision (session 2026-11-24, not in raw): keep the `measured` flag in `mb_link_counters_t` (architect proposal, approved). No raw files changed.
- Commits (pushed to origin/main): `3310030` (specs 012-014 + spike report `docs/spikes/002-esp-modbus.md`), `895d7ee` (wiki: S3 decision E2, E1b fallback, spike facts), `236f047` (`.claude/agents/architect.md` and `orchestrator.md` fix: sole master on "A A B B -", D1).
- concepts/firmware-architecture: section 6 "Task 002 spike decisions" new row "Seam types and `measured` flag" (`mb_link.h` own `mb_link_port_cfg_t` and `mb_link_counters_t`, no `rs485_port.h` include, seam rule spec 014; E2 `measured = true`; E1b `measured = false`, zeros, `mb status` prints `link counters: n/a`); section 2 `mb_link` row points to it.
- index.md summary for firmware-architecture updated.

## [2026-11-24] sync | task 012 accepted
No raw files added or changed. Session facts (task 012 implementation and review, verified output, not in raw; not committed).
- Task 012 (pure `mb_rtu`, `firmware/components/mb_rtu/`, host REQUIRES in `firmware/test/host/main/CMakeLists.txt`) implemented by `firmware-dev` (sonnet). `build-verifier` PASS: esp32s3 fresh build 0 warnings; host tests 118 Tests, 0 Failures, 0 Ignored; bus-access lint rc 0, self-test 82 ok; no esp_/FreeRTOS/driver includes; registry-only, no esp-modbus; clang-tidy SKIPPED (not installed).
- `firmware-reviewer`: APPROVE WITH MINORS. Independent ASan/UBSan build: builder (1, 0, 1) gives `01 03 00 00 00 01 84 0A`; CRC vectors 0x4B37 / 0x1241; t3.5 at 9600 = 4011 us; 3M random-frame fuzz clean.
- Minors: (a) `val == NULL` moved to the caller-error rule (BAD_FRAME, after TIMEOUT), spec 012 rule 2 amended; (b) 19201 -> 1750 t3.5 test; both applied by `firmware-dev` (haiku), re-verified 118/0/0, 0 warnings, lint rc 0. (c) `mb_rtu_fc03_reply_len` does not validate qty: spec 014 amended (reply-length adapter in `mb_link` maps UNKNOWN and > `MB_RTU_MAX_REPLY_LEN` to "more"; rx buffer `MB_RTU_MAX_REPLY_LEN + 1` with write guard; acceptance 3k).
- Task 012 done, accepted 2026-11-24, not committed. Next: 013, then 014, 006, 011, 008.
- concepts/firmware-architecture: section 2 `mb_rtu` row; section 7 update paragraph, table rows 012/013/014, order line.
- concepts/project-goal: tasks line. index.md summaries for project-goal and firmware-architecture.
- Not changed: `tasks/` (spec 012/014 amendments and `tasks/README.md` are already modified in the working tree, edited by the architect; the README 012 status line may still read "spec ready").

## [2026-11-24] sync | task 012 committed and pushed
No raw files added or changed. Git facts (verified with `git log` / `git fetch` in this session, not in raw).
- Pushed to origin/main: `568ec8a` (Add mb_rtu FC03 RTU frame codec with host tests, task 012; `firmware/components/mb_rtu/`, host REQUIRES in `firmware/test/host/main/CMakeLists.txt`) and `32709f7` (Record task 012 acceptance; bound reply length in spec 014; wiki section 7, project-goal, index, log, RESUME, `tasks/012`, `tasks/014`, `tasks/README.md`).
- Task 013 (lint + ELF check without esp-modbus) handed to `firmware-dev` (sonnet); in progress. No 013 file changes were visible in the working tree when this entry was written.
- RESUME.md updated: last step, status (origin/main = `32709f7`), next steps, start prompt unchanged.
- Not changed: concepts/ and index.md (no content change in this step).

## [2026-11-24] sync | task 013 review decisions
No raw files added or changed. User decisions (session 2026-11-24, not in raw; cited as user decision, session 2026-11-24) and session facts (task 013 first pass, verified output, not in raw; not committed).
- Task 013 first pass by `firmware-dev` (sonnet): R1/R3/R4/R8 rewritten for E2 (self-test 218 cases); `firmware/tools/check_elf_symbols.py` (24 self-test cases, stdlib only) as POST_BUILD hook on the ELF (`CMAKE_NM` = `xtensa-esp32s3-elf-nm`); `DMT_ELF_REQUIRED_SYMBOLS` empty until 006; esp-modbus gone from manifests, lock file and `managed_components`. `build-verifier` PASS (esp32s3 0 warnings; host 118/0/0; ELF `forbidden 0 found; required: none configured`). Five guard proofs failed the build as expected; the ELF guard proof needed `noinline` plus a side effect (an empty function was inlined away). `firmware-reviewer`: APPROVE WITH MINORS, bypass table (not copied; open).
- User decisions L1–L9 (fix now): include detection (`#include`, `#import`, `%:include`, comments between `#` and the directive); deterministic comment regex; `++`/`--` banned on function-code fields; manifests match `esp[-_]modbus`; ELF check fails on empty nm output or missing `app_main`; stricter UART bans outside `components/rs485_port/` (low-level TX paths, `uart_set_*` setup calls, `uart_flush`, `uart_intr_config`, `uart_isr_register`, `uart_enable_tx_intr`); octal/binary literal bans not adopted; documented remaining limits; consequence for 014 (names `fc`/`func`/`command`, strings like `"fc=%d"` fail R8; FC hex constants decimal or named).
- Order after the fix: spec 013 amendment (architect, running) -> 013 follow-up (`firmware-dev`) -> `build-verifier` + `firmware-reviewer` -> accept; commit on request. Then 014, 006, 011, 008.
- concepts/firmware-architecture: section 3 lint summary (013 status, ELF behaviour, stricter UART bans); section 6 new subsection "Task 013 review decisions (user, 2026-11-24)" L1–L9; section 7 update paragraph and 013 row (compound-operator minor closed by 013).
- index.md summaries for firmware-architecture and project-goal; concepts/project-goal tasks line updated (013 status).
- Not changed: `tasks/`, `firmware/`, `raw/`, `.claude/`.

## [2026-11-24] sync | task 013 accepted
No raw files added or changed. User decisions (session 2026-11-24, not in raw) and session facts (verified output, not in raw; not committed).
- Task 013 follow-up (L1–L9, spec 013 amended by the architect, applied by `firmware-dev` on sonnet) verified by `build-verifier` PASS: esp32s3 fresh build 0 warnings; lint self-test 343 cases (0.24 s), tree scan rc 0; ELF self-test 32 cases, fails on empty `nm` output and missing `app_main` (`--nm /bin/true` exit 1); ELF summary `forbidden 0 found; required: none configured`. `firmware-reviewer` re-review: APPROVE WITH MINORS, all earlier findings closed.
- Host-test re-run NOT done (blocked by the permission classifier). User accepted 013 without it: no C code changed in 013; `firmware-dev` reported 118 / 0 / 0.
- User accepted task 013 "as is" on 2026-11-24.
- Decided, NOT yet implemented: ban the remaining ROM/register UART TX routes everywhere (R4): `esp_rom_output_tx_one_char`, `esp_rom_uart_putc` (pattern `esp_rom_(output|uart)_\w*` for TX names), `UART[0-2]->`, `REG_WRITE` / `WRITE_PERI_REG` on `UART_FIFO\w*_REG`, `soc/uart_reg.h` include. Lint follow-up before task 014.
- Open minors accepted as is for the acceptance: (1) prefix `++a[b[1]].fc` with nested brackets not detected (document); (3) docstring claim "matching is linear" false for unterminated/repeated `/*` (correct docstring or cap gap); (4) R5 `_second_argument` quadratic on 3000 × `uart_set_mode(` (pre-existing, mention); (5) self-test 1.0 s per-case timing guard may fail on a loaded machine (consider 5 s). Caller's plan: minors 1, 3, 4, 5 go into the same lint follow-up as the ROM/register ban.
- Earlier inference "R5 touched by the follow-up" confirmed correct (ten new UART setup names only in `components/rs485_port/`).
- concepts/firmware-architecture: section 3 lint summary with rule numbers R3/R4/R5/R8 as implemented; section 6 new subsection "Task 013 acceptance" (rows A1–A4); section 7 update paragraph, table row 013 and order line.
- concepts/project-goal: tasks line. index.md summaries for project-goal and firmware-architecture.
- Not changed: `tasks/`, `firmware/`, `raw/`, `.claude/`. Not committed.

## [2026-11-24] sync | task 013 pushed; follow-up decisions Q-a..Q-c
No raw files added or changed. User decisions (session 2026-11-24, not in raw; cited as user decision, session 2026-11-24). Git facts from the local `git log` (pushed status user-reported).
- Pushed: `fc5b50e` (013 lint/ELF check) and `19a3e94` (013 acceptance).
- 013 follow-up started, handed to `firmware-dev` (sonnet). Decisions: Q-a yes (also ban `esp_rom_install_*`, e.g. `esp_rom_install_uart_printf` can redirect output to a UART); Q-b yes (also ban `REG_READ` / `READ_PERI_REG` on `UART_FIFO*_REG`); Q-c no (no blanket `esp_rom_*` ban).
- Minors in the same follow-up: document nested-bracket prefix `++` as a limit; correct the "linear" docstring (no cap on the gap); document the quadratic R5 helper; self-test timing guard 1 s -> 5 s.
- Order: follow-up -> `build-verifier` + `firmware-reviewer` -> accept -> commit on request. Then 014, 006, 011, 008.
- concepts/firmware-architecture: section 6 rows A1 (status), A5–A12 (new); section 7 update paragraph and 013 row. index.md summary for firmware-architecture.
- Not changed: `tasks/`, `firmware/`, `raw/`, `.claude/`.

## [2026-11-24] sync | 013 follow-up accepted, 014 started
No raw files added or changed. User decisions (session 2026-11-24, not in raw; cited as user decision, session 2026-11-24) and session facts (verified output, not in raw). Git facts from the local `git log` (`9ca4517` present); pushed status user-reported.
- Pushed: `9ca4517` (Ban ROM and register UART TX routes in bus-access lint, 013 follow-up). Changed files: `firmware/tools/check_bus_access.py`, `firmware/README.md`.
- Follow-up content (rows A5–A12): R4 also bans `esp_rom_output_*`, `esp_rom_uart_*`, `esp_rom_install_*` (no blanket `esp_rom_*` ban), raw `UARTn.` / `UARTn->` access, `REG_WRITE` / `WRITE_PERI_REG` / `REG_READ` / `READ_PERI_REG` on `UART_FIFO*_REG` (bounded 200-character window, linear) and the `uart_reg.h` include, everywhere including `rs485_port`; limits documented; self-test timing guard 5 s; lint self-test 372 cases (0.3 s).
- `build-verifier` PASS: fresh esp32s3 build 0 warnings (self-tests 372 / 32); ELF `forbidden 0 found; required: none configured`; fresh host build and run 118 / 0 / 0 (closes the host-run gap of 013).
- `firmware-reviewer`: APPROVE WITH MINORS (200-character window acceptable). User accepted the follow-up as is, 2026-11-24.
- Open doc minors, not fixed (firmware-architecture section 6 row A14): (a) the FIFO rule can be bypassed with a `;` inside a comment or string in the `REG_*` argument list (add to limits); (b) R5 `_second_argument` on 5000 × `uart_set_mode(` takes more than 60 s (effectively a hang; state in limit text); (c) optional self-test case for an unterminated `/*`.
- Task 014 (`rs485_port` + `mb_link`) handed to `firmware-dev` (sonnet), in progress. Bus-touching, first TX code. Next: `build-verifier` and independent `firmware-reviewer`; escalation conditions in spec 014 are hard stops.
- concepts/firmware-architecture: section 3 R4 summary (follow-up bans) and ROM/register status; section 6 rows A1, A3, A4, A5 status and new rows A13, A14; section 7 update paragraph, rows 013 and 014, order line.
- concepts/project-goal: tasks line rewritten (done and pushed: 001, 009, 010, 012, 013 + follow-up; 014 in progress; stale "not committed" text removed). index.md summaries for project-goal and firmware-architecture.
- Not changed: `tasks/`, `firmware/`, `raw/`, `.claude/`. Not committed.

## [2026-11-24] sync | task 014 review decisions
No raw files added or changed. User decisions (session 2026-11-24, not in raw; cited as user decision, session 2026-11-24) and session facts (verified output and `firmware-reviewer` reading of the local IDF v6.0.2 source, project evidence, not in raw; not committed).
- Task 014 (`rs485_port` + `mb_link`, implemented by `firmware-dev` on sonnet): `build-verifier` PASS (fresh esp32s3 build 0 warnings; `librs485_port.a` and `libmb_link.a` built; lint 372 and ELF 32 self-tests; ELF `forbidden 0 found; required: none configured`; fresh host 118 / 0 / 0; no dependency added). No escalation condition hit. `firmware-reviewer`: APPROVE WITH MINORS.
- Reviewer's DE and ISR facts (local IDF source, not hardware-verified): DE high only during our 8-byte transmit; RTS asserted before FIFO fill (`uart.c:1191`), released in TX_DONE ISR (`uart.c:1506`); half-duplex sets `sw_rts` = 1 (RTS low, `uart_ll.h:773-783`); mode before pins; DE low before output enable (`gpio.c:228-229`); driver delete floats DE on the 10 kΩ pull-down; no reply byte lost to ISR RX-FIFO reset (`uart.c:1505`). `CONFIG_UART_ISR_IN_IRAM=y` is load-bearing for the DE release.
- User decisions on 014 minors: M1 IRAM interrupt flag on `uart_driver_install` (removes IDF "flag updated" log); M2 `#error` if `CONFIG_UART_ISR_IN_IRAM` is off; M3 document `gap_bytes` as lower bound and `max_byte_interval_us` as approximate; M4 reject 0 ms silence check. Fix now, then re-check, accept, commit (user approved the commit after the fix). `firmware-dev` applies M1–M4 (in progress).
- User decisions on 006 (architect amended spec): mute IDF `uart` log to WARN; failed hold-receive at boot is fatal; no read before a clean silence check.
- User decisions on 008 (architect amended spec): bench/site DE checks at boot, open, per transaction, with concurrent flash/NVS activity, at silence check and at close; measure slave turnaround; no answering slave gives TIMEOUT; 10–20 ms end-of-frame wait accepted.
- concepts/firmware-architecture: section 3 (ISR-in-IRAM load-bearing, counters approximate); section 4 DE timing facts; section 6 new subsection "Task 014 review decisions (user, 2026-11-24)"; section 7 update paragraph and 014 row, order line; section 8 risks (flash/NVS stall vs DE release; counters approximate).
- concepts/project-goal: tasks line (014 implemented and verified, M1–M4 pending). index.md summaries for project-goal and firmware-architecture.
- Not changed: `tasks/`, `firmware/`, `raw/`, `.claude/`. Not committed.

## [2026-11-24] sync | task 014 accepted
No raw files added or changed. User decision (session 2026-11-24, not in raw; cited as user decision, session 2026-11-24) and session facts (verified output and `firmware-reviewer` re-review, not in raw).
- Task 014 M1–M4 fixes applied by `firmware-dev` (sonnet): M1 IRAM interrupt flag on both `uart_driver_install` calls; M2 `#error` unless `CONFIG_UART_ISR_IN_IRAM`; M3 counter notes in `rs485_port.h` and `mb_link.h`; M4 silence check rejects 0 ms and NULL. `build-verifier` PASS: fresh esp32s3 build 0 warnings; `librs485_port.a` and `libmb_link.a`; lint 372 and ELF 32 self-tests; ELF `forbidden 0 found; required: none configured`; guard proof (build with `CONFIG_UART_ISR_IN_IRAM` off fails with the `rs485_port` `#error`); fresh host 118 / 0 / 0. `firmware-reviewer` re-review: APPROVE.
- User accepted task 014 on 2026-11-24. Firmware committed and pushed as `987fbd1` ("Add rs485_port and mb_link FC03 read path over own RTU codec (task 014)").
- Open questions for the user, recorded (not answered), to be settled before task 008 is finalised: Q-HW1 (is an oscilloscope or logic analyser available on the bench or on site; needed for 008 B.6 and D.8b, otherwise "not measured") and Q-HW2 (the phase-1 image cannot generate flash/NVS load, so 008 B.6 "DE with concurrent flash/NVS activity" is "not done in phase 1, repeat at phase-2 bring-up", unless a test console command is added to 006).
- concepts/firmware-architecture: section 6 status line of "Task 014 review decisions" updated; new subsection "Task 014 acceptance" (K1–K5); section 7 014 row, update paragraph and order line; section 8 new open questions Q-HW1 and Q-HW2; header unchanged.
- concepts/project-goal: tasks line (014 done, next 006, 011, 008). index.md summaries for project-goal and firmware-architecture updated.
- Not changed: `tasks/`, `firmware/`, `raw/`, `.claude/`. Not committed.

## [2026-11-24] sync | task 014 docs pushed (4752afd), 006 started
No raw files added or changed. Session facts (local `git log` and `git status -sb`: `4752afd` on `main`, in sync with `origin/main`; firmware-dev handoff reported by the caller, not in raw). User decision (session 2026-11-24, not in raw; cited as user decision, session 2026-11-24) for the 006 handoff.
- Commit `4752afd` ("Record task 014 acceptance and review items in specs and wiki") follows firmware `987fbd1`. It holds the wiki log entries and the `tasks/` spec updates for 006, 008, 013, 014 and `tasks/README.md`.
- Task 006 (bus_task, probe console, `app_main`; first flashable probe image) handed to `firmware-dev` (sonnet), in progress. Not verified, not committed. Next: `build-verifier`, then `firmware-reviewer`, then user acceptance. The image has no flash/NVS test command, so Q-HW2 stays open.
- Open questions unchanged and still for the user: Q-HW1 (oscilloscope or logic analyser), Q-HW2 (flash/NVS load command in the phase-1 image). The 013 doc minors (firmware-architecture section 6 row A14) stay open.
- Not changed in this step: `firmware/`, `tasks/`, `raw/`, `.claude/`, `index.md` (no wiki page changed). RESUME.md updated. Not committed.

## [2026-11-24] sync | task 006 review decisions
No raw files added or changed. User decisions (session 2026-11-24, not in raw; cited as user decision, session 2026-11-24) and session facts (verified output of `build-verifier` and the `firmware-reviewer` review, not in raw). Not committed.
- Task 006 (`bus_task`, `mb` probe console, `app_main`; first flashable probe image) implemented by `firmware-dev` (sonnet). `build-verifier` PASS: fresh esp32s3 build 0 warnings; lint 372 and ELF 32 self-tests; ELF `forbidden 0 found; required: mb_link_read, mb_rtu_build_fc03 present`; image 236 KB, 77% of the app partition free; no Wi-Fi, lwip, netif, BT or Matter code linked (`nvs_flash` and `esp_phy` members are force-included by the IDF startup, not used by project code); host 118 / 0 / 0; no dependency added. `firmware-reviewer`: APPROVE WITH MINORS (boot order, single-owner rule, non-blocking output, console-off-bus-UART guard and timing loop verified).
- User decision: fix all minors before commit. T1 boot hint from real state (never "silence ok" while latched); T2 status snapshot at boot and when `mb silence` stops polling; T3 `mb silence` cancels a pending manual read (small `poll_sched` API addition, spec 010 amended); T4 console-UART runtime check before `bus_task` starts; T5 "faults since boot" wording and one `# mb_link: port fault ...` comment line after a fault; T6 `log_out` priority comment.
- T7: the reviewer's 7 on-site checks are added to spec 008 (stack high-water after boot and after `mb silence 1`; abort path by absence of T lines; wait out `mb silence`; pending read cancelled; no "queue free spaces" line; latch lines; console port).
- The architect amends specs 006, 008 and 010 (running). `tasks/` not changed by the wiki-keeper.
- concepts/firmware-architecture: section 6 new subsection "Task 006 review decisions (user, 2026-11-24)"; section 7 006 update paragraph, 006 row and order line; section 8 new risk (`bus_task` 4 KB stack to verify on site).
- concepts/project-goal: tasks line (006 implemented and verified, minors to fix before commit). index.md summaries for project-goal and firmware-architecture updated.
- Open, unchanged: Q-HW1 (oscilloscope or logic analyser), Q-HW2 (no flash/NVS load command in the 006 image); firmware-architecture section 6 row A14 (013 doc minors).
- Not changed: `tasks/`, `firmware/`, `raw/`, `.claude/`. Not committed.

## [2026-11-24] sync | task 006 accepted
No raw files added or changed. User decision (session 2026-11-24, not in raw; cited as user decision, session 2026-11-24) and session facts (verified output of `build-verifier` and the `firmware-reviewer` re-review, not in raw). The session labels the fixes M1–M6; the wiki uses T1–T6 for the same items (M1 = T1 … M6 = T6).
- Task 006 review fixes applied (T1–T7, section 6 "Task 006 acceptance" V1): boot hint from the real state; status snapshot at boot and around `mb silence` (`silence_running` flag, confirmed by the user; `# silence check: started <s> s` line; `mb status` shows `boot silence: <ms> ms, activity running`); `poll_sched_cancel_manual` called by `mb silence`; console-UART runtime check after the hold, before `bus_task` starts (on failure nothing starts and the hold stays); `faults since boot` and one `# mb_link: port fault, link closed ...` line on transition; `log_out` priority comment; spec 008 carries the reviewer's 7 on-site checks.
- `build-verifier` PASS: fresh esp32s3 build 0 warnings; lint 372 and ELF 32 self-tests; ELF `forbidden 0 found; required: mb_link_read, mb_rtu_build_fc03 present`; guard proof (a missing required symbol fails the build); image 236819 bytes, about 77 % of the app partition free; no Wi-Fi, lwip, netif, BT or Matter linked; host 119 Tests, 0 Failures (new cancel-manual test passes).
- `firmware-reviewer` re-review: APPROVE (minor on shared status text: spec aligned to the code).
- User accepted task 006 on 2026-11-24. Firmware committed and pushed as `b8ad00d` ("Add bus_task, mb probe console and app_main (task 006)"). This is the first flashable phase-1 probe image; not run on hardware yet.
- concepts/firmware-architecture: section 6 "Task 006 review decisions" status line and new subsection "Task 006 acceptance" (V1–V5); section 7 006 update paragraph, 006 row and order line; section 8 new "open after acceptance" bullet, 4 KB stack risk kept as to-verify.
- concepts/project-goal: tasks line (006 done, next 011, 008). index.md summaries for project-goal and firmware-architecture updated.
- Open, unchanged: Q-HW1 (oscilloscope or logic analyser), Q-HW2 (no flash/NVS load command in the 006 image); `bus_task` 4 KB stack to verify on site; firmware-architecture section 6 row A14 (013 doc minors).
- Not changed: `tasks/`, `firmware/`, `raw/`, `.claude/`. Not committed by the wiki-keeper (the firmware commit `b8ad00d` is the user's).

## [2026-11-24] sync | session suspended
No raw files added or changed. Session facts (local `git log` / `git status`, not in raw). Task 006 spec and wiki docs committed and pushed as `564fcc5` (on `main`, in sync with `origin/main`). User suspended the session at this point. Working tree clean except untracked `.vscode/` and `ProjectDefinitionPrompt.txt` (intentionally not committed).
- Next session starts with RESUME "Next" item 1: task 011 (fake-slave host tests and end-to-end host tests).
- RESUME.md: last step, status (tooling note on RTK-rewritten read-only Bash calls being blocked for subagents), start prompt unchanged.
- Not changed: `index.md`, concepts/, entities/, sources/, `tasks/`, `firmware/`, `raw/`, `.claude/`. Not committed.

## [2026-11-24] sync | Q-HW1 / Q-HW2 answered; task 011 in progress
No raw files added or changed. User decision (session 2026-11-24, not in raw) and session facts (not in raw).
- Q-HW1 answered: no oscilloscope and no logic analyser, on the bench or on site. 008 B.6 (DE timing) and D.8b (slave turnaround) are logged "not measured"; only console counters and timestamps are evidence.
- Q-HW2 decided: defer. 008 B.6 "DE with concurrent flash/NVS activity" is not done in phase 1, repeat at phase-2 bring-up. No test console command added to the 006 image.
- New residual risk: DE timing, incl. the load-bearing ISR-in-IRAM assumption, stays unmeasured in phase 1; re-check at phase-2 bring-up where possible (method without a scope still open).
- Session facts: task 011 (fake-slave host tests) handed to `firmware-dev` (sonnet), in progress, not verified. The `architect` is finalising spec 008 with these answers (in parallel; `tasks/` not touched by the wiki-keeper).
- concepts/firmware-architecture: section 8 Q-HW1/Q-HW2 moved from open to answered/decided (question text kept, answer + date added), new residual-risk bullet, "open after 006 acceptance" bullet updated; section 6 K5 row, 006 Q-HW2 note and V5 row annotated; section 7 new update paragraph (answers, 011 in progress, 008 being finalised).
- concepts/project-goal: tasks line "Next" no longer lists Q-HW1/Q-HW2 as blocking. index.md summaries for project-goal and firmware-architecture updated.
- Not changed: `tasks/`, `firmware/`, `raw/`, `.claude/`. Not committed.

## [2026-11-24] sync | spec 008 finalised (F1–F4); task 011 implemented
No raw files added or changed. User decisions (session 2026-11-24, not in raw; cited as user decision, session 2026-11-24) and session facts (not in raw). Not committed.
- Spec 008 finalised by the `architect` (session fact): Q-HW1/Q-HW2 applied (B.6 six DE timing items and D.8b turnaround "not measured (no instrument, Q-HW1)"; flash/NVS DE run "not done in phase 1, repeat at phase-2 bring-up (Q-HW2)"); indirect evidence (multimeter static DE level, multimeter averaged DE level during `mb probe`, complete OK replies + counters) recorded but not counted as pass; stale console texts aligned to `b8ad00d`. Awaiting user approval of the spec.
- User decisions F1–F4: F1 heap/leak check after UART delete/re-install (008 B.5) "not checked (no heap output in the 006 image)" in phase 1, no 006 change; F2 exit item 6 answered by `dur_ms` plus derived upper bound (about `dur_ms` - 25 ms, "derived, not measured"), pure turnaround "unknown, no instrument", replaces 008-b (row annotated, not deleted); F3 new hard stop 008 B.6.b (averaged DE near 3.3 V during `mb probe` on the bench -> record and stop, back to the architect; in `tasks/README.md` Escalations); F4 phase-2 DE re-check method decided later (section 8 open question).
- Task 011 implemented by `firmware-dev` (sonnet), not yet verified: `fake_slave.{h,c}`, `test_e2e_poll.c`, fixtures `README.md`, `healthy-synthetic.log`, `starred-timeout-synthetic.log`; implementer reports host 137 / 0 / 0 (was 119), 0.04 s, 10 scenarios fail under mutation, no contradiction with spec 010, no component code changed (orchestrator `git status`). Deviation `fake_tag_last()` (`ps_link_fn` carries no request kind), pending review. `build-verifier` and `firmware-reviewer` running.
- concepts/firmware-architecture: section 1 exit item 6 note (F2); section 6 rows 008-a and 008-b annotated, new subsection "Spec 008 finalisation (user, 2026-11-24)" (F1–F4); section 7 stale "needs Q-HW1/Q-HW2" texts annotated, new update paragraph, rows 008 and 011, order line; section 8 residual-risk bullet extended, new open question (F4), new hard stop (F3), open note on the 25 ms basis, "open after 006 acceptance" bullet updated.
- concepts/project-goal: tasks line. index.md summaries for project-goal and firmware-architecture updated.
- Open: phase-2 DE re-check method (F4); basis of the "about 25 ms" in F2 not in the session facts; 011 verification results; 008 spec approval.
- Not changed: `tasks/`, `firmware/`, `raw/`, `.claude/`. Not committed.

## [2026-11-24] sync | task 011 verified and reviewed; user decision on the major
No raw files added or changed. Session facts (verified output, not in raw) and user decision (session 2026-11-24, not in raw). Not committed.
- `build-verifier` on 011: PASS (host 137 / 0 / 0, 0 warnings, 2.2 s; esp32s3 0 warnings; lint rc 0, self-test 372; ELF required `mb_link_read`, `mb_rtu_build_fc03` present, forbidden 0; 0 fake/e2e symbols in the target ELF; no component/main code changed; IDF-only dependencies; clang-tidy skipped).
- `firmware-reviewer` on 011: REQUEST CHANGES. Major: D16 gap rule asserted only in scenarios 1 and 6; five `poll_sched` gap mutations uncaught by the e2e tests (test-coverage gap, not a code fault). All 8 implementer deviations accepted. Minors m2–m6; informational: sweep-while-OFFLINE caught only by unit tests.
- User decision: fix the major only (gap enforced in the fake on every request + asserted at the end of every scenario, proven with the uncaught mutations); m2–m6 stay open. Fix running with `firmware-dev` (sonnet); then re-check and user acceptance.
- concepts/firmware-architecture: section 6 new subsection "Task 011 review decisions (user, 2026-11-24)"; section 7 new update paragraph, row 011 and order line annotated; section 8 new note (m5) for turning the 008 probe log into a fixture.
- concepts/project-goal: tasks line. index.md summaries for project-goal and firmware-architecture updated.
- Open: the two of the 8 accepted deviations not named in the session facts.
- Not changed: `tasks/`, `firmware/`, `raw/`, `.claude/`. Not committed.

## [2026-11-24] sync | task 011 accepted (`17825cb`); spec 008 approved
No raw files added or changed. Session facts (verified output, not in raw) and user decisions (session 2026-11-24, not in raw). Not committed.
- 011 gap major fixed by `firmware-dev` (sonnet): `fake_link` fails a request starting < `FAKE_DEFAULT_GAP_MS` (100 ms) after the previous request end (D16 end-to-start, first request exempt, no setter); `assert_gaps_respected` at the end of every scenario; new e2e cases 11a (MANUAL right after a request) and 11b (MANUAL right after the OFFLINE-entering failure). Implementer mutation proof (a)–(f) all caught.
- `build-verifier` re-check PASS (host 139 / 0 / 0, 0 warnings, 2.1 s; components/ and main/ unchanged; lint rc 0, self-test 372; target build skipped). `firmware-reviewer` re-review APPROVE (re-ran (b), (d), (e): caught); new minor m7 (11b wait-for-OFFLINE loop has no iteration cap; test-suite only, no section 8 entry).
- User decisions: task 011 accepted as is; m2–m7 stay open; spec 011 not amended (11a/11b and the in-fake gap rule recorded as extra scope beyond spec 011). Code committed as `17825cb`; push after the docs commit. Spec 008 approved; `firmware-dev` writing `docs/phase1-probe/checklist.md` and `measurement-log-template.md` (in progress), then `firmware-reviewer`.
- concepts/firmware-architecture: section 6 deviation list completed (8 of 8; closes the earlier open "two deviations not named"), new subsections "Task 011 acceptance" (G1–G6) and "Spec 008 approval", major row and 008 status line annotated; section 7 new update paragraph, rows 008 and 011, order line; section 8 "open after 006 acceptance" bullet updated.
- concepts/project-goal: tasks line. index.md summaries for project-goal and firmware-architecture updated.
- Open: phase-2 DE re-check method (F4); basis of the "about 25 ms" (F2); 011 minors m2–m7; 013 row A14 doc minors.
- Not changed: `tasks/`, `firmware/`, `raw/`, `.claude/`. Not committed.

## [2026-11-24] sync | task 008 docs reviewed; end-of-frame wait corrected; user decisions J1–J8
No raw files added or changed. Session facts (verified output, not in raw) and user decisions (session 2026-11-24, not in raw). Not committed.
- `firmware-dev` (sonnet) wrote `docs/phase1-probe/checklist.md` and `measurement-log-template.md` (uncommitted; console strings cited to 006 file:line). `firmware-reviewer`: APPROVE WITH MINORS (major: no step tagged as transmitting); all deviations accepted.
- Architect findings (orchestrator checked in code): `mb probe` suspended while OFFLINE (`poll_sched.c` rule 8) -> false-pass risk for B.6.b; `mb poll off`/`mb silence`/`mb addr` cancel a sweep; `link counters: n/a` returns early (no `faults since boot`).
- Conflict recorded (code vs spec 014 rule 4 / spec 008 / wiki): end-of-frame wait is about 20–30 ms (`rs485_port.c:413`), not 10–20 ms. User: code stays, texts corrected. Wiki amended with struck-through history.
- Turnaround bound derivation recorded (inferred): closes the "~25 ms basis" question; bound tightened to about `dur_ms` - 34 ms; sanity check OK qty-1 `dur_ms` >= about 36 ms.
- User decisions J1–J8 (safety items only: transmit tags, B.6.b, B.5, D.2, C.4; D.3 rule; new stop on `link counters: n/a`; timing figures). Remaining docs minors open.
- concepts/firmware-architecture: section 1 exit item 6 note; section 3 "Timing" amended; section 6 rows "Rejected extras", 008-b, 008-d, F2 and F2 status note amended, new subsection "Task 008 docs review decisions (user, 2026-11-24)" (H1–H5, conflict, derivation, J1–J8, open minors); section 7 new update paragraph, row 008, order line; section 8 "~25 ms" note closed, new hard-stop/rules, false-pass risk and open-minors bullets, frame-end risk amended, after-006 bullet updated.
- concepts/project-goal: tasks line. index.md summaries for project-goal and firmware-architecture updated.
- In progress: architect amends spec 008 and spec 014 rule 4; then docs update, re-review, user acceptance.
- Open: wording "unplug USB at once" in J2 recorded as given (exact trigger not stated); "first H line" taken verbatim.
- Not changed: `tasks/`, `firmware/`, `docs/`, `raw/`, `.claude/`. Not committed.

## [2026-11-24] sync | task 008 accepted (docs `4be04e3`); phase-1 tasks complete
No raw files added or changed. Session facts (verified output, not in raw) and user decisions (session 2026-11-24, not in raw). Not committed by the wiki-keeper.
- `architect` amended spec 008 and spec 014 rule 4; the two leftover "10–20 ms" statements corrected (014 rule 4 earlier sentence -> "20 ms (2 ticks), measured from the last byte, up to one tick longer"; `tasks/README.md` line 34 -> "about 20–30 ms"); only "was 10–20 ms" history notes remain in `tasks/`.
- `firmware-dev` updated `docs/phase1-probe/checklist.md` and `measurement-log-template.md` (S1–S9 applied; extra `[TRANSMITS]` tags: B.4.2, B.5.1 and B.11.1 "only if the 5 s window is missed", B.7, B.10, B.14.4, D.7).
- `firmware-reviewer` re-review: APPROVE WITH MINORS (S1–S9 verified against the code, tags correct, nothing unsafe). User: two minors fixed (B.4.3 tag; D.9 tag note names only `mb poll on`); orchestrator verified.
- User accepted task 008; docs committed as `4be04e3`. All phase-1 tasks done; probe image built, not flashed.
- Closed: J2 "unplug USB at once" applies only to the B.6.b hard stop; J7 "first H line" = first `H <t> <health>` console line after boot.
- concepts/firmware-architecture: section 6 rows J2/J7 clarified, conflict paragraph and status line annotated, new subsection "Task 008 acceptance" (P1–P6); section 7 new update paragraph, row 008, order line; section 8 B.6.b hard stop, J7 rule, open-minors bullet and after-006 bullet updated.
- concepts/project-goal: tasks line. index.md summaries for project-goal and firmware-architecture updated.
- Open (unchanged): 008 docs minors; phase-2 DE re-check method; 011 minors m2–m7; 013 row A14. New: S1–S9 items not listed individually in the session facts.
- Not changed: `tasks/`, `firmware/`, `docs/`, `raw/`, `.claude/`.

## [2026-11-24] sync | task 008 specs + wiki committed as `13b0591` and pushed (`17825cb`, `537200e`, `4be04e3`, `13b0591`); `main` in sync with `origin/main`, no page changes

## [2026-11-24] sync | working-mode decisions S1–S5, phase-2a gate change, wiki slimmed (S2)
No raw files added or changed. User decisions (session 2026-11-24, not in raw).
- S1 wiki updated once per task at acceptance (progress in `RESUME.md` and git); S2 slim the wiki; S3 full `firmware-reviewer` only for bus-touching code, else `build-verifier` + one review at feature end, no re-review loop for text-only fixes; S4 fewer, bigger tasks, short specs, `firmware-dev` sonnet (haiku for small edits); S5 open questions batched once per task, recommended defaults for minor items.
- Gate change: phase 2a (bus-independent Matter skeleton on a fake/simulated register model; no new bus code, no writes) may start now; phase 2b (real Modbus <-> Matter mapping, anything touching the bus) still waits for the probe log in `raw/` + `/capture`. Supersedes "stage 2/3 specs only after the probe log" for the bus-independent part only. Architect drafting the phase-2a plan.
- New page concepts/firmware-history (progress diary, moved verbatim with citations: header addenda, section 1 item 6 notes, superseded D7 and lint texts, task 002/013/014/006/008/011 subsections in full, section 7 narrative and old table, section 8 full text incl. closed items, superseded v0.2, pre-slimming wording of the compacted sections).
- concepts/firmware-architecture slimmed 678 -> 431 lines: section 1 phase 2a/2b; section 6 new "Working mode and gate" table, decisions as compact rows; section 7 compact task status table; section 8 one line per item, closed items moved.
- concepts/project-goal: gate change and working-mode bullet; pointers to firmware-history. index.md: one-line summaries for project-goal and firmware-architecture, new firmware-history entry.
- Not changed: `tasks/`, `firmware/`, `docs/`, `raw/`, `.claude/`. Not committed.

## [2026-11-24] sync | phase-2a plan approved; Matter controllers; ELF-check scope correction
No raw files added or changed. User decisions and user statements (session 2026-11-24, not in raw); controller notes marked external, unverified.
- Phase-2a plan (2a-1..2a-9): separate `firmware/matter/` project (esp_matter ^1.6.1, own sdkconfig/partitions, 16 MB inferred), reuses `daikin_regs` + `reg_model`, no bus code, DE not driven; EP1 read-only from `sim_source`, writes rejected, Fan in 2b; separate Matter bus-symbol check; tasks 2a-A (step 0 registry-only build stop) and 2a-B (ids pending); Q-HW2 moved to 2b.
- Controllers: chip-tool + Home Assistant (Core, separate `python-matter-server`) + Apple Home for 2a; SmartThings on site later. User setup: HA Core in Docker on Synology, Apple TV, site has SmartThings hub.
- Correction: `check_elf_symbols.py:22` forbids only `mbc_|mbm_|mbs_|mb_link_write`; "no Wi-Fi/lwip/Matter linked" was a manual `build-verifier` item, not tooling (orchestrator, local inspection). No earlier wiki text claimed tooling enforcement; clarification added.
- concepts/firmware-architecture: section 1 phase 2a, section 3 ELF bullet, section 5 intro, section 6 gate row + new "Phase-2a plan" table, section 7 rows 2a-A/2a-B, section 8 risks (Q-HW2 -> 2b, RAM, image size, step 0).
- concepts/dev-environment: ELF note, new "Matter controllers" table, open questions 2 and 4. concepts/project-goal: one line. index.md summaries.
- Not changed: `firmware/`, `tasks/`, `docs/`, `raw/`, `.claude/`. Not committed.

## [2026-11-25] sync | task 015 (Matter skeleton) build-verified; decision 2a-10 NONVOLATILE attributes
No raw files added or changed. Session facts (orchestrator + independent `build-verifier`, local inspection) and user decision (session 2026-11-25, not in raw). Task 015 not yet reviewed or committed; wiki kept small (S1).
- Decision 2a-10 "Accept + limit": esp_matter 1.6.1 creates both occupied setpoints, SystemMode and ControlSequenceOfOperation as NONVOLATILE (public API cannot clear it); keep the default, 016 updates only on change with `attribute::set_deferred_persistence`, architect adds a flash-wear estimate to spec 016; consequence: NVS write per change, stored value overrides initial value at boot until first update.
- concepts/firmware-architecture: section 3 "NVS wear" note; section 6 row 2a-10; section 7 rows 015/016 + "Task 015 verified facts" (step 0 passed, lock contents, build, image size/partitions, warnings, bus guard, C++ deviation, GPIO0 reset); section 8 three 2a risks closed, new open item (D1–D7, N16R8 flash/PSRAM inferred, host tests blocked by `libbsd-dev`).
- concepts/dev-environment: open question 2 step-0 result. index.md: firmware-architecture summary.
- Not changed: `raw/`, `firmware/`, `tasks/`. Not committed.

## [2026-11-25] sync | NVS wear conflict fixed; 2a-10 60 s deferral approved; 015/016 commits
No raw files added or changed. Sources: esp_matter 1.6.1 source (`esp_matter_data_model.cpp`, read by the `architect` 2026-11-25; external, not in raw), user decision (session 2026-11-25), git history (local inspection).
- concepts/firmware-architecture section 3 "NVS wear": old "no NONVOLATILE flag" sentence marked superseded; current: OccupiedHeating/CoolingSetpoint + SystemMode NONVOLATILE; bridge pushes only changes (esp_matter also skips unchanged writes); deferred persistence on the 3 attributes, global `CONFIG_ESP_MATTER_DEFERRED_ATTR_PERSISTENCE_TIME_MS=60000` (rate limit, not quiet-period); boot behaviour (`first=true`, under 1 s, inferred); wear estimate 4,320 writes/day ~38 years (3 s: ~1.9 years), assumption half of 48 KB NVS free (unverified, `nvs_get_stats`).
- Conflict recorded, not resolved: task-015 finding listed ControlSequenceOfOperation as NONVOLATILE too; architect note names only 3 attributes. Open item added in section 8.
- Section 6 row 2a-10: 3 attributes, 60 s Kconfig, user approval of 60 s 2026-11-25. Section 7: 015 committed `9dbace6` (specs `bb2b004`), still not reviewed (review after 016); 016 being implemented by `firmware-dev`.
- index.md unchanged (summaries still valid). Not changed: `raw/`, `firmware/`, `tasks/`. Not committed.

## [2026-11-25] sync | phase-2a result: task 016 reviewed + verified; CSO NONVOLATILE resolved; libbsd-dev installed
No raw files added or changed. Session facts (orchestrator + independent agents, local inspection; esp_matter 1.6.1 source lines external, not in raw) and user decision (session 2026-11-25).
- concepts/firmware-architecture section 7: rows 015/016 (feature-end review 015+016: 0 blocker, 2 major, 8 minor, fixed, delta-reviewed, re-verified; 016 not committed); new "Task 016 verified facts" (host 62/0/0, image 0x1b3260 = 42.5 %, check_no_bus, phase-1 regression 372 lint + 139/0/0, review fixes, accepted deviations, notes for 2b); accepted 015 deviations.
- Section 3 "NVS wear": conflict resolved: ControlSequenceOfOperation is NONVOLATILE (`esp_matter_attribute.cpp:2131-2132`, `thermostat.cpp:368`), never updated (stays 4), writes rejected, no wear, no spec change.
- Section 8: on-device item now 015 D1–D7 + 016 O1–O9 + NVS free space; new provisional items (a) standby exit with stale 233, (b) 247 bit 5 with stale 245/246; STAT both/neither bits provisional "unusable"; CSO and libbsd-dev items closed.
- concepts/dev-environment: `libbsd-dev` installed, `~/esp-host-stubs` CPATH workaround no longer needed; open question 3 closed. index.md: firmware-architecture summary.
- Not changed: `raw/`, `firmware/`, `tasks/`. Not committed.
