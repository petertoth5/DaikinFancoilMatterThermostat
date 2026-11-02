# DaikinFancoilMatterThermostat

A Matter thermostat for Daikin fancoils. It controls EKRTCTRL1 / EKWHCTRL1 wall controls (INN-FR-B32 board) over their Modbus RTU serial interface (RS485, 9600 8N1).

**Status:** early stage. Project brief and Modbus documentation are in the wiki; no firmware yet. Target: ESP32-S3 (ESP-IDF, components only from the ESP component registry via the IDF component manager). Dev environment ready; agents defined (orchestrator is the default agent, see `.claude/agents/README.md`); next: answer bus-topology questions, then architecture.

## Repository layout

| Path | Purpose |
|---|---|
| `Knowledgebase/raw/` | Unedited source documents (manuals, datasheets). Never modified. |
| `Knowledgebase/wiki/` | Markdown knowledge base maintained by the AI agent: summaries, concepts, entities, cross-references. Start at [`index.md`](Knowledgebase/wiki/index.md). |
| `firmware/` | ESP-IDF v6.0.2 project skeleton (esp32s3); components only from the ESP registry. |
| `.claude/skills/` | `/capture`, `/sync` and `/lint` commands that maintain the wiki. |
| `.claude/agents/` | Task-specific agents (none defined yet). |
| `RESUME.md` | Short hand-over note and start prompt so any session can continue the work. |
| `CLAUDE.md` | Rules for agents working in this repo. |

## Working with the LLM wiki

- `/capture` – ingest new documents added to `raw/`
- `/sync` – reconcile wiki and raw sources
- `/lint` – check the wiki for contradictions, orphans and dangling sources

To continue development in a fresh session, paste the start prompt from [`RESUME.md`](RESUME.md).
