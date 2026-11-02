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

Development, architecture and review agents are still to be refined (brainstorm after these).
