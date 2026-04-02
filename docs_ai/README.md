# docs_ai/ — AI-Agent Documentation for OpenPRU

This README file
- directs you to the correct task runbook, if one exists
- lists useful reference files with when-to-read triggers

## Task runbook index

| Task | File |
|------|------|
| Create a new OpenPRU project | `docs_ai/task_create_project.md` |
| Port an existing project to a new device or board | `docs_ai/task_port_project.md` |

## Reference files (read on demand)

Read these files when directed to by a task runbook, or when the when-to-read
trigger applies. Do not read them upfront.

| File | Contents | When to read |
|------|----------|--------------|
| `docs/open_pru_organization.md` | Repo layout and project structure | Before placing a new project, directory, or source file |
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

---

Modifying docs_ai itself (adding a tasklist, reference, or feature runbook)?
See `docs_ai/authoring_guide.md`. Day-to-day task agents do not need it.
