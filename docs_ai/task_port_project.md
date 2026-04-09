# Task: Port an Existing Project to a New Device or Board

## Pre-read list

Read these files before starting. Do not skip based on familiarity.

1. `docs/open_pru_organization.md` — repo layout and project structure
2. The project's existing `makefile` — understand current `SUPPORTED_PROCESSORS`
3. The project's existing `firmware/<board>/<core>/ti-pru-cgt/makefile` for an
   existing device/board — understand the build patterns to replicate
4. `academy/readme.md` or `examples/readme.md` §"Supported processors per-project"
   — check portability status of the project before assessing manually

## Decision tree

**Step D1. Is the target device in this repo's supported list?**

Check `imports.mak` (or `imports.mak.default`) for valid `DEVICE` values.
→ If not supported: stop. The device is not supported by this repo.

**Step D2. New board variant for the same device, or a different device?**

- Same device, new board (for example, am64x-evm → am64x-lp):
  → Go to Section A (board-only port).
- Different device (for example, am64x → am243x):
  → Go to Section B (device port). Complete Step D3 first.

**Step D3. Is the project compatible with the target device?**

First, check the portability table in `academy/readme.md` or `examples/readme.md`
(§"Supported processors per-project"). The table uses these codes:
- **Y** / **Yport**: already supported or can be ported
- **Npru**: PRU subsystem on the target device lacks features the project needs
- **N-hw**: project depends on SoC hardware not present on the target device
- **N-sw**: project's non-PRU software is not compatible with the target device

If the table covers the source→target combination, use it as your answer and
proceed or stop accordingly. If the project is not in the table, assess each
dimension below.

