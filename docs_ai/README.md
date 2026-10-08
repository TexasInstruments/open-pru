# docs_ai/ — AI-Agent Documentation for OpenPRU

This README file
- directs you to the correct task runbook, if one exists
- lists useful reference files with when-to-read triggers

## Task runbook index

| Task | File |
|------|------|
| Create a new OpenPRU project | `docs_ai/task_create_project.md` |
| Port an existing project to a new device or board | `docs_ai/task_port_project.md` |

## Building with makefiles

Run `make help` from the repo root for the authoritative list of build targets
and options. Use `gmake` instead of `make` on Windows (gmake ships with CCS).

## Reference files (read on demand)

Read these files when directed to by a task runbook, or when the when-to-read
trigger applies. Do not read them upfront.

| File | Contents | When to read |
|------|----------|--------------|
| `docs/open_pru_organization.md` | Repo layout and project structure | Before placing a new project, directory, or source file |
| `docs/release_notes.md` | Compatible SDK versions per release, known issues and workarounds, feature additions and changes | When checking SDK-version compatibility, investigating a known issue, or confirming when a feature was added or changed |
| `docs/open_pru_create_new_project.md` | Project creation patterns | When creating a new project |
| `docs/open_pru_create_new_mcuplus_project.md` | Project creation patterns with MCU+ code | When creating a new project with MCU+ (R5F) host code |
| `best_practices.md` | Coding standards for PRU assembly and C | Before writing or reviewing PRU C/assembly; skip for makefile, projectspec, linker, or other build-infrastructure files |
| `docs/PRU Assembly Instruction Cheat Sheet.md` | PRU instruction reference | When writing or debugging PRU assembly |
| `docs_ai/reference/pru_subsystem_features_comparison_g/pru_subsystem_features_comparison_g.md` | Feature-by-feature comparison of PRU-ICSS, PRU_ICSSG, and PRUSS subsystems across devices | When selecting a device or confirming whether a PRU feature/peripheral exists on the target subsystem |
| `docs_ai/reference/pru_subsystem_migration_guide/pru_subsystem_migration_guide.md` | PRU-ICSS vs PRU_ICSSG hardware differences: memory maps, constant tables, I/O, interrupts, peripherals (UART/eCAP/PWM/IEP/MDIO/MII_RT) | When porting firmware between PRU-ICSS and PRU_ICSSG subsystems, or resolving register/memory-map/peripheral differences |

**Deep references** (read only when a compiler or assembler question arises;
grep for the relevant section first rather than reading the file in full):

| File | Contents | When to read |
|------|----------|--------------|
| `docs_ai/reference/pru_assembly_language_tools_users_guide_v2_3/pru_assembly_language_tools_users_guide_v2_3.md` | PRU assembler, linker, and object-tools reference (directives, sections, pseudo-ops) | For PRU assembler syntax, directives, or linker/section questions |
| `docs_ai/reference/pru_optimizing_c_compiler_users_guide_v2_3/pru_optimizing_c_compiler_users_guide_v2_3.md` | PRU C/C++ compiler reference (pragmas, intrinsics, optimization, run-time environment) | For PRU C compiler pragmas, intrinsics, or optimization questions |

**Technical Reference Manual (TRM) and Register info**
The PRU subsystem is documented in a chapter of the device TRM. TRM is the
reference for information like:
- Subsystem overview, memory map, and integration
- PRU cores, module interface (including `R30` output / `R31` input), and the constant table
- Interrupt controller (INTC) and event/interrupt mapping
- Peripherals and hardware accelerators in the subsystem (IEP, UART, etc.)

For the register-level detail (bit fields, offsets, reset values), refer to the
Register documentation.

The TRM and Register documentation are both VERY large PDF files - the PRU
chapter alone is 200+ pages in some TRMs - so identify the chapter, section, or
register you need and read only that span (use a page range if your tooling
supports it) rather than reading the PDF in full.

| Device family  | TRM | TRM chapter name | Register documentation |
|----------------|-----|------------------|------------------------|
| AM243x / AM64x | https://www.ti.com/lit/spruim2 | "Programmable Real-Time Unit and Industrial Communication Subsystem - Gigabit (PRU_ICSSG)" | Same TRM, "PRU_ICSSG Registers" subsection of the PRU_ICSSG chapter |
| AM261x | https://www.ti.com/lit/sprujb6 | "Programmable Real-Time Unit Subsystem (PRU-ICSS)" | https://www.ti.com/lit/spruj94 > "ICSSM" |
| AM263Px | https://www.ti.com/lit/spruj55 | "Programmable Real-Time Unit Subsystem (PRU-ICSS)" | https://www.ti.com/lit/spruj57 > "ICSSM" |
| AM263x | https://www.ti.com/lit/spruj17 | "Programmable Real-Time Unit Subsystem (PRU-ICSS)" | https://www.ti.com/lit/spruj42 > "ICSSM" |
| AM62x | https://www.ti.com/lit/spruiv7 | "Programmable Real-Time Unit Subsystem (PRUSS)" | Same TRM, "PRUSS Registers" subsection of the "Registers" chapter |

---

Modifying docs_ai itself (adding a tasklist, reference, or feature runbook)?
See `docs_ai/authoring_guide.md`. Day-to-day task agents do not need it.
