# docs_ai/ — AI-Agent Documentation for OpenPRU

This folder contains documentation for agentic AI tools — tools that can read
files and execute multi-step plans. It is not a tutorial and not intended for
reading by humans following along manually.

## Who this is for

Agentic AI tools performing OpenPRU development tasks such as:
- Porting an existing project to a new processor or board
- Adding new features to an existing project
- Creating a new OpenPRU project from scratch

## How to use this folder

Configure your tool to read `docs_ai/README.md` at the start of any OpenPRU
task. This file directs you to the correct task runbook.

**Claude Code users**: the repo-level `CLAUDE.md` includes a pointer to this
folder. No additional configuration is needed.

**Other tools**: add a rule that reads `docs_ai/README.md` before any OpenPRU
task. No per-tool config files are maintained in this repo.

**Personal Claude settings**: add them to `~/.claude/CLAUDE.md` on your own
machine, not to this repo's `CLAUDE.md`. The repo `CLAUDE.md` is for
project-level instructions only.

## Task runbook index

| Task | File |
|------|------|
| Port an existing project to a new device or board | `docs_ai/task_port_project.md` |
| Add new features to an existing project | `docs_ai/task_add_features.md` |
| Create a new OpenPRU project from scratch | `docs_ai/task_create_project.md` |

## Reference files (read on demand)

Read these files when directed to by a task runbook — do not read all of them
upfront.

| File | Contents |
|------|----------|
| `docs/open_pru_organization.md` | Repo layout and project structure |
| `docs/open_pru_create_new_project.md` | Project creation patterns |
| `docs/open_pru_create_new_mcuplus_project.md` | MCU+ code addition patterns |
| `best_practices.md` | Coding standards for PRU assembly and C |
| `docs/PRU Assembly Instruction Cheat Sheet.md` | PRU instruction reference |

**Deep references** (read only when a compiler or assembler question arises):

- PRU assembly language tools user guide:
  Can be found online at `https://www.ti.com/lit/spruhv6`
- PRU optimizing C compiler user guide:
  Can be found online at `https://www.ti.com/lit/spruhv7`

## Self-contained guarantee

All task runbooks reference only files within the repo and standard tool
install paths. They do not depend on any individual developer's local
configuration (for example, `~/.claude/CLAUDE.md`).

## Maintenance

When a PR changes the patterns documented in these runbooks — project
structure, build conventions, file naming, or adding a new processor/device —
the PR author must check whether an update is needed. See
`docs/contributing.md` for the full checklist.
