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

Your project should build from a top-level make command.

For details about how to set up the makefile infrastructure for your project,
refer to
[Creating a New Project in the OPEN PRU Repo](./open_pru_create_new_project.md) 

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
