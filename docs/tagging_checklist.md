# Tagging checklist

This document is a checklist for the open-pru repo maintainers when they are
tagging a new version of the open-pru repo. Most users can ignore this document.

[Test SDK releases](#test-sdk-releases)  
[Update the year](#update-the-year)  
[Update release notes](#update-release-notes)  

## Test SDK releases

test which SDK releases build for each part.

> [!NOTE]
> Linux code builds must be tested on a Linux computer.
> If you are testing on a Windows computer, please get a team member with a
> Linux computer to test that the Linux code builds.

### Set up prerequisites

Make sure that all build prerequisites are installed in the system, as discussed
in docs/getting_started.md.

In imports.mak:

* set BUILD_MCUPLUS?=y
* set BUILD_LINUX?=y (Linux PC only)

### Test MCU+ processors and supported SDK versions

#### Steps to test

imports.mak will be modified before each test.

Test all of DEVICE ?=
am243x, am261x, am263px, am263x, am64x

For each SDK release, update these imports.mak entries:
* MCU_PLUS_SDK_PATH?=
* SYSCFG_PATH ?=

After modifying imports.mak, from the top of the open-pru directory:
```
// build all projects with makefile
$ make -s
// did all projects successfully build?
$ make -s clean
// modify imports.mak for the next test
```

#### Example

For example, to validate tag v2025.00.00:

For am243x & am64x, tested
* MCU+ SDK 10.0 (failed)
* MCU+ SDK 10.1 (success)
* MCU+ SDK 11.0 (success)
* MCU+ SDK 11.1 (failed)

and so on.

It is important to test each processor and SDK combination. For v2025.00.00,
am263x supported different SDK versions than am263px & am261x.

### Test Linux processors and supported SDK versions

imports.mak will be modified before each test.

Test all of DEVICE ?=
am62x, am64x

TODO: Fill in the rest of this section after adding Linux support.

## Update the year

Update the year in:

source/firmware/pru_load_bin_copyright.h

## Update release notes

### docs/release_notes.md

Update with information like compatible SDK releases.

### the tag content

Most of the release information should go here. Details like:
* new features added
* new examples/projects added
* major bugfixes
