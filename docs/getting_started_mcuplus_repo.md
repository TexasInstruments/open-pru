<div align="center">

<img src="https://upload.wikimedia.org/wikipedia/commons/b/ba/TexasInstruments-Logo.svg" width="150"><br/>

# Getting Started with the MCU+ SDK Repo

</div>

[Install the MCU+ SDK](#install-the-mcu-sdk)  
[Install the tools](#install-the-tools)  
[Set up imports.mak](#set-up-importsmak)  
[Update the CCS files](#update-the-ccs-files)  

## Install the MCU+ SDK

Users can either install the prebuilt MCU+ SDK, or clone the MCU+ SDK
repository. In general, we suggest using the prebuilt MCU+ SDK:
   * The prebuilt SDK is packaged specifically for a single device, so it takes
     up less hard drive space than the MCU+ SDK repo
   * The CCS files in the OpenPRU project need modification to work with the
     MCU+ SDK repository

This file is for users who decide to clone the MCU+ SDK repository. For
documentation around using the prebuilt MCU+ SDK, refer to
[Getting Started with MCU+ SDK](./getting_started_mcuplus.md).

**The MCU+ SDK repository**
can be cloned from
[here](https://github.com/TexasInstruments/mcupsdk-core). Follow the
mcupsdk-core README.md file for additional steps to set up the repository.

## Install the tools

> [!NOTE]
> MCU+ SDK repository users:
> * Refer to the documentation associated with the latest version of the MCU+ SDK

Install the versions of the tools that are listed in these pages:
   - MCU+ SDK docs > Getting Started > Download, Install, and Setup SDK and Tools
   - MCU+ SDK docs > Getting Started > Download, Install, and Setup CCS
     - Developers using a Linux computer, make sure to follow the additional steps at "CCS Linux Host Support"

## Set up imports.mak

The imports.mak file contains the information that the OpenPRU makefiles need
in order to build on your computer.

### Copy the default file

Copy `open-pru/imports.mak.default` into a new file, `open-pru/imports.mak`.

For ease of use, the new imports.mak file is already excluded from git tracking.

### Customize imports.mak for your computer

Open imports.mak and update the settings based on your specific computer. Follow
the `TODO` comments to see which settings need to be modified.

## Update the CCS files

Projects in the OpenPRU repo assume that the user is using a processor-specific
SDK. Processor-specific SDKs have a specific name in CCS. However, the MCU+
SDK repo has a different name. Before CCS can open a project and build it with
the MCU+ SDK repo, the project's example.projectspec files must be updated.

### Updating the CCS files in an existing project

Update the "products" and "pathVariable" entries to reflect the MCU+ github
repo. Please note that if you want to push a project to the OpenPRU repo for
others to use, you will need to revert the "products" and "pathVariable" entries
back to the processor-specific SDK settings.

The "products" entry is different for different processors. Check the
example.projectspec files for the processors & boards that you care about.

```
AM243x example:
$ vi examples/empty/firmware/am243x-lp/icss_g0_pru0_fw/ti-pru-cgt/example.projectspec
products="com.ti.OPEN_PRU;com.ti.MCU_PLUS_SDK_AM243X"
<pathVariable name="MCU_PLUS_SDK_PATH" path="${COM_TI_MCU_PLUS_SDK_AM243X_INSTALL_DIR}" scope="project" />

AM261x example:
$ vi examples/empty/firmware/am261x-lp/icss_m0_pru0_fw/ti-pru-cgt/example.projectspec
products="com.ti.OPEN_PRU;MCU-PLUS-SDK-AM261X"
<pathVariable name="MCU_PLUS_SDK_PATH" path="${COM_TI_MCU_PLUS_SDK_AM261X_INSTALL_DIR}" scope="project" />
```

We want to replace these variables with the MCU+ SDK repo values:
```
products="com.ti.OPEN_PRU;com.ti.MCU_PLUS_SDK_AMXXX"
<pathVariable name="MCU_PLUS_SDK_PATH" path="${COM_TI_MCU_PLUS_SDK_AMXXX_INSTALL_DIR}" scope="project" />
```

These changes can be made manually, or with automated commands. For example:

```
$ cd /path/to/open-pru/examples/my_project

// let's say we are creating firmware for AM243x
// get the existing variable names that you want to modify
$ vi examples/empty/firmware/am243x-lp/icss_g0_pru0_fw/ti-pru-cgt/example.projectspec

// double-check that grep returns all the files you want to modify
$ grep -rli --include=*.projectspec 'com.ti.MCU_PLUS_SDK_AM243X'

// did that return all the expected files? If so, replace the string
$ grep -rli --include=*.projectspec 'com.ti.MCU_PLUS_SDK_AM243X' | xargs -i@ sed -i 's/com.ti.MCU_PLUS_SDK_AM243X/com.ti.MCU_PLUS_SDK_AMXXX/g' @

// now do the same thing with the pathVariable entry
$ grep -rli --include=*.projectspec 'COM_TI_MCU_PLUS_SDK_AM243X_INSTALL_DIR'

// did that return all the expected files? If so, replace the string
$ grep -rli --include=*.projectspec 'COM_TI_MCU_PLUS_SDK_AM243X_INSTALL_DIR' | xargs -i@ sed -i 's/COM_TI_MCU_PLUS_SDK_AM243X_INSTALL_DIR/COM_TI_MCU_PLUS_SDK_AMXXX_INSTALL_DIR/g' @
```

### Updating the CCS files in a new project

The steps from section
[Updating the CCS files in an existing project](#updating-the-ccs-files-in-an-existing-project)
still apply.

For additional steps to create a new OpenPRU project, refer to page
[Creating a New Project in the OpenPRU Repo](./open_pru_create_new_project.md).

## Next steps

After the OpenPRU repository has been set up, refer back to the
[README](./../README.md) for build steps.