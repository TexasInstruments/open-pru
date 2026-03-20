# Git Development Workflow Guide

This document defines the **standard Git workflow** for contributors and LLM agents working on this repository.

It describes:

* branch naming rules
* development workflow
* commit guidelines
* pull request rules
* merge strategy
* advanced Git topics

The goal is to maintain a **clean, traceable, and automation-friendly repository history**.

---

# 1. Core Principles

1. **Never commit directly to `main`.** (Enforce via branch protection rules — see Section 13.)
2. All development must occur in **dedicated branches**.
3. Every change must be integrated via a **Pull Request (PR)**.
4. PRs must include **clear documentation**.
5. Commit history must remain **clean and meaningful**.
6. Branch names must follow the **defined syntax**.

---

# 2. Branch Naming Convention

To ensure consistency and traceability, all development branches must follow a **strict naming syntax**.

## Branch Name Format

```
dev_<author>_<type>_<subject>
```

### Structure

| Field       | Description                                       | Example                                                       |
| ----------- | ------------------------------------------------- | ------------------------------------------------------------- |
| `dev`       | Indicates development branch                      | `dev`                                                         |
| `<author>`  | Developer name, ID, or `llm` for AI contributions | `thomas`, `tley`, `llm`                                       |
| `<type>`    | Type of change                                    | `feature`, `bugfix`, `docs`, `refactor`, `test`, `experiment` |
| `<subject>` | Short description of the change                   | `pru-selftest`, `overflow-fix`                                |

## Examples

### Human developer

```
dev_thomas_feature_pru-selftest
dev_tley_bugfix_integrator-overflow
```

### LLM-generated work

```
dev_llm_feature_pru-validation-suite
dev_llm_docs_pru-instruction-guide
```

### Experimental work

```
dev_thomas_experiment_pru-timing-check
```

## Naming Rules

1. Use **lowercase only**
2. Separate fields with **underscore `_`**
3. Use **hyphen `-` within the subject**
4. Keep subject **short and meaningful**
5. Avoid spaces
6. Recommended maximum length **64 characters**

### Good Examples

```
dev_llm_feature_pru-test-suite
dev_thomas_bugfix_qbbs-sign-check
dev_tley_docs_pru-cheatsheet-update
```

### Bad Examples

```
feature_pru
thomas_dev_bugfix
dev_feature_fix
dev_thomas_fix
```

---

# 3. Branch Structure

The repository uses a **trunk-based workflow**.

```
main
 └── dev_<author>_<type>_<subject>
```

All work branches originate from **main**.

---

# 4. Development Workflow

## Step 1 — Update Local Repository

```
git checkout main
git pull origin main
```

## Step 2 — Create Development Branch

Example:

```
git checkout -b dev_thomas_feature_pru-selftest
```

## Step 3 — Implement Changes

Edit files and develop the feature or fix.

Commit changes in **logical steps**.

## Step 4 — Commit Changes

Review staged changes before committing:

```
git status
git add <specific files>
git commit -m "feat: add PRU instruction self-test framework"
```

> **Note:** Prefer `git add <specific files>` over `git add .` to avoid accidentally staging untracked or generated files. Use `git status` to review what will be staged.

## Step 5 — Push Branch

```
git push origin dev_thomas_feature_pru-selftest
```

## Step 6 — Open Pull Request

Create a Pull Request on GitHub:

```
dev_thomas_feature_pru-selftest -> main
```

## Step 7 — Review and Merge

After approval:

* PR is merged
* branch is deleted

---

# 5. Commit Rules

## Commit Message Format

```
<type>: <short description>

(optional longer description)
```

## Allowed Types

| Type     | Purpose            |
| -------- | ------------------ |
| feat     | new feature        |
| fix      | bug fix            |
| refactor | code restructuring |
| docs     | documentation      |
| test     | tests              |
| chore    | maintenance        |

## Examples

```
feat: add PRU instruction validation suite

fix: correct overflow detection in integrator
docs: update PRU instruction cheat sheet reference
```

---

# 6. Pull Request Rules

Each Pull Request must contain a **clear description**.

## PR Title

Short summary of the change.

Example:

```
Add PRU instruction self-test library
```

## Pull Request Description Template

```
## Purpose
Explain why this change is needed.

## Changes
- Added instruction self-test framework
- Implemented tests for ADD, SUB, QBBS

## Testing
Describe how the change was verified.

## Impact
Explain potential side effects.

## References
Links to issues or documentation.
```

---

# 7. Advanced Git Topics

## 7.1 Git Stash

