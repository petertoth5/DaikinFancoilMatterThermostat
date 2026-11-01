---
name: sync
description: Reconcile the LLM wiki with Knowledgebase/raw. Use when the user types /sync.
---

# /sync

1. Compare `raw/` against wiki `sources/` pages and `log.md`: find raw files with no source page, source pages whose raw file changed (mtime newer than `updated`) or vanished.
2. For new files run the `/capture` procedure. For changed files re-read and update the source page plus every concept/entity page citing it. For vanished files flag the dependent pages (do not delete without approval).
3. Verify `index.md` lists every page and cross-references resolve.
4. Append `## [date] sync | ...` to `log.md`, update `RESUME.md`, report a summary and a suggested commit message.
