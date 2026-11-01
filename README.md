# DaikinFancoilMatterThermostat

A Matter thermostat for Daikin fancoils. It controls EKRTCTRL1 / EKWHCTRL1 wall controls (INN-FR-B32 board) over their Modbus RTU serial interface (RS485, 9600 8N1).

**Status:** early stage. The Modbus documentation is ingested into the wiki; no firmware or code yet.

## Repository layout

| Path | Purpose |
|---|---|
| `Knowledgebase/raw/` | Unedited source documents (manuals, datasheets). Never modified. |
| `Knowledgebase/wiki/` | Markdown knowledge base maintained by the AI agent: summaries, concepts, entities, cross-references. Start at [`index.md`](Knowledgebase/wiki/index.md). |
| `.claude/skills/` | `/capture`, `/sync` and `/lint` commands that maintain the wiki. |
| `.claude/agents/` | Task-specific agents (none defined yet). |
| `RESUME.md` | Short hand-over note and start prompt so any session can continue the work. |
| `CLAUDE.md` | Rules for agents working in this repo. |

## Working with the LLM wiki

- `/capture` – ingest new documents added to `raw/`
- `/sync` – reconcile wiki and raw sources
- `/lint` – check the wiki for contradictions, orphans and dangling sources

To continue development in a fresh session, paste the start prompt from [`RESUME.md`](RESUME.md).
