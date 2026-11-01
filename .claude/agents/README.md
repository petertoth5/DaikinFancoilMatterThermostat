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

Agents read knowledge from `Knowledgebase/wiki/`, never write to `raw/`, and must keep `RESUME.md` current. None defined yet.