`git stash` temporarily saves uncommitted changes.

Useful when switching branches quickly.

### Save changes

```
git stash
```

### List stashes

```
git stash list
```

### Restore stash

```
git stash pop
```

### Example Workflow

```bash
# Working on a feature, but an urgent bug fix is needed:
git stash                        # save current uncommitted work
git checkout main                # switch to main
git pull origin main
git checkout -b dev_tley_bugfix_urgent-fix
# ... fix the bug, commit, push, open PR ...
git checkout dev_tley_feature_original-work
git stash pop                    # restore saved work
```

## 7.2 Combine (Squash) Commits

Before merging a branch, combine small commits into **one clean commit**.

### Interactive Rebase

```
git rebase -i HEAD~5
```

Example:

```
pick 1234 initial code
squash 5678 fix typo
squash 9abc formatting
```

Final result:

```
feat: implement PRU instruction test library
```

Benefits:

* cleaner history
* easier review
* simpler debugging

> **Warning:** After squashing, you must force-push the branch:
> ```bash
> git push --force-with-lease origin <branch-name>
> ```
> Only do this on **development branches**, never on `main`. Coordinate with collaborators if the branch is shared.

## 7.3 Amend Last Commit

Modify the last commit.

```
git commit --amend
```

Use for:

* fixing commit message
* adding forgotten files

> **Note:** If the commit has already been pushed, you will need to force-push afterwards (`git push --force-with-lease`). Only do this on development branches.

---

# 8. Conflict Resolution

When `main` has advanced ahead of your development branch, you may encounter merge conflicts.

## Keeping Your Branch Up to Date

```bash
git checkout dev_tley_feature_my-work
git fetch origin
git rebase origin/main
```

Alternatively, use the **"Update branch"** button on the GitHub PR page.

## Resolving Conflicts

1. Git will pause the rebase and indicate conflicting files.
2. Open each conflicting file and resolve the conflicts manually.
3. Stage the resolved files:

   ```bash
   git add <resolved-file>
   ```

4. Continue the rebase:

   ```bash
   git rebase --continue
   ```

5. Force-push the updated branch:

   ```bash
   git push --force-with-lease origin <branch-name>
   ```

> **For LLM agents:** If conflicts arise, clearly document which conflicts were resolved and how in the PR description. If unsure, request human review.

---

# 9. PR Review Guidelines

Reviewers should check the following.

## Code Quality

* readability
* maintainability
* consistent style

## Functionality

* correct behaviour
* edge cases handled
* no regressions

## Documentation

* comments updated
* README updated if required
* PR description clear

---

# 10. Merge Strategy

This repository uses **Squash and Merge**.

Benefits:

* clean main branch
* one commit per feature
* easier debugging

Example result on main:

```
feat: add PRU instruction validation suite
```

---

# 11. LLM Contribution Rules

When an **LLM agent contributes**:

1. Use `llm` as the `<author>` field in the branch name (for example, `dev_llm_feature_...`).
   If a specific agent identifier is preferred (for example, `copilot`), it may be used instead,
   but must be lowercase and consistent.
2. Keep commits **small and logical**.
3. Clearly document reasoning in the **PR description**.
4. Modify **only relevant files**.
5. Include **tests whenever possible**.
6. If resolving merge conflicts, **document all conflict resolutions** in the PR description.

Example branch:

```
dev_llm_feature_pru-validation-suite
```

---

# 12. Repository Hygiene

Regular maintenance tasks:

```
git fetch --prune
git branch -d old-feature
git gc
```

---

# 13. Branch Protection

The `main` branch should have **branch protection rules** enabled:

* **Require pull request reviews** before merging.
* **Require status checks** to pass before merging (if CI is configured).
* **Do not allow direct pushes** to `main`.
* **Require branches to be up to date** before merging.

These settings enforce the core principles defined in Section 1.

---

# 14. Example Full Workflow

```bash
git checkout main
git pull

git checkout -b dev_thomas_feature_pru-selftest

# edit code

git status
git add src/pru_selftest.c
git commit -m "feat: add PRU instruction selftest"

git push origin dev_thomas_feature_pru-selftest
```

Open PR → Review → Squash Merge.

---

# 15. Summary

Key rules:

* use **standardized branch names**
* develop only in **dev branches**
* use **clear commit messages**
* integrate via **Pull Requests**
* keep history **clean with squash merge**
* **resolve conflicts** by rebasing onto main
* document changes properly

This workflow enables **efficient collaboration between human developers and LLM agents** while keeping the repository maintainable and traceable.
