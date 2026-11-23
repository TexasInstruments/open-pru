# docs_ai/ — Authoring Guide

Read this before adding to or changing `docs_ai/` (a new tasklist, a new
reference doc, a feature or simulator-testing runbook, or an edit to
`README.md`). It is not read during day-to-day tasks — `README.md` does not
route task agents here.

## Why docs_ai is structured the way it is

`AGENTS.md` makes every OpenPRU task begin by reading `docs_ai/README.md`. That
file is therefore always-loaded context for every task, competing with the
actual work for the agent's attention and token budget. Everything below follows
from that: keep the always-loaded surface tiny, and push detail into files that
are loaded only on demand.

## Two kinds of file, kept separate

- **Execution docs** — task runbooks (`task_*.md`) an agent *follows* to do work,
  and reference docs under `reference/` an agent *consults*. These are what
  `README.md` routes to.
- **Meta docs** — this authoring guide. Read only when editing docs_ai, by a
  different actor at a different time. Never routed from the task or reference
  tables.

Do not mix the two. Guidance about *how to write* a runbook does not belong in a
runbook, and never belongs in `README.md`.

## Authoring practices

1. **Progressive disclosure.** `README.md` is a router, not a manual. Detail
   lives in leaf files loaded on demand. Never inline into `README.md` what a
   table row can point to.

2. **A when-to-read trigger on every reference.** Each row in the reference
   tables must say when to read the file and, where useful, when to skip it.
   Without an explicit trigger an agent over-reads. For large files, add a "grep
   for the relevant section first" instruction.

3. **Self-contained.** Runbooks and references must point only to files inside
   the repo and standard tool install paths. They must not depend on any
   individual developer's local configuration (for example, `~/.claude/CLAUDE.md`
   or a machine-specific path). Anything shared with other engineers must assume
   no ambient local state.

4. **Keep README.md a pure router.** Concretely, `README.md` contains only:
   its short intro, the task runbook index table, the reference-files table, the
   deep-references table, and the one-line pointer to this guide. It must NOT
   contain: prose explaining a task (that goes in the runbook), authoring or
   maintenance rules (they go here), background or rationale, or any content an
   agent would read but not act on. Before committing a `README.md` change, ask:
   "does a task-executing agent act on this line?" If no, it does not belong in
   `README.md`. New rows are fine; new sections are a signal you are adding
   content that belongs in a leaf file or in this guide.

5. **Maintenance is part of the same PR.** When a PR changes an OpenPRU repo
   pattern that docs_ai documents, the PR author updates the affected docs_ai
   files in that same PR. The authoritative list of triggering changes lives in
   `docs/contributing.md` §"Update AI-agent documentation" — do not restate it
   here; keep that list the single source of truth.

## Writing a task runbook for an AI agent

A task runbook is a procedure an agent *executes* end to end, often unattended
and often on a non-Claude harness. Write for an executor that is capable but
has no memory of this repo beyond the runbook and its pre-reads, follows
instructions literally, and will fill any gap with a plausible guess unless
told not to. The failure mode is not missing instructions — it is steps buried
in prose getting skipped, and steps marked done that were never performed.

### Required skeleton

Use this top-level structure, in order:

1. **Title** — `# Task: <imperative goal>`.
2. **How to run this task** — the execution protocol the agent follows before
   touching a step. Use the template in "Make the checklist the spine" below.
   This is what turns a document into a tracked procedure.
3. **Pre-read list** — the exact files to read first, each with a one-line
   reason. Repo knowledge enters here; every step below assumes it.
4. **Decision tree** — the branching questions that set scope (target
   device/board, host type, PR vs private), answered *before* any step runs.
   End it with an instruction to record the answers to a visible note; later
   sections branch on them.
5. **Sections of checkboxed steps** — the work (see step rules below).
6. **Verification steps** — numbered `V1, V2, …`, each with the command to run,
   the expected result, and an instruction to paste the actual output. Have them
   reproduce locally whatever gate the change must ultimately pass (for this
   repo, the top-level and CCS builds that CI runs), so failures surface before
   handoff — the agent runs these steps, not CI. End with a step that checks each
   recorded Do-not-do constraint held.
7. **Do-not-do list** — the mistakes an agent would otherwise make, each with a
   one-line reason so the constraint survives paraphrase.

### Make the checklist the spine

The "How to run this task" section converts a static document into a tracked
procedure. Use this template verbatim, adapting only the reference to the
runbook's decision steps:

````markdown
## How to run this task

Before starting:

