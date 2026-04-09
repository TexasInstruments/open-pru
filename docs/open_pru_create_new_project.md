<div align="center">

<img src="https://upload.wikimedia.org/wikipedia/commons/b/ba/TexasInstruments-Logo.svg" width="150"><br/>

# Creating a New Project in the OpenPRU Repo

</div>

TI supports developing PRU projects either in CCS, or in the OpenPRU repository.

This document discusses the steps to create a new PRU firmware project
in the OpenPRU repo.

For a detailed, processor-specific example of creating a new project in the
OpenPRU repo, or for documentation about how to create a PRU
project in the CCS development environment, please refer to
**the PRU Academy > Getting Started Labs > Lab 1: How to Create a PRU Project**.

Commands throughout this document will be given in the format used by the Linux
terminal, or a Windows application like Git Bash.

## Prerequisites

### Set up the repo

Before creating a new OpenPRU project, set up the OpenPRU repo by following the
steps at [Getting Started with OpenPRU](docs/getting_started.md).

### Test the make infrastructure

Before making any changes, ensure that all projects in the OpenPRU repo build
and clean successfully. Follow the instructions in the `make help` command:

```
$ cd /path/to/open-pru
$ make help

# build the projects and inspect the output
# afterwards, clean projects before copying anything
```

If the projects do not build, there is probably an issue with your `imports.mak`
file. Fix the issue before moving forward.

### Create a feature branch

Save all development work in a dedicated branch:

```
$ git branch <author>_<subject>
$ git checkout <author>_<subject>
```

Save changes often with `git add` and `git commit`. Squash multiple commits
later using `git rebase -i`.

## Copy existing project

The easiest way to create a new project in the OpenPRU repo is to copy an
existing project into a new directory. This copied project serves as a starting
point for development.

