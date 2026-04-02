# Task: Add New Features to an Existing Project

## Pre-read list

Read these files before starting:

1. `best_practices.md` — coding standards for PRU assembly and C
2. The project's `firmware/` source files — understand the existing structure
3. `docs/open_pru_organization.md` — if unfamiliar with the project layout
4. `docs/PRU Assembly Instruction Cheat Sheet.md` — when writing or reading
   PRU assembly

**Deep references** (read only when a compiler or assembler question arises):

- PRU assembly language tools user guide:
  typically at `$CG_TOOL_ROOT/../../../docs/pru_assembly_language_tools_users_guide*.txt`
- PRU optimizing C compiler user guide:
  typically at `$CG_TOOL_ROOT/../../../docs/pru_optimizing_c_compiler_users_guide*.txt`

## Decision tree

**Step D1. Does the feature require new source files?**

- Yes: follow Section A (adding source files) before Section B.
- No: proceed directly to Section B (modifying existing code).

**Step D2. Does the feature require changes to the linker script?**

Linker changes are needed when:
- Adding a new memory section
- Increasing data memory usage beyond current allocation
- Adding a new shared memory region

→ Yes: edit `firmware/<board>/<core>/ti-pru-cgt/linker.cmd`. Verify the new
  allocation fits within the PRU's physical memory limits (8KB data RAM per
  core; shared data RAM varies by device — see `best_practices.md`).

**Step D3. Does the feature require changes to the host side?**

- R5F MCU+: changes go in the MCU+ core directory. Some projects use
  `mcuplus/<board>/<core>/ti-arm-clang/`; others use `<board>/<core>/ti-arm-clang/`
  at the project root. Match the convention used by the project.
- Linux A53: changes go in `linux/<board>/`
- Both PRU firmware and host code must be kept in sync for any change to a
  shared memory interface.

**Step D4. Does the feature require new or changed pin connections?**

Determine this by inspecting the feature's requirements and any code being
added: does it write to or read from R30/R31? Does it call I2C, SPI, UART,
I2S, ADC, GPIO, or other peripheral drivers?

- Yes: flag pin routing as a required manual step (see B7). Describe to the
  user which signals or peripherals need pin routing based on the new
  feature's requirements. Do not attempt to edit any configuration file.
- No: no pin routing work needed.

---

## Section A: Adding new source files

- [ ] A1. Place shared source files (used by multiple cores or boards) in the
  project's `firmware/` top-level directory.
- [ ] A2. Place core-specific source files in
  `firmware/<board>/<core>/ti-pru-cgt/`.
- [ ] A3. Update the core `ti-pru-cgt/makefile` if needed:
  - If the new source file is in a directory already listed in `FILES_PATH`,
    no makefile change is needed — `pru_rules.mak` auto-discovers all `.asm`
    and `.c` files by globbing each directory in `FILES_PATH`.
  - If the new source file is in a new directory: add that directory to
    `FILES_PATH`.
  - For new include directories: append to `INCLUDE` after the `pru_rules.mak`
    include (do not use `INCLUDES_common` — that variable belongs to
    `ti-arm-clang/makefile`).
- [ ] A4. Add the new file to `example.projectspec` for CCS builds:
  - Add a `<file path="..." action="copy"/>` entry inside `<project>`.
  - Count the directory levels from the repo root down to the `ti-pru-cgt/`
    directory — that is the number of `../` levels needed to reach the repo root.
    Verify against an existing `../` entry in `example.projectspec` or
    `makefile_projectspec` in the same directory.
- [ ] A5. For new assembly `.inc` files, add a header guard:
  ```asm
  .if !$isdefed("____filename_inc")
  ____filename_inc   .set     1

  ; contents

  .endif ; ____filename_inc
  ```

---

## Section B: Modifying existing code

- [ ] B1. For any assembly macro or function being modified, update the
  NaturalDocs documentation block before editing the implementation:
  - Pseudo code
  - PEAK cycles (worst-case, hand-counted or profiled)
  - Registers modified (including those modified by invoked macros)
  Format: see `best_practices.md` §"Function/Macro Documentation".

- [ ] B2. For new macros and functions, follow naming conventions from
  `best_practices.md`:
  - Macro: `m_<name>` (lowercase, underscore_case)
  - Function: `FN_<NAME>` (uppercase, underscore_case)
  - Labels: `UPPERCASE_LABEL` with function/macro short name as prefix
  - Local labels in macros: use `?` suffix to avoid name collisions

- [ ] B3. For C host code changes, follow patterns from `best_practices.md`:
  - Check all API return values with `DebugP_assert`
  - Write shared memory data fields before the command/flag field
  - Use `__sync_synchronize()` before writing any trigger field

- [ ] B4. Confirm the new assembly code does not corrupt caller registers.
  List all modified registers in the documentation block.

- [ ] B5. For timing-critical sections, count cycles for the added code and
  update the PEAK cycle total for any macro or function that was changed.

- [ ] B6. Use FIXME (not TODO) for markers on pending work.

- [ ] B7. If D4 identified new or changed pin connections, inform the user
  that pin routing must be updated manually:
  - MCU+ projects: update `example.syscfg` in SysConfig.
  - Linux projects: update the relevant device tree file (`.dts`/`.dtbo`).
  - Standalone PRU projects: no host-side configuration file; verify that
    the PRU firmware directly manages its R30/R31 bit fields as needed.
  Describe which signals require changes. Do not edit any configuration file.

---

## Verification steps

- [ ] V1. Build the project after each significant change:
  `cd <project> && make -s clean && make`
  Expected: zero errors, zero unexpected warnings.
- [ ] V2. If assembly was changed, confirm the `.out` size fits in instruction
  memory. Check the linker map file (in the build output directory) for
  section sizes.
- [ ] V3. Build from the repo top level: `cd <repo_root> && make -s`
- [ ] V4. If CCS support is required, import and build in CCS.
- [ ] V5. Review the diff against `best_practices.md` checklists before
  submitting a PR.

---

## Do-not-do list

- Do not add a source file to `example.projectspec` without checking whether
  the file is in a directory already scanned by `FILES_PATH` in the core
  `ti-pru-cgt/makefile`. The command-line build auto-discovers files by
  directory; CCS requires explicit `<file>` entries in `example.projectspec`.
  The two builds diverge if `example.projectspec` is not updated.
- Do not modify files under `source/` (shared libraries) without verifying
  impact across all projects that use them. Grep for the filename first.
- Do not use RTU_PRU or TX_PRU-specific instructions without first verifying
  the target device has those core types (see `docs_ai/task_port_project.md`
  Step D3 for the device breakdown).
- Do not use TODO markers — use FIXME.
- Do not commit build output files (`debug/`, `release/`, `*_load_bin.h`).
- Do not add signal I/O (R30/R31 register access, peripheral driver calls)
  to PRU firmware or host code without flagging `example.syscfg` pin routing
  as a required manual step. Do not edit `example.syscfg` pin routing —
  inform the user what needs to be configured in SysConfig.