1. Read the Pre-read list below.
2. Answer the Decision tree questions and record the answers — plus the
   Do-not-do constraints — in a "Run parameters" note you keep visible. Every
   section below branches on the answers; the final verification checks the
   constraints held.
3. Turn each `- [ ]` item in the sections your answers select into tracked
   work:
   - If your environment provides a task or to-do tracking tool, create one
     tracked item per checkbox and mark each done as you complete it.
   - Otherwise, copy the relevant checklist into your first response and
     re-post it with items marked `[x]` as you finish them.
   Either way, your first response must show the materialized list — the tool's
   items or the pasted checklist — so the tracking is visible before any step
   runs.
4. A checkbox with sub-bullets is done only when every sub-bullet is done.
5. Do the steps in order; later sections depend on earlier outputs. Do not skip
   an item because it "looks unnecessary." If an item is N/A given your
   answers, mark it N/A with a one-line reason.
````

The rules this template encodes:

- **Atomic imperative items.** Every step is a `- [ ]` box with one action.
  Prose paragraphs bury actions — do not hide a required step inside one.
- **Portable task-tracking — never name a tool.** The template describes the
  capability ("if your environment provides a task/to-do tool …"). Naming a
  specific harness's task tool breaks the runbook on other harnesses.
- **Verification proves success, not asserts it.** Every verification step
  states the command, the expected result, and an instruction to paste the
  actual output. A step the agent can satisfy by saying "done" — "confirm it
  works," "ensure the build passes" — verifies nothing.

### How to write the steps

- **Point to the authoritative doc; do not restate it.** Write
  `See \`docs/…\` §"Section name"` and summarize only the keys that matter.
  Duplicated procedure drifts out of sync — the single-source rule the rest of
  this guide follows.
- **Make every branch explicit.** State the condition and both outcomes
  ("For Linux-only or standalone hosts, skip this step; otherwise …"). Never
  leave the executor to infer whether a step applies; tell it to mark an
  inapplicable step N/A with a one-line reason rather than silently skip it.
- **Inline the exact command** where one exists (`cp -r <src> <dst>`), with
  placeholders in angle brackets.
- **Gate irreversible, hardware, or ambiguous actions on the user.** For pin
  routing, SysConfig, device settings, and anything unverifiable from repo
  files: copy a known-good starting point, tell the user it is unmodified and
  needs manual review, and do not let the agent edit it — even with
  user-provided values.
- **Forbid guessing explicitly.** Where a value cannot be derived from an
  observed source, instruct the agent to mark a FIXME and ask the user rather
  than invent one. Agents guess by default; the runbook is where you stop it.
- **Insert a first-artifact checkpoint.** Where the work has a natural first
  coherent artifact — a renamed skeleton before customization, the first core
  that builds — add a step that presents it and re-checks it against the
  recorded scope before the pattern is replicated or customized further. An
  interpretation error caught here is caught once, not repeated across every
  later step.
- **Bound blind retries.** Tell the agent that if a verification (build or test)
  fails, it may make a small number of fix attempts, then must stop and report —
  never invent a workaround or edit outside the project to force a pass.
  Unbounded edit→build loops are a silent failure mode.
- **Use stable named anchors** for cross-references (section names, step IDs),
  never numeric footnotes. Step IDs must be unique within a runbook; the same
  IDs (`A1`, `D1`, `V1`) recurring across runbooks is fine — each is executed in
  isolation.

### Adherence is not a guarantee

In-document wording maximizes adherence but does not enforce it. If a task must
not deviate, enforcement comes from a wrapper outside the document (a harness
hook or an external runner that checks the ticked checklist), not from stronger
prose. Write the runbook for maximum adherence; do not assume it is airtight.

### Self-check before committing a runbook

- Could an agent with no prior knowledge of this repo follow it top to bottom,
  stopping at every point a human decision is required?
- Does every step either contain its command or name the doc and section that
  does?
- Does every verification step state a command, an expected result, and an
  instruction to paste the actual output?
- Do the verification steps reproduce the gate the change must ultimately pass,
  and end by checking each Do-not-do constraint held?

## Adding a new task runbook

1. Write `docs_ai/task_<name>.md` following the skeleton and rules in
   "Writing a task runbook for an AI agent" above.
2. Add one row to the task runbook index table in `README.md`.
3. Do not add prose about the task to `README.md` — it goes in the runbook.

## Adding a new reference doc

1. Place it under `docs_ai/reference/<doc_name>/`, one folder per document, so
   any figures or assets stay beside the file that links them.
2. Add one row to the reference-files table (or the deep-references table for
   large tool manuals) in `README.md`, with a when-to-read trigger.
