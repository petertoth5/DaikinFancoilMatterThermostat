# RESUME

Update after EVERY step. Keep short.

## Project
Matter thermostat for a Daikin fancoil (EKWHCTRL/EKRTCTRL controllers, Modbus RTU). Knowledge in `Knowledgebase/wiki/` (agent-maintained), sources in `Knowledgebase/raw/`. Rules: root `CLAUDE.md`.

## Last step
Wiki ingest of both N420384A PDFs, README and workflow docs committed and pushed to `origin/main` (SSH). Session closed; waiting for the user's project brief.

## Status
- Wiki populated (2 sources, 5 concepts, 4 entities); open conflict: +AB- vs +BA- terminal label (`concepts/modbus-rs485-link.md`).
- Wiki derived from `pdftotext`; diagrams/bit positions not verified visually (`pdftoppm` is installed).
- No code, no agents, no project brief yet. Git clean and in sync with remote.

## Next
1. **User gives a descriptive prompt**: project goal, available hardware units, and the needed SW architecture (brief).
2. Agent saves it verbatim as `Knowledgebase/raw/project-brief.md` (raw is never edited afterwards), runs `/capture`, and records goal/hardware/architecture in the wiki (concepts + entities).
3. Agent asks clarifying questions on gaps/conflicts with the Modbus docs, then proposes an architecture and a step plan; update this file.
4. Later: define agents in `.claude/agents/`; optionally verify flag bit positions on PDF page 5.

## Start prompt (copy/paste into a fresh session)
```
Read RESUME.md and CLAUDE.md, then continue the DaikinFancoilMatterThermostat project from "Next". I will now give you the project goal, the available hardware and the brief SW architecture: save it to Knowledgebase/raw/project-brief.md, /capture it into the wiki, ask me about gaps, and update RESUME.md after every step. Use the wiki as the source of truth.
```
