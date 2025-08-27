<div align="center">

<img src="https://upload.wikimedia.org/wikipedia/commons/b/ba/TexasInstruments-Logo.svg" width="150"><br/>
# Contributing to the OPEN PRU project

</div>

[Coding guidelines](#coding-guidelines)  
[Add documentation](#add-documentation)  
[Add build infrastructure](#add-build-infrastructure):
[Make infrastructure](#make-infrastructure),
[CCS build infrastructure](#ccs-build-infrastructure)  
[Rebase on top of main](#rebase-on-top-of-main)  

## Introduction

Ready to contribute back to the OPEN PRU project? This page provides a checklist
to simplify the contribution process.

## Coding guidelines

When adding new code, please make sure that the code follows the
[best practices](../best_practices.md).

## Add documentation

A project's README file should include all information that another programmer
would need to run the example:

* An overview of what the project does

* A list of the specific hardware used to test the code
    * include details like board revision number

* The exact version of all SDKs and tools where the project was validated on hardware
    * (for example, AM64x MCU+ SDK 11.0, CCS v12.8.1)

* All steps a programmer needs to follow in order to run the project and
  validate the outputs

## Add build infrastructure

When adding a new project, or when porting an existing project to a new device,
please ensure that the project builds from both the command line, and in CCS.

Many PRU projects involve interacting with peripherals outside of the
processor. That means that specific peripherals and boards may be required to
validate a project on hardware. This has a couple of impacts:

* TI will rely on the project author to thoroughly validate their code. We are
  unable to manually validate all pull requests on hardware
    
* Every time the OPEN PRU repository updates to a new version of SDK,
  TI does not re-validate all projects on hardware. Instead, TI uses a
  top-level build to check that all projects continue to compile. This is
  why it is so important to document the hardware-validated
  software versions in the project README. This provides a "known good"
  starting point for other developers if your project breaks in a future
  software release

### Make infrastructure

Your project should build from a top-level make command. Like this:

```
$ cd /path/to/open-pru
$ make -s
```

So what makefile infrastructure do we need in order to build?

Let's say we are adding an example with MCU+ code to examples/new_project. This
example runs on AM243x and AM64x.

#### Which files need to be modified?

We can use the empty example as a template.

open-pru/makefile
* "make" is the same as "make all". The "make all" command passes a "make"
  command to all subfolders listed in SUBDIRS
* no modifications needed

open-pru/examples/makefile
* "make all" passes a "make" command to all subfolders listed in SUBDIRS
* We need to add new_project to this file
* If this project only had PRU code, then we would list the project under
  SUBDIRS :=
* However, this project also has MCU+ code. Add new_project to
  SUBDIRS_MCUPLUS := .
  Now a top-level make command will only build the project if BUILD_MCUPLUS = y
  in imports.mak

open-pru/examples/new_project/makefile
* Add a makefile in this directory. Let's start by copying the file
  examples/empty/makefile
* We will modify this file in the next section

Finally, there should be a makefile for every processor that is getting
programmed in the project. Typical locations for these makefiles include:

* open-pru/examples/new_project/firmware/board/pru-core/build-tool/makefile
* open-pru/examples/new_project/board/non-pru-core/build-tool/makefile

#### Modifying the project makefile

Now let's modify open-pru/examples/new_project/makefile.

Update the "project information" section:
* PROJECT_NAME := new_project
* SUPPORTED_PROCESSORS := am243x am64x

"make all" builds the target that matches $(DEVICE) in imports.mak. So:
* "make" with $(DEVICE) = am261x does nothing
* "make" with $(DEVICE) = am243x runs all commands under the target called
  am243x:
* etc

First, remove all targets that do not apply for this project. Delete
sections "am261x:", "am263px:", etc.

Next, make sure that the $(MAKE) commands under am243x: and am64x: have actual
targets. Let's say that we are only building for ICSSG0_PRU0, ICSSG1_PRU0, and
R5F0_0 on the EVM boards. Then we would update the makefile like this:

```
am243x:
# am243x-evm
        $(MAKE) -C firmware/am243x-evm/icss_g0_pru0_fw/ti-pru-cgt $(ARGUMENTS)
        $(MAKE) -C firmware/am243x-evm/icss_g1_pru0_fw/ti-pru-cgt $(ARGUMENTS)
        $(MAKE) -C am243x-evm/r5fss0-0_freertos/ti-arm-clang $(ARGUMENTS)

am64x:
# am64x-evm
        $(MAKE) -C firmware/am64x-evm/icss_g0_pru0_fw/ti-pru-cgt $(ARGUMENTS)
        $(MAKE) -C firmware/am64x-evm/icss_g1_pru0_fw/ti-pru-cgt $(ARGUMENTS)
        $(MAKE) -C am64x-evm/r5fss0-0_freertos/ti-arm-clang $(ARGUMENTS)
```

#### Testing the make infrastructure

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

### CCS Build infrastructure

Please use the processor-specific SDK to create the CCS project files, instead
of the mcupsdk-core github project.

More guidelines will be added later.

## Rebase on top of main

The main branch might have been updated during your development. Rebase on top
of the main branch before creating the pull request, and resolve any conflicts.
Like this:

```
$ git pull --rebase origin main
```
