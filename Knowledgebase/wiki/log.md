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