**Npru — PRU subsystem compatibility:**
Grep the firmware source for: `XFR`, `xfer`, `RTU`, `TX_PRU`, `xin`, `xout`.
- Found: verify the target device's PRU subsystem supports those features.
  - PRU_ICSSG (has RTU_PRU, TX_PRU, accelerators): AM243x, AM64x
  - PRU-ICSSM (PRU0/PRU1 only): AM261x, AM263x, AM263Px
  - PRUSS (PRU0/PRU1 only): AM62x
  - Feature-by-feature comparison: TI app note
    [PRU Subsystem Features Comparison](https://www.ti.com/lit/sprac90).
  - If the project uses PRU_ICSSG-only features and the target is PRU-ICSSM or
    PRUSS → Npru: stop.

**N-hw — SoC hardware compatibility:**
Identify which SoC peripherals the project uses (SPI, I2C, ADC, LCD, specific
memory interfaces, etc.). Verify those peripherals exist on the target SoC. If
a required peripheral is absent → N-hw: stop or substitute.

**N-sw — Non-PRU software compatibility:**
Check whether the project's host code assumes MCU+ (R5F) or Linux (A53).
- AM62x uses Linux on A53 and does not support MCU+ R5F host code.
- Projects with hard real-time host requirements may not be compatible with Linux.
- If the host code needs significant rework → N-sw: new host code is required
  alongside the PRU firmware port.

**I/O — Signal I/O and pinmux:**
Pin routing is board-specific and must not be copied from the source project
to a different board. Before proceeding, detect whether the project uses
signal I/O, then ask the user to confirm whether pin routing needs
reconfiguring for the target board.

Detection — run all of the following:
- PRU firmware (assembly and C): grep for `r30`/`R30` and `r31`/`R31`.
  Reads and writes to R30/R31 indicate the PRU is using its I/O interface,
  but do not indicate which entity (PRU or host) configures the pinmux.
  Also grep for peripheral driver calls and direct peripheral register
  access (I2C, SPI, UART, I2S, ADC, GPIO expanders, and similar) in both
  assembly and C source files across the entire project.
- MCU+ host: check `example.syscfg` for PRU GPIO signal assignments or
  other pin mux configuration.
- Linux host: check for devicetree source files (`.dts`, `.dtsi`, `.dtbo`)
  that configure pin mux for the PRU subsystem.

After running detection, summarize findings and ask the user:
"This project appears to use signal I/O based on [evidence]. Please confirm
whether pin routing must be reconfigured for the target board."

→ I/O confirmed by user: flag `example.syscfg` pin routing as a required
  manual step. The syscfg selection in Section A/B will copy a starting-point
  file; the user must configure pin routing in SysConfig afterward.
→ No I/O: no pin routing work needed.

→ If all three pass: proceed to Section B.
→ If any block: do not proceed without resolving or explicitly scoping the blocker.

---

## Section A: Board-only port (same device family)

- [ ] A1. Identify an existing board directory to copy:
  `firmware/<existing_board>/`
- [ ] A2. Copy it to `firmware/<new_board>/`. Preserve the core subdirectory
  names (for example, `icss_g0_pru0_fw/`).
- [ ] A3. In each copied `ti-pru-cgt/makefile`:
  - Update the include path for `imports.mak` (count `../` levels from this
    file to the repo root).
  - Update any board-specific defines or pin macros.
- [ ] A4. In each copied `ti-pru-cgt/linker.cmd`:
  - Verify memory sizes match the target board's PRU subsystem. Check the
    device TRM or an existing project for that board.
- [ ] A5. In each copied `ti-pru-cgt/example.projectspec`:
  - Update `name=` to a unique CCS project name (convention:
    `<project>_<board>_<core>_fw`).
  - The `postBuildStep` writes to `../pru0_load_bin.h` (relative to the build
    output dir) — this path is board-agnostic and does not need updating.
  - Update any board-specific compiler defines.
  - Also update `PROJECT_NAME` in `makefile_projectspec` to match the new
    `name=` value.
- [ ] A6. If the project has MCU+ code, repeat A2–A5 for the MCU+ directory.
  Check the existing project structure first: some projects use
  `mcuplus/<new_board>/<core>/ti-arm-clang/` (e.g., `examples/empty`);
  others use `<new_board>/<core>/ti-arm-clang/` directly at the project root
  (e.g., `academy/intc/intc_mcu`). Match the convention used by the project
  being ported.
  The MCU+ core directory (`ti-arm-clang/`) must contain: `makefile`,
  `makefile_projectspec`, `makefile_ccs_bootimage_gen`, `example.projectspec`,
  `syscfg_c.rov.xs`.
  The parent board+core directory must contain: `main.c`, `example.syscfg`.
  For `example.syscfg`: use the Situation 2 selection algorithm — present the
  user with existing syscfg files for the target board and ask which to use as
  a template (default: the appropriate empty project). Copy the chosen file.
  Compare it against the source project's syscfg and summarize differences.
  If D3 flagged signal I/O, inform the user that pin routing must be
  configured manually in SysConfig for the new board. Do not edit pin routing.
- [ ] A7. Update the project `makefile`:
  - Add the new board to the appropriate device target.
  - Pattern: grep the existing `makefile` for the old board name to find all
    locations that need updating.
- [ ] A8. Update the parent directory `makefile` if a new device target entry
  is needed (required when adding a device not previously in the parent).

---

## Section B: Different device port

- [ ] B1. Complete Step D3 above. Resolve any Npru/N-hw/N-sw blockers before
  continuing.
- [ ] B2. Identify the closest existing device directory as a template.
- [ ] B3. Follow all steps in Section A, treating the new device as the new
  board.
- [ ] B4. Enumerate all PRU core types referenced in the project — both in
  PRU firmware source and in the host code. The default mapping for any port
  is: PRU0 → PRU0, PRU1 → PRU1, RTU_PRU0 → RTU_PRU0, TX_PRU0 → TX_PRU0,
  and so on. If the host code or PRU firmware references a core type that is
  absent from the source device or the destination device, stop and ask the
  user how to handle those cores before continuing. Do not remap core types
  or add firmware for unused cores without explicit user instruction (see
  Do-not-do list). If the destination device has additional core types not
  referenced in the source project, leave those cores unused.
- [ ] B5. Search the new directory for the old device name and update every
  match. Device names appear in many forms; search for all variants:
  ```
  grep -r "am64x\|AM64x\|AM64X\|AM6432\|Cortex R.AM64" <new_device_dir>/
  ```
  (Replace the old device name pattern with the one you are porting from.)
  Common locations: `ti-arm-clang/makefile` (SOC define, lib names, FreeRTOS
  config path), `example.projectspec` (deviceId, products list,
  package/part flags).
- [ ] B6. Update the project `makefile`:
  - Add the new device to `SUPPORTED_PROCESSORS`.
  - Add a new device-specific make target block. Pattern: copy an existing
    device block and update device name, board name, and core names.
- [ ] B7. Verify `imports.mak.default` lists the new device as a valid
  `DEVICE` option. Do not modify `imports.mak` (it is user-local and
  git-ignored).

---

## Verification steps

- [ ] V1. Build PRU firmware from the core directory:
  `make -s -C firmware/<new_board>/<core>/ti-pru-cgt`
  Expected: no errors; `.out` generated; `*_load_bin.h` generated if the
  project has a MCU+ host (not produced for AM62x Linux-host projects).
- [ ] V2. Build from the project directory:
  `cd <project> && make -s clean && make`
  Expected: no errors.
- [ ] V3. Build from the repo top level:
  `cd <repo_root> && make -s`
  Expected: new board/device builds without errors.
- [ ] V4. Import and build in CCS. Verify `example.projectspec` opens without
  errors and builds PRU firmware and R5F (if present).
- [ ] V5. Update the portability table in `academy/readme.md` or
  `examples/readme.md`: change `Yport` to `Y` for each device/board
  combination that was just successfully ported and verified.

---

## Do-not-do list

- Do not commit the `debug/` or `release/` build output directories.
- Do not hard-code absolute paths in makefiles or projectspecs. Use variables
  like `$(OPEN_PRU_PATH)` and the `imports.mak` include pattern.
- Do not update `SUPPORTED_PROCESSORS` without also adding the corresponding
  make target block in the project `makefile`.
- Do not assume memory sizes are identical across device families. Always
  check `linker.cmd` against the target device data sheet.
- Do not skip the top-level build verification (V3). Errors in the parent
  makefile will break CI for all contributors.
- Do not remap PRU core types or add firmware for cores absent from the
  source project when porting to a device with additional core types. PRU0
  maps to PRU0, PRU1 maps to PRU1; if the host code references a core type
  not covered by the PRU firmware source, stop and ask the user.
- Do not edit `example.syscfg` pin routing, PRUICSS configuration, or
  device settings. Use the Situation 2 selection algorithm to copy a
  starting-point syscfg, highlight differences, and flag all remaining
  SysConfig configuration as manual steps for the user.
