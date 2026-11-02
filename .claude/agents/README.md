# Agents

Project-specific subagents, one markdown file each (`<name>.md`) with front matter:

```
---
name: <name>
description: <when to use it>
tools: Read, Grep, ...
---
<system prompt>
```

Agents read knowledge from `Knowledgebase/wiki/`, never write to `raw/`, and must keep `RESUME.md` current. 
## Defined agents
| Agent | Role | Writes |
|---|---|---|
| `wiki-keeper` | Only writer of `Knowledgebase/wiki/`; capture/sync/lint, records decisions | wiki, RESUME.md |
| `datasheet-reader` | Visual, page-referenced fact extraction from `raw/` PDFs | nothing |

| `orchestrator` (opus) | Entry point: reads the request, decides which workflow steps/agents run (full pipeline or a single step), delegates, enforces gates. Run as main agent: `claude --agent orchestrator` | nothing itself |
| `architect` (opus) | Proposes layered architecture and Modbus<->Matter mapping, needs user approval, then writes task specs to `tasks/` | `tasks/` only |
| `firmware-dev` (sonnet; haiku for small tasks) | Implements one approved task spec; readable, testable code with unit tests | `firmware/` |
| `build-verifier` (haiku) | Independent build, lint, registry-only dependency check | nothing |
| `firmware-reviewer` (sonnet) | Independent review vs spec, architecture, wiki, bus safety | nothing |

The orchestrator is the project default agent (`.claude/settings.json`); otherwise start with `claude --agent orchestrator` (subagents cannot spawn agents, so it must be the main-session agent). Pipeline: architect (user approves) -> task spec -> firmware-dev -> build-verifier + firmware-reviewer -> user. Gate: no task spec before the architecture is approved and the bus-topology questions are answered. `hw-bus-reviewer` is undecided (not defined). Subagents cannot call each other; the main session relays.
