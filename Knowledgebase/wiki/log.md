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