> [!NOTE]
> The "empty" projects are the default starting point for new development,
> though any existing
> OpenPRU project may be used. A PRU project from outside of the OpenPRU
> repo may also be used as a starting point, like projects from the
> [PRU Software Support Package](https://git.ti.com/cgit/pru-software-support-package/pru-software-support-package/),
> or the Beaglebone community. To create a project within OpenPRU that is based
> on code from outside of the OpenPRU repo, first create a new OpenPRU project
> from an "empty" project, and then copy specific files from the external
> project into the new OpenPRU project.

The OpenPRU repo has two "empty" starting projects:
- `examples/empty` — PRU firmware programmed in **assembly**
- `examples/empty_c` — PRU firmware programmed in **C** or **mixed C and assembly**

If using an "empty" project, select the project that matches your language. The
rest of this document is written as if development is based on `examples/empty`.

For example, copy `examples/empty` to `examples/<project_name>`.

If `<existing_project>` and `<new_project>` are in the same parent directory,
the path to imports.mak does not need be updated. However, if the folder depth
changed, the path to imports.mak needs to be updated
in all files that include imports.mak.

For example, this combination of `<existing_project>` and `<new_project>`
requires updating the imports.mak path:
* `<existing_project>`: `examples/fft/split_radix_fft_4k_single_core`
* `<new_project>`: `examples/<project_name>`

Once the path to imports.mak is correct, `<project_name>` can be built from the
project directory after updating any paths that use the project name. This is
discussed more in section
[Rename the project and update paths](#rename-the-project-and-update-paths).
```
$ cd /path/to/open-pru/examples/<project_name>
$ make -s
```

These are the only "critical" steps. You can now start development on
`<project_name>`.

The rest of this document discusses how to customize and clean up your
project. If you will create a pull request to add your project to the main
branch, you must follow ALL of the steps in this document to ensure that your
project can be maintained into the future.

## Remove or replace unused processors, boards, and cores

If the project contains processors, boards, or cores that will not be used in
your project, you should remove the unused components.

If your project uses processors, boards, or cores that were not in the original
project, you will need to add them or modify existing files. If you are using an
existing OpenPRU project as your template other than one of the "empty"
projects, and that does not have the processors, boards, or cores that your
project needs, you may get the infrastructure for those processors, boards, or
cores from the appropriate "empty" project. Just keep in mind that the steps in
section
[Rename the project and update paths](#rename-the-project-and-update-paths)
will need to be run twice: Once to update the code from the template project,
and once to update the code from the "empty" project.

> [!NOTE]
> Many PRU projects can be ported from one processor to another. However,
> sometimes a project will use features of the PRU subsystem that cannot be
> ported to other PRU subsystems. For example, PRU_ICSSG subsystems include
> accelerators and additional PRU cores that do not exist on PRU-ICSSM
> or PRUSS subsystems.
>
> We suggest checking that all the desired features exist before porting a
> project to a new processor.

Check the portability tables before porting an example to another processor:
- `examples/readme.md` § "Supported processors per-project"
- `academy/readme.md` § "Supported processors per-project"

Useful app notes:
- [PRU Subsystem Features Comparison](https://www.ti.com/lit/sprac90)
- [PRU Subsystem Migration Guide](https://www.ti.com/lit/spruid7)

These are the files & directories to modify or delete:

* `<project_name>/makefile`
* `<project_name>/<board>` or `<project_name>/<os>/<board>`
* `<project_name>/firmware/<board>`

## OpenPRU makefiles overview

The OpenPRU repo uses a hierarchical makefile structure. Each level calls the
levels below it.

| Level | File | Modifications for a new project |
|---|---|---|
| Repo root | `open-pru/makefile` | None |
| Parent directory | `open-pru/examples/makefile` | Add project to `SUBDIRS` |
| Project | `open-pru/examples/<project_name>/makefile` | Most modifications (see below) |
| PRU core | `firmware/<board>/<core>/ti-pru-cgt/makefile` | Update build outputs and paths |
| Non-PRU core | `<os>/<board>/<core>/<toolchain>/makefile` *or* `<board>/<core>/<toolchain>/makefile` | Update in a later step |

## Remove unused non-PRU code

Remove non-PRU code for cores or operating systems that will not be used in
your project. For example, if your project targets a Linux host, remove any
MCU+ directory. If your project targets an MCU+ host, remove any Linux
directory. If your project has no host code at all, remove all non-PRU code.

The non-PRU code is in a different directory than the PRU firmware. For the
directory structure, see [OpenPRU Organization](docs/open_pru_organization.md).

## Remove unused PRU code

Within the `firmware/` subdirectory, remove EVM board directories that will not
be used in your project.

After cleanup, `firmware/` should contain only the boards your project supports.

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

## Customize the makefiles

Within the project folder, there is a project makefile, and then makefiles for
each core. There is a separate core makefile for every combination of processor
and board. The project makefile calls all the other makefiles.

Every directory above the project folder also includes a makefile. This allows
us to build the project from higher level directories.

### Customize the project makefile

#### Project information

Update `PROJECT_NAME`, `SUPPORTED_PROCESSORS`, `PRU_DEPENDENCIES`, and
`NON_PRU_DEPENDENCIES`.

**`SUPPORTED_PROCESSORS`** — List the processors this project supports. When
`make` runs, it builds the target that matches `$(DEVICE)` in `imports.mak`.
If `$(DEVICE)` is not in this list, `make` prints a message and exits without
building anything.

**`PRU_DEPENDENCIES`** — List any dependencies of the PRU firmware on code
outside the OpenPRU repo. For example, add `mcuplus` here if the PRU firmware
includes headers from the MCU+ SDK.

**`NON_PRU_DEPENDENCIES`** — List the non-PRU code this project should build.
Examples:
- `mcuplus` — builds MCU+ R5F code alongside PRU firmware (AM243x, AM64x)
- `linux` — builds Linux userspace code alongside PRU firmware (AM62x, AM64x)
- If `NON_PRU_DEPENDENCIES` is empty, only PRU firmware is built

#### Target definitions

Add a target for each processor in `SUPPORTED_PROCESSORS`. Non-PRU targets use
the pattern `<device>_<host>` (for example, `am243x_mcuplus`, `am62x_linux`).

Example — MCU+ host (AM243x or AM64x):
```makefile
am243x:
	$(MAKE) -C firmware/am243x-evm/icss_g0_pru0_fw/ti-pru-cgt $(ARGUMENTS_PRU)
	$(MAKE) -C firmware/am243x-evm/icss_g0_pru1_fw/ti-pru-cgt $(ARGUMENTS_PRU)

am243x_mcuplus:
	$(MAKE) -C mcuplus/am243x-evm/r5fss0-0_freertos/ti-arm-clang $(ARGUMENTS_MCUPLUS)
```

> [!NOTE]
> Update the path to match your project's actual MCU+ directory structure.
> Some projects place MCU+ code under `mcuplus/<board>/` (e.g., `examples/empty`);
> others place it directly under `<board>/` at the project root
> (e.g., `academy/intc/intc_mcu`).

#### Prebuild checks

Top-level `make` calls every project makefile. Prebuild checks ensure a project
only builds when `imports.mak` has the correct settings.

The `DEVICE_NON_PRU` variable is built up from `NON_PRU_DEPENDENCIES`:
- If `BUILD_MCUPLUS=y` in `imports.mak` **and** `mcuplus` is in
  `NON_PRU_DEPENDENCIES`, then `$(DEVICE)_mcuplus` is added to `DEVICE_NON_PRU`.
- If `BUILD_LINUX=y` in `imports.mak` **and** `linux` is in
  `NON_PRU_DEPENDENCIES`, then `$(DEVICE)_linux` is added to `DEVICE_NON_PRU`.

The `all` target then runs:
```makefile
all: pre_build_message
	$(MAKE) pru      # builds $(DEVICE) — all PRU firmware
	$(MAKE) host     # builds each target in $(DEVICE_NON_PRU)
```

If `NON_PRU_DEPENDENCIES` is empty, `host` is a no-op and only PRU firmware is
built.

### Customize the core makefiles

Update:

* The include path for `imports.mak`
* Section **Define build outputs**

The makefile includes important information that you may need to modify later
in development, like

* Where to search for source files
* Include paths
* Compiler flags
* Linker flags
* Defines

#### Define build outputs

Update `OUTPUT_NAME` to match the new project name:

```makefile
OUTPUT_NAME := <project_name>_am243x-evm_icss_g0_pru0_fw
```

For projects with a **MCU+ host**, also update the hex array output variables:
```makefile
MCU_HEX_NAME     := pru0_load_bin.h
HEX_ARRAY_PREFIX := PRU0Firmware
MCU_HEX_PATH     := $(OPEN_PRU_PATH)/examples/<project_name>/firmware/am243x-evm/$(MCU_HEX_NAME)
```

> [!NOTE]
> If the new project is in a **different parent directory** than the source
> project, update `MCU_HEX_PATH` accordingly — the relative depth to
> `$(OPEN_PRU_PATH)` will have changed.

For projects with a **Linux host** (e.g., AM62x) or **no host at all**
(standalone PRU), `MCU_HEX_PATH` is not used by any host project. Only
`OUTPUT_NAME` requires updating.

> [!NOTE]
> For projects without an MCU+ host, leave `postBuildStep` in
> `example.projectspec` empty or omit it. The `postBuildStep` is only needed
> when an MCU+ project must consume the generated hex array header from the
> CCS workspace.

## Build checkpoint: verify PRU firmware

> [!IMPORTANT]
> Stop here. Verify that the PRU firmware builds successfully before making any
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

Add `<project_name>` to the makefile in the parent directory. That allows the
project to build from a top-level make command.

### Check the top level makefile

You already verified that the PRU code builds from the project directory in the
**Build checkpoint** section. Now, check that the project make builds from the
top-level directory.

```
$ cd /path/to/open-pru
$ make -s clean
$ make -s
```

## Update the CCS project files

This section discusses how to update the project files which are used to import
the PRU project from the OpenPRU repo, into the CCS workspace.

For steps to import, rename, and build a CCS PRU project in the CCS
IDE, refer to **the PRU Academy > Getting Started Labs > Lab 1: How to Create a
PRU Project > Creating a CCS PRU Project**.

### `example.projectspec`

When a CCS project is created or imported from an OpenPRU project, CCS reads
`example.projectspec` to configure the project.

#### Different example.projectspec templates for ASM vs C

PRU projects which are written only in assembly code require different linker
flags than PRU projects that include C code. Refer to the comment at the top
of the example.projectspec file for guidance on whether that file uses the
template for assembly code, or C / mixed C and assembly code:

```
<?xml version="1.0" encoding="UTF-8"?>
<!-- This example.projectspec file is for PRU projects written in assembly -->
```
or
```
<?xml version="1.0" encoding="UTF-8"?>
<!-- This example.projectspec file is for PRU projects written in C, or mixed C and assembly -->
```

Choose the flags that match your project:

| Flag | ASM (`examples/empty`) | C (`examples/empty_c`) |
|---|---|---|
| `--disable_auto_rts` | yes | no |
| `--entry_point=main` | yes | no |
| `--stack_size`, `--heap_size` | no | yes |
| `-i${CG_TOOL_ROOT}/lib`, `-i${CG_TOOL_ROOT}/include` | no | yes |
| `--library=libc.a` | no | yes |

Compiler build options are identical between ASM and C projects.

#### Project name

Update the `name` attribute and the map file name to match
the new project. For example:

```
    <project
        title="<New Project Title>"
        name = "<project_name>_am243x-evm_icss_g0_pru0_fw_ti-pru-cgt"
...
        linkerBuildOptions="
            -m=<project_name>.${ConfigName}.map
```

Both must stay consistent with `PROJECT_NAME` in `makefile_projectspec`.

#### postBuildStep is different for makefile than for CCS

The `postBuildStep` is used to generate a PRU hex array binary file, and then
write that binary file to a known location. If the project uses an MCU+ core,
the MCU+ project then takes the hex array file from that known location.

> [!NOTE]
> In `pru_rules.mak`, `post-build` writes the hex array file to a location
> within the OpenPRU repo, called TARGET_HEX. On the
> other hand, in `example.projectspec`, the hex array file is written to a
> location within the CCS workspace. `postBuildStep` writes the hex array file
> to the root directory of the PRU project, one level above the
> `Debug`/`Release` build output directory.

Example `postBuildStep` in PRU example.projectspec:

```
        postBuildStep="
            $(CG_TOOL_ROOT)/bin/hexpru --diag_wrap=off --array --array:name_prefix=PRU0Firmware -o ../pru0_load_bin.h ${BuildArtifactFileBaseName}.out;
        "
```

The MCU+ project's `example.projectspec` must include the CCS workspace path
to the PRU hex arrays, like this:

```
        compilerBuildOptions="
...
            -I${WORKSPACE_LOC}/<project_name>_am243x-evm_icss_g0_pru0_fw_ti-pru-cgt
            -I${WORKSPACE_LOC}/<project_name>_am243x-evm_icss_g0_pru1_fw_ti-pru-cgt
```

We discuss more about creating an MCU+ project in the OpenPRU repo in
[Adding MCU+ Code to a New Project in the OpenPRU Repo](docs/open_pru_create_new_mcuplus_project.md).

#### Device-specific include path

If the device changed, update:

```
-I${OPEN_PRU_PATH}/source/include/<device>
```

### `makefile_projectspec`

`makefile_projectspec` is used to build or export CCS projects from the
command line.

Update:

- `PROJECT_NAME` — must match the `name` attribute in `example.projectspec`
- `OPEN_PRU_PATH` — same folder-depth rule as `makefile`: if the new project
  is at a different directory depth than the template project, update the
  number of `..` levels accordingly

## Next steps

- Will the PRU be controlled by a core running MCU+ SDK? Continue to
  [Adding MCU+ Code to a New Project in the OpenPRU Repo](docs/open_pru_create_new_mcuplus_project.md).
- Are you done creating your PRU project? Continue to
  **the PRU Academy > Getting Started Labs > Lab 2: How to Write PRU Firmware**.
