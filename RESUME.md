# RESUME

Update after EVERY step. Keep short.

## Project
Matter thermostat for a Daikin fancoil (EKWHCTRL/EKRTCTRL controllers, Modbus RTU). Knowledge in `Knowledgebase/wiki/` (agent-maintained), sources in `Knowledgebase/raw/`. Rules: root `CLAUDE.md`.

## Last step
Wiki ingest of both N420384A PDFs committed; repo pushed to `origin/main` (git@github.com:petertoth5/DaikinFancoilMatterThermostat.git, SSH). README rewritten.

## Status
- Wiki populated (2 sources, 5 concepts, 4 entities); open conflict: +AB- vs +BA- terminal label (`concepts/modbus-rs485-link.md`).
- Wiki derived from `pdftotext`; diagrams/bit positions not visually verified.
- No code yet. No agents defined yet.
- Git clean and in sync with remote.

## Next
1. Optionally verify STAT/ALR_STAT bit positions against rendered PDF page 5 (`pdftoppm` is installed).
2. Define agents (user to specify).
3. Decide hardware/firmware architecture from the wiki (Matter bridge to Modbus RTU).

## Start prompt (copy/paste into a fresh session)
```
Read RESUME.md and CLAUDE.md, then continue the DaikinFancoilMatterThermostat project from "Next". Use the wiki in Knowledgebase/wiki as the source of truth, update RESUME.md after every step, and tell me what you're about to do first.
```
