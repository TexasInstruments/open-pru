# The PRU Dependencies Scripts

## Introduction

> [!WARNING]
> The "pru_dependencies" scripts are not recommended for use. We suggest
> manually installing the dependencies for the open-pru repo by following the
> [Getting Started with OPEN PRU](./getting_started.md) documentation.

> [!NOTE]
> The "pru_dependencies" scripts currently only support installing MCU+ SDK 11.0
> dependencies for AM243x & AM64x. Modifying the scripts to install anything else
> is more effort than manually installing dependencies as documented in
> the [Getting Started with OPEN PRU](./getting_started.md) documentation.

## Install dependencies

The pru_dependencies scripts currently support installing MCU+ SDK and other
dependencies on **AM243x & AM64x**. For all other devices, or for steps when
installing the Linux SDK, please follow the steps at
[Getting Started with OPEN PRU](./getting_started.md).

> [!NOTE]
> Different releases of the open-pru repo are compatible with different SDK
> versions. Please refer to the [release notes](./release_notes.md) for more
> information.

> [!NOTE]
> In general, software dependencies should all be installed under the same
> folder. So Windows developers would put all software tools under C:\ti, and
> Linux developers would put all software tools under ${HOME}/ti.

If the OPEN PRU repo is installed on a Windows machine, follow the steps in
[Windows: Use script `pru_dependencies.bat` to install SDK & tools](#windows-use-script-pru_dependenciesbat-to-install-sdk--tools).

If the OPEN PRU repo is installed on a Linux machine, follow the steps in
[Linux: Use script `pru_dependencies.sh` to install SDK & tools](#linux-use-script-pru_dependenciessh-to-install-sdk--tools).

### Windows: Use script `pru_dependencies.bat` to install SDK & tools

**NOTES**

   - If the script is executed from any folder but `C:\ti`, then the script will
     be copied to `C:\ti` after running. Then a second terminal screen will open
     with the location of the script. Please re-run script in the new terminal
     screen
   - If OpenSSL is needed to be installed, when prompted select the option to
     install binaries to /bin folder of installed path instead of the Windows
     system path
   - After installing dependencies, if the script is executed again in the same
     terminal to verify the installation, the script will show the same missing
     dependencies as the first run. This is a Windows limitation. Please close
     and re-open the terminal in order to get the updated state.

#### Install the MCU+ SDK

By default, the script checks for and installs MCU+ SDK 11.0. In order to use a
different SDK version, edit the **version numbers** in pru_dependencies.bat to
match the desired SDK version from:
   - [AM243x MCU+ SDK](https://www.ti.com/tool/download/MCU-PLUS-SDK-AM243X)
   - [AM64x MCU+ SDK](https://www.ti.com/tool/download/MCU-PLUS-SDK-AM64X)

Run the pru_dependencies script like this:
   - AM243x: `pru_dependencies.bat install --am243x_sdk`
   - AM64x: `pru_dependencies.bat install --am64xx_sdk`

Alternatively, the script can be used to clone the MCU+ SDK repository:
   - `pru_dependencies.bat install --clone_sdk`

#### Install the tools

By default, the script checks for and installs the tool versions used with
MCU+ SDK 11.0. If using a different SDK version, edit the **version numbers**
in pru_dependencies.bat to match the versions listed in **the MCU+ SDK docs**,
sections
   - Getting Started > Download, Install, and Setup SDK and Tools
   - Getting Started > Download, Install, and Setup CCS

   1. Verify the dependencies that are already installed:
       - `pru_dependencies.bat verify`

   2. Install dependencies:
       - `pru_dependencies.bat install [dependencies]`

For assistance on how to use the script, run the command `pru_dependencies.bat help`.

After installing all dependencies, continue to section [Set up imports.mak](#set-up-importsmak).

### Linux: Use script `pru_dependencies.sh` to install SDK & tools

**NOTES**

   - If the script is executed from any folder but `${HOME}/ti`, then the script
     will be copied to `${HOME}/ti`. The script will execute from there automatically

#### Install the MCU+ SDK

By default, the script checks for and installs MCU+ SDK 11.0. In order to use a
different SDK version, edit the **version numbers** in pru_dependencies.sh to
match the desired SDK version from:
   - [AM243x MCU+ SDK](https://www.ti.com/tool/download/MCU-PLUS-SDK-AM243X)
   - [AM64x MCU+ SDK](https://www.ti.com/tool/download/MCU-PLUS-SDK-AM64X)

Run the pru_dependencies script like this:
   - AM243x: `./pru_dependencies.sh install --am243x_sdk`
   - AM64x: `./pru_dependencies.sh install --am64xx_sdk`

Alternatively, the script can be used to clone the MCU+ SDK repository:
   - `./pru_dependencies.sh install --clone_sdk`

#### Install the tools

By default, the script checks for and installs the tool versions used with
MCU+ SDK 11.0. If using a different SDK version, edit the **version numbers**
in pru_dependencies.sh to match the versions listed in **the MCU+ SDK docs**,
sections
   - Getting Started > Download, Install, and Setup SDK and Tools
   - Getting Started > Download, Install, and Setup CCS

   1. Verify the dependencies that are already installed:
       - `./pru_dependencies.sh verify`

   2. Install dependencies:
       - `./pru_dependencies.sh install [dependencies]`

For assistance on how to use the script, run the command `./pru_dependencies.sh help`.

After installing all dependencies, continue to "Getting Started with OPEN PRU"
section [Set up imports.mak](./getting_started.md#set-up-importsmak).