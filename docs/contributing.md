<div align="center">

<img src="https://upload.wikimedia.org/wikipedia/commons/b/ba/TexasInstruments-Logo.svg" width="150"><br/>

# Contributing to the OpenPRU project

</div>

[Introduction](#introduction)  
[Common requirements (all PRs)](#common-requirements-all-prs)  
[Adding a bugfix](#adding-a-bugfix)  
[Adding a feature to an existing project](#adding-a-feature-to-an-existing-project)  
[Porting a project to a new processor or board](#porting-a-project-to-a-new-processor-or-board)  
[Adding a new project](#adding-a-new-project)  
[Reference: project documentation](#reference-project-documentation):
[Project-level readme](#project-level-readme),
[Update the section-level readme](#update-the-section-level-readme)
[Reference: build verification](#reference-build-verification):
[Testing the build](#testing-the-build)

## Introduction

Ready to contribute back to the OpenPRU project? This page provides a checklist
to simplify the contribution process.

## Common requirements (all PRs)

### Coding guidelines

When adding new code, please make sure that the code follows the
[best practices](../best_practices.md).

### Rebase on main

The main branch might have been updated during your development. Rebase on top
of the main branch before creating the pull request, and resolve any conflicts.
Like this:

```
$ git pull --rebase origin main
```

### Update AI-agent documentation

If your PR changes any of the following, check whether `docs_ai/` needs
updating before submitting:

* Project directory structure or file naming conventions
* Makefile patterns (project `makefile`, core makefile, parent `makefile`)
* CCS build infrastructure (`example.projectspec` patterns)
* Adding support for a new device or board
* Adding or removing a shared library in `source/`

If the existing runbooks in `docs_ai/` still accurately describe the patterns
after your change, no update is needed. If they no longer match, update the
relevant runbook(s) as part of the same PR.

### Update pr_compliance_checklist.yaml

If your PR adds or changes a contribution requirement in this file, also
update `pr_compliance_checklist.yaml` so that Qodo can enforce the
requirement in code review.

## Adding a bugfix

- If the fix changes a shared library (`source/`): grep for all projects
  that use the modified file and verify none are broken.
- Build verification: project-level build + top-level build
  (see [§"Testing the build"](#testing-the-build)).
- No README update needed unless documented behavior changes.

## Adding a feature to an existing project

- Documentation: if the feature changes user-visible behavior or hardware
  requirements, update the project README.
- Build verification: project + top-level + CCS
  (see [§"Testing the build"](#testing-the-build)).

## Porting a project to a new processor or board

- Build infrastructure:
  - Copy `firmware/<existing_board>/` to `firmware/<new_board>/`. If porting
    to a different device with a different PRU subsystem, also verify
    `linker.cmd` memory sizes against the target device data sheet.
  - For projects with MCU+ host code: copy the MCU+ board directory too.
  - Add the new board/device to the project `makefile` and, if it introduces
    a device not previously in the parent makefile, to the parent makefile.
    For makefile target patterns, see
    [Creating a New Project](./open_pru_create_new_project.md)
    §"Customize the project makefile".
- Documentation:
  - Update the project README "Validated HW & SW" section for the new board.
  - Update the portability table in `academy/readme.md` or `examples/readme.md`
    (see [§"Update the section-level readme"](#update-the-section-level-readme)).
- Build verification: core + project + top-level + CCS
  (see [§"Testing the build"](#testing-the-build)).

## Adding a new project

- Build infrastructure: follow
  [Creating a New Project in the OpenPRU Repo](./open_pru_create_new_project.md).
  If the project includes MCU+ code, also follow
  [Adding MCU+ Code to a New Project](./open_pru_create_new_mcuplus_project.md).

  > [!NOTE]
  > TI uses an automated top-level build to verify that all projects compile.
  > Hardware validation is the project author's responsibility — TI cannot
  > validate every PR on physical hardware. This makes it critical to document
  > exact SDK and tool versions in the README: it gives future developers a
  > known-good starting point if the project stops building after an SDK update.

- Documentation: create `README.md`
  (see [§"Project-level readme"](#project-level-readme)); update the section-level
  readme (see [§"Update the section-level readme"](#update-the-section-level-readme)).
- Build verification: project + top-level + CCS
  (see [§"Testing the build"](#testing-the-build)).

## Reference: project documentation

### Project-level readme

A project's README file should include all information that another programmer
would need to run the example:

* An overview of what the project does

* A section titled "Supported Combinations" that points users to the
  section-level readme, section "Supported processors per-project".

* A section titled "Validated HW & SW" that lists:

    * the specific hardware used to test the code
        * include details like board revision number

    * The exact version of all SDKs and tools where the project was validated on hardware
        * (for example, AM64x MCU+ SDK 11.0, CCS v12.8.1)
        * Suggest to also list the OpenPRU commit message which was used to run
          the tests, or at least the OpenPRU version.

* A high level explanation of any important concepts to understand the code

* All steps a programmer needs to follow in order to run the project and
  validate the outputs. Do not include generic build and load steps (CCS
  import, build, launch debug session, load `.out` file) — those steps are
  covered by the PRU Getting Started Labs. Include only steps that are
  specific to the project: pre-run data setup, which core(s) to use,
  required run order, and expected output validation.

### Update the section-level readme

When adding a new project, also update the readme in the project's parent
directory (`examples/readme.md` or `academy/readme.md`):

1. **Project description**: Add the project name and a 1–2 sentence
   description under the appropriate heading in the `## Projects` section.
   Follow the format of the existing entries (project name on one line,
   bullet with description on the next line).

2. **Portability table**: Add a row to the `## Supported processors per-project`
   table. Fill in each device column with the correct value:
   - `Y` — project has build infrastructure for this device
   - `Yport` — project can be ported but has no build infrastructure yet
   - `Npru` — project is not compatible with the PRU subsystem on this device
   - `N-hw` — project relies on hardware that does not exist on this device
   - `N-sw` — project's non-PRU software is not compatible with this device

   If the project's compatibility with a device is not known, mark the entry
   with a FIXME comment — do not guess.

## Reference: build verification

### Testing the build

**Command-line (make) build**

First, make sure that the project make file builds from the project directory.
Then, check that the project make builds from the top-level directory.

```
$ cd /path/to/open-pru/examples/new_project

// remove files from previous builds
$ make -s clean

// view all build output to make sure there are no sneaky errors
// in order to see all the prints, do not use -s
$ make

// now test the top-level build
$ make -s clean
$ cd /path/to/open-pru
$ make -s
```

**CCS build**

For steps to import, rename, and build a CCS PRU project in the CCS
IDE, refer to **the PRU Academy > Getting Started Labs > Lab 1: How to Create a
PRU Project > Creating a CCS PRU Project**.

**GitHub CI builds**

You can also trigger both a makefile build and a CCS build for all projects in
the OpenPRU repo by using the GitHub workflows.

To trigger a makefile build:

1. Push the latest version of the code from your local repository to the
   remote OpenPRU repository.
2. Go to <https://github.com/TexasInstruments/open-pru> > Actions tab >
   select the "Makefile CI" workflow in the left sidebar.
3. You should see a table entry which says "This workflow has a
   `workflow_dispatch` event trigger." Select "Run workflow" > Use workflow
   from Branch: \<feature_branch\>

To trigger a CCS build for all projects, follow the same steps and select
"CCS Build" in the left sidebar.
