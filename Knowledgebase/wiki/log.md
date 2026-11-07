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
