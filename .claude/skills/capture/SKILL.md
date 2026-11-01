---
name: capture
description: Ingest new documents from Knowledgebase/raw into the LLM wiki. Use when the user types /capture or says new sources were added to raw.
---

# /capture

1. Read `Knowledgebase/wiki/log.md` and `index.md` to learn which raw files are already ingested. List `Knowledgebase/raw/`; the new files (or those named by the user) are the targets.
2. For each target file, read it fully (PDFs: use `pages` in chunks of ≤20).
3. Create `wiki/sources/<slug>.md`: front matter, summary, key facts (specs, registers, wiring, limits), page references.
4. Create or update `wiki/concepts/*` and `wiki/entities/*` pages touched by the source. Add cross-links both ways. Record conflicts with existing content explicitly.
5. Update `wiki/index.md` and append to `wiki/log.md` (`## [YYYY-MM-DD] capture | <file>` + pages touched).
6. Never edit `raw/`.
7. Update `RESUME.md`. Report what was added; suggest a commit message.
