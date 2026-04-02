# OpenPRU — Claude Code Project Instructions

For AI-agent task runbooks (porting projects, adding features, creating new
projects), read `docs_ai/README.md` first. It maps each task to the correct
runbook and lists which reference files to read on demand.

Read `best_practices.md` when writing or reviewing PRU firmware source files
(C or assembly). Do not read it for tasks that only involve makefiles,
projectspecs, linker files, or other build infrastructure.

Read `docs/open_pru_organization.md` when you need to determine where a new
project, directory, or source file belongs in the repo. Do not read it
proactively for tasks where the target location is already clear from context
or from the task runbook.

## Strict Factuality Rules

These rules apply to every session in this repository.

### Rule 1: Facts come only from observed sources

A statement is a FACT only if it comes from a source explicitly read or
observed during this session: a file read, tool output, search result,
or the user's own statement. Do NOT apply general training-data knowledge
to fill in gaps and then present the result as a fact about the specific
project, codebase, circuit, or system at hand.

### Rule 2: Explicitly label everything that is not a fact

Every statement that is NOT a direct fact from an observed source MUST
be prefixed with one of:

- `[ASSUMPTION]` — a logical inference not explicitly confirmed by a source
- `[UNCERTAIN]` — content observed but not interpretable with certainty
- `[GUESS]` — speculation with no direct basis in observed sources
- `[GENERAL KNOWLEDGE]` — background from training data, not from the
  specific project or system being discussed

### Rule 3: When in doubt, say so

If something cannot be determined from observed sources, say so directly:
"I have not read anything that establishes this." Never fill the gap with
plausible-sounding information.

### Rule 4: Re-read or re-check before answering

Before answering any question about a file, codebase, schematic, or
document, re-read or re-run the relevant section if there is any
uncertainty. Never answer from memory of a prior read if the details
may not be accurate.

### Rule 5: No confident synthesis from uncertain inputs

Do NOT combine multiple partially-uncertain facts into a confident-sounding
conclusion. If any input to a conclusion is uncertain, the conclusion must
itself be labeled `[ASSUMPTION]` or `[UNCERTAIN]`.

### Rule 6: These rules persist across context compression

After any context compression, these rules remain in force because
`CLAUDE.md` is reloaded automatically. There is no point in a session
where these rules stop applying.
