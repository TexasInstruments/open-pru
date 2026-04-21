<div align="center">

<img src="https://upload.wikimedia.org/wikipedia/commons/b/ba/TexasInstruments-Logo.svg" width="150"><br/>

# Adding MCU+ Code to a New Project in the OpenPRU Repo

</div>

This document discusses how to add MCU+ code to a new project in the OpenPRU
repo.

First, create the new PRU firmware project by completing
[Creating a New Project in the OpenPRU Repo](./open_pru_create_new_project.md).

If your project will not include MCU+ code, skip this document and continue to
**the PRU Academy > Getting Started Labs > Lab 2: How to Write PRU Firmware**.

Commands throughout this document will be given in the format used by the Linux
terminal, or a Windows application like Git Bash.

## Where to find MCU+ code

Some OpenPRU projects already include MCU+ code. One option is to copy that
MCU+ code into the new project.

The MCU+ SDK is the largest source of MCU+ examples. Another option is to find
an example from the MCU+ SDK and copy it into the OpenPRU project.

## Copy the MCU+ code

Before copying the MCU+ example, ensure that it builds successfully in its
original project and repo.

To build an existing project in the OpenPRU repo, follow the instructions in
the `make help` command:

```
$ cd /path/to/open-pru
$ make help
```

If the MCU+ example builds successfully, clean the example before copying.
Remove any existing MCU+ code from the new OpenPRU project before copying in new
MCU+ code.

To build a project in the MCU+ SDK, follow the instructions in the MCU+ SDK
docs: **Getting Started > Build a Hello World example**.

### Where to put the MCU+ code

Put the MCU+ code in a separate directory from the PRU firmware. For example,
the "empty" projects use `mcuplus/<board>/` for MCU+ code and
`firmware/<board>/` for PRU firmware.

For the full directory structure, see
[OpenPRU Organization](./open_pru_organization.md).

## Remove or replace unused processors, boards, and cores

If the MCU+ example code contains processors, boards, or cores that will not be
used in the new project, remove the unused components.

These are the files and directories to modify or delete:

* `<project_name>/makefile`
* `<project_name>/mcuplus/<board>` or `<project_name>/<board>`

### Update the project makefile

Update the **Project information** and **Target definitions** so that the
project makefile calls the core makefiles for the new MCU+ code.

Add `mcuplus` to `NON_PRU_DEPENDENCIES` and add a `<device>_mcuplus` target
definition that calls the MCU+ core makefile.

For the target definition pattern, refer to
[Creating a New Project in the OpenPRU Repo](./open_pru_create_new_project.md)
§ "Customize the project makefile".

## Rename the project and update paths

Find and replace the old project name and path with the new project name and
path throughout the project. These references may be modified by hand, with a
bulk find/replace, or with a script or AI agent.

First, find all references to the old project name:

```
$ cd /path/to/open-pru/examples/<project_name>
# search for filenames from the template project, in this case `empty`
$ find -name "*empty*"
# search for mentions within files
$ grep -r empty
```

> [!WARNING]
> Inspect the output carefully before running any bulk find/replace command,
> script, or AI agent. Review all changes before committing. Avoid replacing
> strings in unintended locations.

> [!NOTE]
> MCU+ source files may have their own project-specific names (for example,
> `empty_example.c`). Search for and rename those files and their references
> separately.

## Customize the makefiles

### Customize the project makefile

Refer to
[Creating a New Project in the OpenPRU Repo](./open_pru_create_new_project.md)
§ "Customize the project makefile". Verify that:

- `NON_PRU_DEPENDENCIES` includes `mcuplus`
- The target definitions include the MCU+ core. For example:

```makefile
am243x_mcuplus:
	$(MAKE) -C mcuplus/am243x-evm/r5fss0-0_freertos/ti-arm-clang $(ARGUMENTS_MCUPLUS)
```

> [!NOTE]
> Update the path to match your project's actual MCU+ directory structure.
> Some projects place MCU+ code under `mcuplus/<board>/` (e.g., `examples/empty`);
> others place it directly under `<board>/` at the project root
> (e.g., `academy/intc/intc_mcu`).

### Customize the core makefiles (MCU+ side)

#### MCU+ code from the MCU+ SDK

Replace the MCU+ SDK `imports.mak` include with the OpenPRU pattern.
For example:

```
# Replace:
export MCU_PLUS_SDK_PATH?=$(abspath ../../../../../../..)
include $(MCU_PLUS_SDK_PATH)/imports.mak

# With:
export OPEN_PRU_PATH?=$(abspath ../../../../../..)
include $(OPEN_PRU_PATH)/imports.mak
```

