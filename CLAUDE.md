# CLAUDE.md

Behavioral guidelines to reduce common LLM coding mistakes. Merge with project-specific instructions as needed.

**Tradeoff:** These guidelines bias toward caution over speed. For trivial tasks, use judgment.

## 1. Think Before Coding

**Don't assume. Don't hide confusion. Surface tradeoffs.**

Before implementing:
- State your assumptions explicitly. If uncertain, ask.
- If multiple interpretations exist, present them - don't pick silently.
- If a simpler approach exists, say so. Push back when warranted.
- If something is unclear, stop. Name what's confusing. Ask.

## 2. Simplicity First

**Minimum code that solves the problem. Nothing speculative.**

- No features beyond what was asked.
- No abstractions for single-use code.
- No "flexibility" or "configurability" that wasn't requested.
- No error handling for impossible scenarios.
- If you write 200 lines and it could be 50, rewrite it.

Ask yourself: "Would a senior engineer say this is overcomplicated?" If yes, simplify.

## 3. Surgical Changes

**Touch only what you must. Clean up only your own mess.**

When editing existing code:
- Don't "improve" adjacent code, comments, or formatting.
- Don't refactor things that aren't broken.
- Match existing style, even if you'd do it differently.
- If you notice unrelated dead code, mention it - don't delete it.

When your changes create orphans:
- Remove imports/variables/functions that YOUR changes made unused.
- Don't remove pre-existing dead code unless asked.

The test: Every changed line should trace directly to the user's request.

## 4. Goal-Driven Execution

**Define success criteria. Loop until verified.**

Transform tasks into verifiable goals:
- "Add validation" → "Write tests for invalid inputs, then make them pass"
- "Fix the bug" → "Write a test that reproduces it, then make it pass"
- "Refactor X" → "Ensure tests pass before and after"

For multi-step tasks, state a brief plan:
```
1. [Step] → verify: [check]
2. [Step] → verify: [check]
3. [Step] → verify: [check]
```

Strong success criteria let you loop independently. Weak criteria ("make it work") require constant clarification.

---

**These guidelines are working if:** fewer unnecessary changes in diffs, fewer rewrites due to overcomplication, and clarifying questions come before implementation rather than after mistakes.

---

# LLM Wiki System

Project knowledge lives in `Knowledgebase/`:

- `Knowledgebase/raw/` — unedited source material (PDFs, datasheets, manuals). **Read-only for agents. Never modify or delete.**
- `Knowledgebase/wiki/` — markdown knowledge base. **Exclusively maintained by the agent.** The user does not edit it by hand.
  - `index.md` — catalog of every wiki page with a one-line summary. Update on every change.
  - `log.md` — append-only chronological record of capture/sync/lint actions.
  - `sources/` — one summary page per raw document (`sources/<slug>.md`).
  - `concepts/` — topic pages (protocols, registers, modes, wiring...).
  - `entities/` — pages for concrete things (devices, components, products).

## Wiki rules

- Every claim must trace to a raw source: cite as `[[sources/<slug>]]` plus page/section where possible. No unsourced facts; mark inferences as `(inferred)`.
- Cross-reference with relative markdown links. Every page must be linked from `index.md` and from at least one other page.
- Page front matter: `title`, `type` (source|concept|entity), `sources` (list of raw filenames), `updated` (date).
- Conflicts between sources are recorded on the page, never silently resolved.

## Commands

- `/capture` — ingest new/changed files in `raw/`: summarize, create/update concept and entity pages, update index and log.
- `/sync` — reconcile wiki with raw: detect raw files not yet ingested or changed since ingestion, update affected pages.
- `/lint` — audit wiki: contradictions, orphan pages, broken links, claims whose source no longer exists. Report, then fix on approval.

Details are in `.claude/skills/{capture,sync,lint}/SKILL.md`.

## RESUME.md rules

1. `RESUME.md` (repo root) lets any agent continue from a blank session.
2. **Update it after EVERY step** executed in a session, before moving on.
3. Keep it short: last step done, current status, next steps, and the copy-paste start prompt. No full history (that's git's job).
4. It must always contain a ready-to-paste prompt that makes a fresh agent pick up the work.

## Agents

Project agents go in `.claude/agents/` (one `.md` per agent). See `.claude/agents/README.md`.

## Git

- Commit with meaningful messages: imperative subject ≤72 chars saying *what and why*; body when non-obvious. One logical change per commit.
- `raw/` is committed (PDFs are source of truth). Don't commit secrets (`.claude/settings.local.json` is ignored).
