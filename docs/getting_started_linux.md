<div align="center">

<img src="https://upload.wikimedia.org/wikipedia/commons/b/ba/TexasInstruments-Logo.svg" width="150"><br/>

# Getting Started with Linux SDK

</div>

[Install the Linux SDK](#install-the-linux-sdk)  
[Install the tools](#install-the-tools)  
[Set up imports.mak](#set-up-importsmak)  

The Linux SDK can only be installed on a Linux computer, not a Windows computer.

### Install the Linux SDK

The Linux SDK is only a dependency if building code that will run on a Linux A53 core.

**The Linux SDK** installer can be downloaded here:
   - [AM62x Linux SDK](https://www.ti.com/tool/download/PROCESSOR-SDK-LINUX-AM62X)
   - [AM64x Linux SDK](https://www.ti.com/tool/download/PROCESSOR-SDK-LINUX-AM64X)

### Install the tools

   1. Download and install Code Composer Studio v12.8 or v20.x from [here](https://www.ti.com/tool/download/CCSTUDIO "Code Composer Studio")
         - Please install these dependencies before installing CCS on a Linux computer. Use the command
         ```bash
         $ sudo apt -y install libc6:i386 libusb-0.1-4 libgconf-2-4 libncurses5 libpython2.7 libtinfo5 build-essential
         ```

   2. Download and install the PRU compiler
      - [PRU-CGT-2-3](https://www.ti.com/tool/PRU-CGT) (ti-pru-cgt)

   3. Download and install GCC for Cortex A53
      - A53: [GNU-A](https://developer.arm.com/-/media/Files/downloads/gnu-a/9.2-2019.12/binrel/gcc-arm-9.2-2019.12-x86_64-aarch64-none-elf.tar.xz)


## Set up imports.mak

The imports.mak file contains the information that the OPEN PRU makefiles need
in order to build on your computer.

### Copy the default file

Copy `open-pru/imports.mak.default` into a new file, `open-pru/imports.mak`.

For ease of use, the new imports.mak file is excluded from git tracking.

### Customize imports.mak for your computer

Open imports.mak and update the settings based on your specific computer. Follow
the `TODO` comments to see which settings need to be modified.

## Next steps

After the OPEN PRU repository has been set up, refer back to the
[README](./../README.md) for build steps.