Add OpenPRU include paths to `INCLUDES_common`. Include the path to the PRU
hex array file(s) used by the MCU+ code:

```
INCLUDES_common := \
    ...
    -I${OPEN_PRU_PATH}/source \
    -I${OPEN_PRU_PATH}/examples/<project_name>/firmware/am243x-evm \
```

> [!NOTE]
> If the new project is at a different directory depth than the MCU+ SDK
> example, update the `OPEN_PRU_PATH` depth accordingly. The path uses the
> same folder-depth rule as the `makefile`: count the number of directory
> levels between the core makefile and the repo root.

#### MCU+ code from the OpenPRU repo

Update:

- The include path for `imports.mak` (same folder-depth rule as above)
- Any include paths that changed between the old project and the new project,
  including the path to the PRU firmware binaries
- If filenames were bulk-renamed, verify that filenames in the core makefiles
  match the actual renamed filenames

## Build checkpoint: verify MCU+ code

> [!IMPORTANT]
> Stop here. Verify that the MCU+ code builds successfully before making any
> further changes.

```
$ cd /path/to/open-pru/examples/<project_name>

# remove previous build outputs
$ make -s clean

# build with all output visible (no -s) to catch any errors
$ make

# if no errors, clean again before continuing
$ make -s clean
```

## Customize the parent directory

Refer to
[Creating a New Project in the OpenPRU Repo](./open_pru_create_new_project.md)
§ "Customize the parent directory". The same process applies for a project with
MCU+ code.

## Update the CCS project files

This section discusses how to update the project files which are used to import
the MCU+ project from the OpenPRU repo, into the CCS workspace.

For steps to import, rename, and build a CCS MCU+ project in the CCS IDE,
refer to **the PRU Academy > Getting Started Labs > Lab 1: How to Create a PRU
Project > Creating a CCS PRU Project with MCU+ Code**.

### `example.projectspec`

When a CCS project is created or imported from an OpenPRU project, CCS reads
`example.projectspec` to configure the project.

The MCU+ core directory (`ti-arm-clang/`) must contain these files:
`makefile`, `makefile_projectspec`, `makefile_ccs_bootimage_gen`,
`example.projectspec`, `syscfg_c.rov.xs`.
The parent board+core directory must contain: `main.c`, `example.syscfg`.

#### Project name

Update the `name` attribute and the map file name to match
the new project. For example:

```
    <project
        title="<New Project Title>"
        name = "<project_name>_am243x-evm_r5fss0-0_freertos_ti-arm-clang"
...
        linkerBuildOptions="
            -m=<project_name>.${ConfigName}.map
```

Both must stay consistent with `PROJECT_NAME` in `makefile_projectspec`.

#### PRU firmware header include paths

For more information about how the PRU postBuildStep is different for makefile
builds than it is for CCS project builds, refer to
[Creating a New Project in the OpenPRU Repo](./open_pru_create_new_project.md)
§ "postBuildStep is different for makefile than for CCS".

The R5F project uses
`${WORKSPACE_LOC}/<pru_project_name>` to find the PRU-generated hex headers.
Update each include path to match the renamed PRU project names. For example:

```
        compilerBuildOptions="
...
            -I${WORKSPACE_LOC}/<project_name>_am243x-evm_icss_g0_pru0_fw_ti-pru-cgt
            -I${WORKSPACE_LOC}/<project_name>_am243x-evm_icss_g0_pru1_fw_ti-pru-cgt
```

The XML comment in
`examples/empty/mcuplus/am243x-evm/r5fss0-0_freertos/ti-arm-clang/example.projectspec`
explains the dependency:

> PRU firmware headers are generated into each PRU project's workspace folder
> by the postBuildStep. The paths in compilerBuildOptions above assume PRU
> projects are imported with their default names. If you rename a PRU project
> in CCS, update the matching `-I${WORKSPACE_LOC}/...` path above.

Remove `${WORKSPACE_LOC}` entries for any PRU cores the project does not use.

### `makefile_projectspec`

`makefile_projectspec` is used to build or export CCS projects from the
command line.

Update:

- `PROJECT_NAME` — must match the `name` attribute in `example.projectspec`
- `OPEN_PRU_PATH` — same folder-depth rule as `makefile`: if the new project
  is at a different directory depth than the template project, update the
  number of `..` levels accordingly

## Next steps

Continue to
**the PRU Academy > Getting Started Labs > Lab 2: How to Write PRU Firmware**.
