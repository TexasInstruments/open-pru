# Task: Create a New OpenPRU Project from Scratch

## Pre-read list

Read these files before starting:

1. `docs/open_pru_create_new_project.md` — authoritative step-by-step guide
   If the project includes MCU+ code (see Step D1):
   also read `docs/open_pru_create_new_mcuplus_project.md`
2. `docs/open_pru_organization.md` — repo layout and project structure
3. `best_practices.md` — coding standards
4. An existing similar project as a structural reference. Two empty starting
   projects are available:
   - `examples/empty` — PRU firmware in assembly
   - `examples/empty_c` — PRU firmware in C or mixed C and assembly
   Both include `mcuplus/` directories. Choose based on firmware language.

## Decision tree

**Step D1. Which host interface does the project use?**

- MCU+ R5F host → project needs `firmware/` and `mcuplus/` directories
- Linux A53 host → project needs `firmware/` and `linux/` directories
- Both MCU+ and Linux → project needs `firmware/`, `mcuplus/`, and `linux/`
- PRU standalone (no host) → project needs `firmware/` only

**Step D2. Which devices and boards will the project support?**

Valid device identifiers: am243x, am261x, am263px, am263x, am62x, am64x.
Board suffix variants (device-dependent): -evm, -lp, -cc, -som.
→ List all `<device>-<board>` combinations before starting.
→ For each combination, identify the PRU cores needed. Potential cores per-processor:
  - PRU_ICSSG (AM243x, AM64x): pru0, pru1, rtu_pru0, rtu_pru1, tx_pru0, tx_pru1
  - PRU-ICSSM (AM263x, AM261x, AM263Px): pru0, pru1
  - PRUSS (AM62x): pru0, pru1
→ Ask the user whether the new project will use signal I/O — direct R30/R31
  register access, I2C, SPI, UART, I2S, ADC, GPIO, or other peripheral or
  physical pin connections. If yes, note that pin routing is a required
  manual step — for MCU+ projects via `example.syscfg` (see B5); for Linux
  projects via device tree files. Do not attempt to configure it.

**Step D3. Is this a PR submission to the main branch?**

- Yes: complete all sections including Section D (documentation).
- No (private use): Sections A–C are required; Section D is recommended.

---

## Section A: Copy and rename the starting project

- [ ] A1. Choose a starting project. Two empty baselines are available:
  `examples/empty` (PRU assembly) and `examples/empty_c` (PRU C or mixed C/assembly).
  Use an existing project with a similar structure if one exists.
  See `docs/open_pru_create_new_project.md` §"Copy existing project".
- [ ] A2. Copy the starting project to the new location:
  `cp -r <parent>/<source_project> <parent>/<new_project>`
  where `<parent>` is `examples/` or `academy/<topic>/` as appropriate.
- [ ] A3. Remove board/device directories not needed by the new project.
  Remove from both `firmware/` and `mcuplus/` (or `linux/`).
- [ ] A4. Find and replace the old project name throughout all copied files.
  Key locations:
  - Project `makefile`: `PROJECT_NAME`, `PRU_DEPENDENCIES`,
    `NON_PRU_DEPENDENCIES`, all board/device target names
  - All `ti-pru-cgt/makefile` files: output file names, include paths
  - All `ti-pru-cgt/example.projectspec` files: `name=` attribute, all paths
  - All `ti-arm-clang/makefile` and `example.projectspec` files (if MCU+)

---

## Section B: Customize the makefiles

See `docs/open_pru_create_new_project.md` §"Customize the makefiles" for the
authoritative reference. Key items for each file type:

- [ ] B1. Project `makefile`:
  - Set `PROJECT_NAME`.
  - Set `SUPPORTED_PROCESSORS` to the device list from Step D2.
  - Verify `PRU_DEPENDENCIES` and `NON_PRU_DEPENDENCIES` are correct.
  - Update the include path for `imports.mak` (count `../` levels from the
    project directory to the repo root).

- [ ] B2. Each PRU core makefile (`ti-pru-cgt/makefile`):
  See `docs/open_pru_create_new_project.md` §"Customize the core makefiles". Key items:
  - Update the include path for `imports.mak` (same folder-depth rule as the
    project `makefile`).
  - Update `OUTPUT_NAME` to match the new project name.
  - For MCU+ projects: update `MCU_HEX_NAME`, `HEX_ARRAY_PREFIX`, and
    `MCU_HEX_PATH` to point to the new project's firmware board directory.
  - For projects without an MCU+ host (Linux host or standalone PRU): only
    `OUTPUT_NAME` requires updating; `MCU_HEX_PATH` is not used by any host
    project. In `example.projectspec`, leave `postBuildStep` empty or omit it.
  - If source files are in a new directory: add that directory to `FILES_PATH`.
    `pru_rules.mak` auto-discovers all `.asm` and `.c` files in each listed
    directory — there is no separate `ASMFILES` or `CFILES` to update.
  - For additional include paths beyond the defaults: append to `INCLUDE` after
    the `pru_rules.mak` include.

- [ ] B3. Each MCU+ core makefile (`ti-arm-clang/makefile`, if MCU+):
  See `docs/open_pru_create_new_mcuplus_project.md`
  §"Customize the core makefiles (MCU+ side)". Key items:
  - If the source came from the MCU+ SDK: replace the `MCU_PLUS_SDK_PATH`
    `imports.mak` include with the `OPEN_PRU_PATH` pattern (same folder-depth
    rule as the project `makefile`).
  - If the source came from another OpenPRU project: update the `OPEN_PRU_PATH`
    depth in the existing include.
  - Add `${OPEN_PRU_PATH}/source` and the PRU firmware board path to
    `INCLUDES_common`.
  - Update `FILES_common` to list the actual source files for the new project.

