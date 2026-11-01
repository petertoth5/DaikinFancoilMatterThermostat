# RESUME

Update after EVERY step. Keep short.

## Project
Matter thermostat for a Daikin fancoil (EKWHCTRL/EKRTCTRL controllers, Modbus RTU). Knowledge in `Knowledgebase/wiki/` (agent-maintained), sources in `Knowledgebase/raw/`. Rules: root `CLAUDE.md`.

## Last step
`/capture` of both N420384A PDFs done: 2 source pages, 5 concept pages, 4 entity pages in `Knowledgebase/wiki/` (text via pdftotext; diagrams not visually checked).

## Status
- Wiki populated; open source conflicts recorded in `concepts/modbus-rs485-link.md` (+AB- vs +BA-).
- No code yet. No agents defined yet.
- Git: repo initialized, all staged, **not committed** (git user.name/email not configured on this machine; ask user).

## Next
1. Configure git identity, commit (suggested: "Add LLM wiki system and ingest N420384A Modbus manuals"), add remote, push.
2. Optionally verify STAT/ALR_STAT bit positions against rendered PDF page 5.
3. Define agents (user to specify), then decide hardware/firmware architecture from the wiki.

## Start prompt (copy/paste into a fresh session)
```
Read RESUME.md and CLAUDE.md, then continue the DaikinFancoilMatterThermostat project from "Next". Use the wiki in Knowledgebase/wiki as the source of truth, update RESUME.md after every step, and tell me what you're about to do first.
```
