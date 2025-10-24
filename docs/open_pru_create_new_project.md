<div align="center">

<img src="https://upload.wikimedia.org/wikipedia/commons/b/ba/TexasInstruments-Logo.svg" width="150"><br/>

# Creating a New Project in the OpenPRU Repo

</div>

This document provides high-level steps that apply to all processors. For more
details, and a processor-specific example, please refer to
**the PRU Academy > Getting Started Labs > Lab 1: How to Create a PRU Project**.

This document is focused on creating a starting point to develop PRU code.
Information about adding MCU+ code to an OpenPRU project is covered in
**the PRU Academy > Getting Started Labs > Lab 1: How to Create a PRU Project >
Creating an OpenPRU Project with MCU+ Code**.

Commands throughout this document will be given in the format used by the Linux
terminal, or a Windows application like Git Bash.

## Copy existing project

The easiest way to create a new project in the OpenPRU repo is to copy an
existing project into a new directory, or import an existing project into CCS.
This copied project will serve as a starting point.

For example, copy `examples/empty` to `examples/my_project`.

Since `empty` and `my_project` are in the same parent directory, `my_project`
can be built from the project directory after updating any paths that use the
project name. This is discussed more in section
[Rename the project and update paths](#rename-the-project-and-update-paths).
```
$ cd /path/to/open-pru/examples/my_project
$ make -s
```

This is the only "critical" step. You can now start development on `my_project`.

The rest of this document discusses how to customize & clean up your
project. If you plan to create a pull request to add your project to the main
branch, please follow ALL of the following steps to make sure that your project
can be maintained into the future.

## Remove or replace unused processors, boards, and cores

If the project contains processors, boards, or cores that will not be used in
your project, you should remove the unused components.

If your project uses processors, boards, or cores that were not in the original
project, you will need to add them or modify existing files.

> [!NOTE]
> Many PRU projects can be ported from one processor to another. However,
> sometimes a project will use features of the PRU subsystem that cannot be
> ported to other PRU subsystems. For example, PRU_ICSSG subsystems include
> accelerators and additional PRU cores that do not exist on PRU-ICSSM
> subsystems.
>
> We suggest checking that all the desired features exist before porting a
> project to a new processor. This app note is a good starting point:
> [PRU Subsystem Features Comparison](https://www.ti.com/lit/sprac90).

These are the files & directories to modify or delete:

* `my_project/makefile`
* `my_project/[boards]` or `my_project/[os]/[boards]`
* `my_project/firmware/[boards]`

## Rename the project and update paths

Find and replace the old project name and path with the new project name and
path throughout the project.

## Customize the makefiles

Within the project folder, there is a project makefile, and then makefiles for
each core. There is a separate core makefile for every combination of processor
and board. The project makefile calls all the other makefiles.

Every directory above the project folder also includes a makefile. This allows
us to build the project from higher level directories.

### Customize the project makefile

Update:

* The include path for `imports.mak`
* **Project information**
* **Prebuild checks** (if any dependencies are added)
* **Target definitions** (if not already updated)

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

### Customize the parent directory

Add my_project to the makefile in the parent directory. That allows the
project to build from a top-level make command.

#### Check the top level makefile

First, make sure that the project make file builds from the project directory.
Then, check that the project make builds from the top-level directory.

```
$ cd /path/to/open-pru/examples/my_project

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
