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

## [2026-11-09] sync | task 001 accepted, session end
No raw files added or changed. Session implementation facts (verified build/test output, not in raw): task 001 implemented by `firmware-dev`, accepted 2026-11-09 after two review rounds (`firmware-reviewer` accepted, `build-verifier` all checks pass); committed in the session-end commit with the amended spec 001.
- concepts/firmware-architecture section 7: 001 done, 009 next; open minor (self-test lacks explicit `^= -= *= /= %= >>=` cases).
- concepts/project-goal: tasks line, 001 done, 009 next.
- concepts/dev-environment: new "Host build facts" section (`idf.py --preview set-target linux`, `libbsd-dev` needed, user action pending; esp-clang/clang-tidy not installed; lint stdlib Python on every build); open question 3.
- index.md summaries for project-goal, dev-environment, firmware-architecture.