- [ ] B4. Parent directory `makefile`:
  - Add the new project to the appropriate target list so it builds from the
    top level. Pattern: copy an existing project entry from the same makefile.
  - For `academy/` projects under a topic subdirectory, the entry in
    `academy/makefile` uses the topic-relative path `<topic>/<project>` (for
    example, `uart/uart_echo`), not just `<project>`. There is no intermediate
    per-topic `makefile` in the academy directory.

- [ ] B5. Select and copy a starting-point `example.syscfg`:
  - **Skip this step if D1 identified a Linux-only or standalone PRU host.**
    Those project types do not use `example.syscfg`. For Linux projects,
    pin routing is configured in device tree files (outside the scope of
    this runbook). For standalone PRU projects, no host-side configuration
    file is needed. If D2 identified signal I/O for either of these project
    types, inform the user that pin routing must be configured separately.
  - Glob for `example.syscfg` files in the repo under paths matching the
    target device/board. Include the appropriate empty project as the
    default: `examples/empty` for assembly firmware, `examples/empty_c`
    for C or mixed firmware.
  - Present the list to the user: "Which of these syscfg files should I
    use as a starting point? The empty project is the default."
  - Copy the chosen syscfg into the new project's board directory.
  - Inform the user: "I copied `example.syscfg` from [chosen project].
    It has not been modified."
  - If no syscfg exists for the target device/board, stop and inform the
    user: "No existing `example.syscfg` found for [device/board]. You
    will need to create one from scratch in SysConfig."
  - The following items in `example.syscfg` require manual review in
    SysConfig — do not attempt to edit them:
    - Device string and memory regions (verify they match the target device)
    - I/O pin routing: if the project uses signal I/O (confirmed in D2),
      configure pin routing for each target board
    - PRUICSS instance: verify the subsystem type, instance number, and
      interrupt routing are correct for the target device

---

## Section C: Write the firmware

- [ ] C1. Start with the correct file structure from `best_practices.md`:
  - **Assembly firmware**: use §"File Structure and Organization" —
    copyright header, `.retain`, `.retainrefs`, `.global main`,
    `.sect ".text"`, register structure definitions, main entry point.
  - **PRU C firmware**: use §"PRU C Firmware Best Practices — File Structure" —
    copyright header, `void main(void)` entry point, `__halt()` at exit.

- [ ] C2. Document all macros and functions before implementing them
  (NaturalDocs format — see `best_practices.md` §"Function/Macro
  Documentation"):
  - Pseudo code
  - PEAK cycles
  - Registers modified

- [ ] C3. After the first successful build, check the linker map file to
  confirm the `.out` size fits within instruction memory.

- [ ] C4. For host code (R5F MCU+), follow patterns from `best_practices.md`
  §"C Code Best Practices (MCU+ Host Cores)":
  - Check all API return values with `DebugP_assert`
  - Write shared memory data before the command/flag field
  - Clean up all resources on error paths

---

## Section D: Documentation and PR requirements

- [ ] D1. Create `README.md` in the project root directory. Required content:
  - Overview of what the project does
  - Exact hardware used (board name and board revision number)
  - Exact SDK and tool versions validated on hardware
    (for example: AM64x MCU+ SDK 11.0, CCS v12.8.1)
  - Step-by-step instructions to run the project and validate the outputs.
    Do not include generic build and load steps (CCS import, build, launch
    debug session, load `.out` file) — those are covered by the PRU Getting
    Started Labs. Include only steps specific to this project: pre-run data
    setup, which core(s) to use, required run order, and expected output
    validation.

- [ ] D2. Verify the project builds from the project directory and from the
  repo top level. See `docs/contributing.md` §"Testing the build"
  for the exact commands.

- [ ] D3. Verify CCS import and build succeed without errors.

- [ ] D4. Update `academy/readme.md` or `examples/readme.md`:
  - Add a 1–2 sentence description under the appropriate heading in the
    `## Projects` section.
  - Add a row to the `## Supported processors per-project` table.
  See `docs/contributing.md` §"Update the section-level readme" for the
  exact format of both.

---

## Verification steps

- [ ] V1. From the project directory: `make -s clean && make`
  Expected: no errors; all expected output files generated.
- [ ] V2. From the repo root: `make -s clean && make -s`
  Expected: new project is included and builds without errors.
- [ ] V3. In CCS: import `example.projectspec`, build PRU firmware first,
  then R5F (if present). No errors or warnings about missing files.
- [ ] V4. Confirm `README.md` covers all items in Section D1.

---

## Do-not-do list

- Do not create a new project without copying an existing one. The build
  infrastructure has many interdependent patterns that are easier to replicate
  than to reconstruct from scratch.
- Do not submit a PR without a `README.md` that meets the Section D1
  requirements.
- Do not use TODO markers — use FIXME (see `best_practices.md`).
- Do not add the project to the parent `makefile` before the project-level
  build succeeds. A broken entry will fail the top-level CI build.
- Do not hard-code absolute paths. Use `$(OPEN_PRU_PATH)` for repo-relative
  paths and the `imports.mak` include pattern for tool paths.
- Do not commit build output directories (`debug/`, `release/`).
- Do not guess portability table values when updating `academy/readme.md` or
  `examples/readme.md`. If compatibility for a device is not known from
  observed sources, mark the entry with a FIXME, then ask the user.
- Do not edit `example.syscfg` pin routing, PRUICSS configuration, or
  device settings — not even with user-provided values. Copy a
  starting-point syscfg and flag all SysConfig configuration as manual
  steps for the user.
