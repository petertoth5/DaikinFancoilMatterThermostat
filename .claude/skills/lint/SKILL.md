---
name: lint
description: Health-check the LLM wiki for discrepancies and orphans. Use when the user types /lint.
---

# /lint

Check, read-only first:

1. **Orphans** – pages not in `index.md` or not linked from any other page.
2. **Dangling sources** – pages citing raw files that no longer exist, or sources without a raw file.
3. **Broken links** – relative links/`[[...]]` that don't resolve.
4. **Contradictions** – conflicting values (registers, voltages, addresses, limits) across pages.
5. **Unsourced claims** – facts without citation and not marked `(inferred)`.
6. **Staleness** – raw file newer than the page's `updated`.
7. **Front matter** – missing/invalid fields.

Output a findings list grouped by severity with file paths. Ask before fixing; after approval fix, append `## [date] lint | ...` to `log.md`, update `RESUME.md`.
