#!/bin/bash
# ===========================================================================
# File: pru_dependencies.sh
# Description: Installation script for TI tools
# ---------------------------------------------------------------------------

# Set version numbers for TI tools
HOME_TI=~/ti
CCS_VERSION=12.8.1
CCS_VERSION_WEB=$CCS_VERSION.00005
CCS_PATH_VERSION=$(echo $CCS_VERSION | tr -d '._')
GCC_AARCH64_VERSION=9.2-2019.12
GCC_ARM_VERSION=7-2017-q4-major
GCC_ARM_VERSION_FOLDER=7-2017q4
PRU_VERSION=2.3.3

function verify_dependencies() {
    echo "##### Python #####"
    which python3 > nul 2>&1 && (echo found) || (echo Please install Python using the command pru_dependencies.sh -I --python)
    echo "##### pip #####"
    which pip > nul 2>&1 && (echo found) || (echo Please install pip using the command pru_dependencies.sh -I --python)
    echo "##### west #####"
    which west > nul 2>&1 && (echo found) || (echo Please install west using the command pru_dependencies.sh -I --python)
    echo "##### Node.js #####"
    node --version > nul 2>&1 && (echo found) || (echo Please install Node.js using the command pru_dependencies.sh -I --node)
    echo "##### OpenSSL #####"
    which openssl > nul 2>&1 && (echo found) || (echo Please install OpenSSL using the command pru_dependencies.sh -I --openssl)
    echo "##### Doxygen #####"
    which doxygen > nul 2>&1 && (echo found) || (echo Please install Doxygen using the command pru_dependencies.sh -I --doxygen)
    echo "##### Mono Runtime #####"
    which mono > nul 2>&1 && (echo found) || (echo Please install Mono Runtime using the command pru_dependencies.sh -I --mono)
    echo "##### Make #####"
    which make > nul 2>&1 && (echo found) || (echo Please install make using the command pru_dependencies.sh -I --make)
    echo "##### MCU+ SDK #####"
    if [ -d $HOME_TI/mcu_plus_sdk ]; then
        echo MCU+ SDK already cloned.
    else
        echo MCU+ SDK not found in system.
        echo Please install MCU+ SDK using the command pru_dependencies.sh -I --clone_sdk
    fi
    if [ -d $HOME_TI/mcu_plus_sdk_am243x_$MCU_SDK_VERSION ]; then
        echo Latest version of MCU+ SDK for AM243x already installed.
    else
        echo MCU+ SDK for AM243x not found in system.
        echo Please install MCU+ SDK using the command pru_dependencies.sh -I --am243x_sdk
    fi
    if [ -d $HOME_TI/mcu_plus_sdk_am64x_$MCU_SDK_VERSION ]; then
        echo Latest version of MCU+ SDK for AM64xx already installed.
    else
        echo MCU+ SDK for AM64xx not found in system.
        echo Please install MCU+ SDK using the command pru_dependencies.sh -I --am243x_sdk
    fi
    echo "##### Code Composer Studio #####"
    if [ -d $"$HOME_TI/ccs$CCS_PATH_VERSION" ]; then
        echo CCS already installed.
    else
        echo CCS was not found inside $HOME_TI. 
        echo Please install CCS using the command pru_dependencies.sh -I --ccs
    fi
    echo "##### SysConfig #####"
    if [ -d $"$HOME_TI/sysconfig_$SYSCONFIG_PATH" ]; then
        echo SysConfig already installed.
    else
        echo SysConfig was not found inside $HOME_TI.
        echo Please install SysConfig using the command pru_dependencies.sh -I --sysconfig
    fi
    echo "##### TI ARM Clang #####"
    if [ -d $"$HOME_TI/ti-cgt-armllvm_$TI_ARM_VERSION" ]; then
        echo TI ARM Clang already installed.
    else
        echo TI ARM Clang was not found inside $HOME_TI. 
        echo Please install TI ARM Clang using the command pru_dependencies.sh -I --ti_clang
    fi
    echo "##### GCC for Cortex A53 #####"
    if [ -d $"$HOME_TI/gcc-arm-${GCC_AARCH64_VERSION}-x86_64-aarch64-none-elf" ]; then
        echo GCC for Cortex A53 already installed.
    else
        echo GCC for Cortex A53 was not found inside $HOME_TI.
        echo Please install the compiler using the command pru_dependencies.sh -I --gcc_a53
    fi
    echo "##### GCC for Cortex R5 #####"
    if [ -d $"$HOME_TI/gcc-arm-none-eabi-$GCC_ARM_VERSION" ]; then
        echo GCC for ARM Cortex R5 already installed.
    else
        echo GCC for ARM Cortex R5 was not found inside $HOME_TI.
        echo Please install the compiler using the command pru_dependencies.sh -I --gcc_r5
    fi
    echo "##### PRU-CGT #####"
    if [ -d $"$HOME_TI/ti-cgt-pru_$PRU_VERSION" ]; then
        echo PRU CGT already installed.
    else
        echo PRU-CGT not found inside $HOME_TI.
        echo Please install PRU-CGT using the command pru_dependencies.sh -I --pru
    fi

    rm -r nul
}

function list_sdkVersion() {
    SOC=$1  # Ex: AM243X, AM64X, AM263X, AM263PX, AM261X

    TOOL_URL="https://www.ti.com/tool/download/MCU-PLUS-SDK-${SOC}"
    URL_BASE="https://dr-download.ti.com/software-development/software-development-kit-sdk/MD-ouHbHEm1PK"

    echo "Searching for available versions of MCU-PLUS-SDK-${SOC}..."
    SDK_VERSIONS=($(wget -qO- "$TOOL_URL" \
          | grep -Eo "MCU-PLUS-SDK-${SOC}/[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+" \
          | sed "s#MCU-PLUS-SDK-${SOC}/##" \
          | sort -rV | uniq))

    if [ ${#SDK_VERSIONS[@]} -eq 0 ]; then
        echo "No available versions found for $SOC. Please check if the name is correct."
        return 1
    fi

    echo ""
    echo "Available versions for $SOC:"
    for i in "${!SDK_VERSIONS[@]}"; do
        printf "%d) %s\n" "$((i+1))" "${SDK_VERSIONS[$i]}"
    done

    echo ""
    read -p "Enter the number of the version you want to download: " choice
    if ! [[ "$choice" =~ ^[0-9]+$ ]] || [ "$choice" -lt 1 ] || [ "$choice" -gt "${#SDK_VERSIONS[@]}" ]; then
        echo "Invalid choice. Exiting."
        return 1
    fi

    SDK_VERSION="${SDK_VERSIONS[$((choice-1))]}"
    VERSION_URL=$(echo "$SDK_VERSION" | sed 's/\./_/g')

    if [[ "$SDK_VERSION" == 08.* ]] && [[ "$SOC" == "AM243X" || "$SOC" == "AM64X" ]]; then
        echo ""
        echo "ATTENTION: Version $SDK_VERSION cannot be downloaded automatically."
        echo "It requires login on the TI website for secure download."
        echo "Please access the link below manually to download:"
        echo "${TOOL_URL}/${SDK_VERSION}"
        return 0
    fi

    DOWNLOAD_LINK="$URL_BASE/${SDK_VERSION}/mcu_plus_sdk_${SOC,,}_${VERSION_URL}-linux-x64-installer.run"

    echo ""
    echo "Selected version: $SDK_VERSION"
    echo "Starting download: $DOWNLOAD_LINK"

    wget --show-progress -c "$DOWNLOAD_LINK"
}

function install_sysconfigLatest() {
    SYSCONFIG_URL="https://www.ti.com/tool/download/SYSCONFIG"
    SYSCONFIG_BASE="https://dr-download.ti.com/software-development/ide-configuration-compiler-or-debugger/MD-nsUM6f7Vvb"
    
    # Get the latest version
    SYSCONFIG_VERSION=$(wget -qO- "$SYSCONFIG_URL" \
        | grep -Eo 'SYSCONFIG/[0-9]+\.[0-9]+\.[0-9]+.[0-9]+' \
        | sed 's#SYSCONFIG/##' \
        | sort -rV | head -n 1)

    echo "Latest version found: $SYSCONFIG_VERSION"

    # The product page has the real download link (dr-download)
    VERSION_URL=$(echo "$SYSCONFIG_VERSION" | sed 's/\.\([0-9]\+\)$/_\1/')
    DOWNLOAD_LINK="$SYSCONFIG_BASE/${SYSCONFIG_VERSION}/sysconfig-${VERSION_URL}-setup.run"

    echo "Download link detected:"
    echo "$DOWNLOAD_LINK"

    wget --show-progress -c "$DOWNLOAD_LINK"

    chmod +x SysConfig*.run && ./SysConfig*.run
}

function install_ClangLatest() {
    CLANG_URL="https://www.ti.com/tool/download/ARM-CGT-CLANG"
    CLANG_BASE="https://dr-download.ti.com/software-development/ide-configuration-compiler-or-debugger/MD-ayxs93eZNN"

    # Get the latest version
    CLANG_VERSION=$(wget -qO- "$CLANG_URL" \
        | grep -Eo 'CLANG/[0-9]+\.[0-9]+\.[0-9]+.LTS+' \
        | sed 's#CLANG/##' \
        | sort -rV | head -n 1)

    echo "Latest version found: $CLANG_VERSION"

    # The product page has the real download link (dr-download)
    VERSION_URL=$(echo "$CLANG_VERSION" | sed 's/\.\([0-9]\+\)$/_\1/')
    DOWNLOAD_LINK="$CLANG_BASE/${CLANG_VERSION}/ti_cgt_armllvm_${VERSION_URL}_linux-x64_installer.bin"

    echo "Download link detected:"
    echo "$DOWNLOAD_LINK"

    wget --show-progress -c "$DOWNLOAD_LINK"

    chmod +x ti_cgt_armllvm_*.bin && ./ti_cgt_armllvm_*.bin
}

function install_dependencies() {
    if [ $PYTHON -eq 1 ]; then
        echo "##### Installing python #####"
        if which python3 >/dev/null 2>&1; then
            echo "Python found. Skip installation..."
        else
            sudo apt install python3
        fi
        if which pip >/dev/null 2>&1; then
            echo "Pip found. Skip installation..."
        else
            sudo python3 -m ensurepip
        fi
        if which west >/dev/null 2>&1; then
            echo "West found. Skip installation..."
        else
            sudo pip3 install west
        fi
    fi
    if [ $DOXYGEN -eq 1 ]; then
        echo "##### Installing doxygen #####"
        if which doxygen > nul 2>&1 ; then
            echo "Doxygen found. Skip installation..."
        else
            sudo apt install doxygen
        fi
    fi
    if [ $NODEJS -eq 1 ]; then
        echo "##### Installing Node.js #####"
        if which node >/dev/null 2>&1; then
            echo "Node.js found. Skip installation..."
        else
            curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.40.3/install.sh | bash
            \. "$HOME/.nvm/nvm.sh"
            nvm install 12

            node -v # Should print "v12.22.12".
            nvm current # Should print "v12.22.12".
            npm -v # Should print "6.14.16".

            command -v export NVM_DIR="$HOME/.nvm"
            command -v [ -s "$NVM_DIR/nvm.sh" ] && \. "$NVM_DIR/nvm.sh"  # This loads nvm
            command -v [ -s "$NVM_DIR/bash_completion" ] && \. "$NVM_DIR/bash_completion"  # This loads nvm bash_completion
        fi
    fi
    if [ $OPENSSL -eq 1 ]; then
        echo "##### Installing OpenSSL #####"
        if which openssl > nul 2>&1; then
            echo "OpenSSL found. Skip installation..."
        else
            sudo apt install openssl
        fi
    fi
    if [ $CLONE_SDK -eq 1 ]; then
        echo "##### Cloning MCU+SDK repository #####"
        if [ -d "$HOME_TI/mcu_plus_sdk" ]; then
            echo "mcu_plus_sdk already cloned."
        else 
            python3 -m west init -m https://github.com/TexasInstruments/mcupsdk-manifests.git --mr mcupsdk_west --mf all/dev.yml
        fi
        python3 -m west update
        echo

        echo run npm
        cd /$HOME_TI/mcu_plus_sdk && npm ci

        echo Switch MCU+ SDK into development mode 
        if which make >/dev/null 2>&1; then
            make gen-buildfiles DEVICE=am64x GEN_BUILDFILES_TARGET=development
            make gen-buildfiles DEVICE=am243x GEN_BUILDFILES_TARGET=development
        fi
    fi
    if [ $AM243X_SDK -eq 1 ]; then
        echo "##### Installing MCU+SDK for AM243x #####",
        list_sdkVersion AM243X
        chmod +x mcu_plus_sdk_am243x_*.run && ./mcu_plus_sdk_am243x_*.run
    fi
    if [ $AM64XX_SDK -eq 1 ]; then
        echo "##### Installing MCU+SDK for AM64xx #####"
        list_sdkVersion AM64X
        chmod +x mcu_plus_sdk_am64x_*.run && ./mcu_plus_sdk_am64x_*.run
    fi
    if [ $AM263X_SDK -eq 1 ]; then
        echo "##### Installing MCU+SDK for AM263x #####",
        list_sdkVersion AM263X
        chmod +x mcu_plus_sdk_am263x_*.run && ./mcu_plus_sdk_am263x_*.run
    fi
    if [ $AM263PX_SDK -eq 1 ]; then
        echo "##### Installing MCU+SDK for AM263Px #####",
        list_sdkVersion AM263PX
        chmod +x mcu_plus_sdk_am263px_*.run && ./mcu_plus_sdk_am263px_*.run
    fi
    if [ $AM261X_SDK -eq 1 ]; then
        echo "##### Installing MCU+SDK for AM261x #####",
        list_sdkVersion AM261X
        chmod +x mcu_plus_sdk_am261x_*.run && ./mcu_plus_sdk_am261x_*.run
    fi
    if [ $TI_CLANG -eq 1 ]; then
        echo "##### Installing TI ARM Clang #####"
        install_ClangLatest
    fi
    if [ $GCC_A53 -eq 1 ]; then 
        echo "##### Installing GCC for Cortex A53 #####"
        wget https://developer.arm.com/-/media/Files/downloads/gnu-a/$GCC_AARCH64_VERSION/binrel/gcc-arm-${GCC_AARCH64_VERSION}-x86_64-aarch64-none-elf.tar.xz
        tar -xvf gcc-arm-${GCC_AARCH64_VERSION}-x86_64-aarch64-none-elf.tar.xz
    fi
    if [ $GCC_R5 -eq 1 ]; then
        echo "##### Installing GCC for Cortex R5 #####"
        wget https://developer.arm.com/-/media/Files/downloads/gnu-rm/$GCC_ARM_VERSION_FOLDER/gcc-arm-none-eabi-${GCC_ARM_VERSION}-linux.tar.bz2
        tar -xvf gcc-arm-none-eabi-${GCC_ARM_VERSION}-linux.tar.bz2
    fi
    if [ $PRU -eq 1 ]; then
        echo "##### Installing PRU-CGT #####"
        wget https://dr-download.ti.com/software-development/ide-configuration-compiler-or-debugger/MD-FaNNGkDH7s/$PRU_VERSION/ti_cgt_pru_"$PRU_VERSION"_linux_installer_x86.bin
        chmod +x ti_cgt_pru_"$PRU_VERSION"_linux_installer_x86.bin && ./ti_cgt_pru_"$PRU_VERSION"_linux_installer_x86.bin
    fi
    if [ $MONO_RUNTIME -eq 1 ]; then
        echo "##### Installing Mono Runtime #####"
        if which mono > nul 2>&1 ; then
            echo "Mono Runtime found. Skip installation..."
        else
            sudo apt install mono-complete 
        fi
    fi
    if [ $MAKE -eq 1 ]; then
        echo "##### Installing Make #####"
        if which make > nul 2>&1; then
            echo "Make found. Skip installation..."
        else
            sudo apt install make
        fi
    fi
    if [ $SYSCONFIG -eq 1 ]; then
        echo "##### Installing SysConfig #####"
        install_sysconfigLatest
    fi
    if [ $CCS -eq 1 ]; then
        echo "##### Installing Code Composer Studio #####"
        wget https://dr-download.ti.com/software-development/ide-configuration-compiler-or-debugger/MD-J1VdearkvK/$CCS_VERSION/CCS"$CCS_VERSION_WEB"_linux-x64.tar.gz
        tar -xvf CCS"$CCS_VERSION_WEB"_linux-x64.tar.gz
        # install ccs dependdencies before installing itself
        sudo apt -y install libc6:i386 libusb-0.1-4 libgconf-2-4 libncurses5 libpython2.7 libtinfo5 build-essential
        cd CCS"$CCS_VERSION_WEB"_linux-x64 && chmod +x ccs_setup_$CCS_VERSION_WEB.run && ./ccs_setup_$CCS_VERSION_WEB.run
    fi

    # Clean installers
    echo Cleaning Installers...
    rm -r *.run
    rm -r *.bin
    rm -r *.tar.*
    rm -rf CCS"$CCS_VERSION_WEB"_linux-x64
    rm -r nul
}

function show_usage() {
    echo "Usage: $0 [options]"
    echo "Options:"
    echo "    -h, help            Show this help message"
    echo "    -V, -v, verify      Verify dependencies"
    echo "    -I, -i, install     Install dependencies"
    echo "      --python          Install python, pip and west"
    echo "      --doxygen         Install Doxygen"
    echo "      --node            Install Node.js"
    echo "      --openssl         Install OpenSSL"
    echo "      --clone_sdk       Clone MCU+ SDK repository"
    echo "      --am243x_sdk      Install MCU+ SDK for AM243x"
    echo "      --am64xx_sdk      Install MCU+ SDK for AM64xx"
    echo "      --am263x_sdk      Install MCU+ SDK for AM263x"
    echo "      --am263px_sdk     Install MCU+ SDK for AM263Px"
    echo "      --am261x_sdk      Install MCU+ SDK for AM261x"
    echo "      --pru             Install PRU-CGT"
    echo "      --mono            Install Mono Runtime"
    echo "      --make            Install Make"
    echo "      --ccs             Install Code Composer Studio"
    echo "      --sysconfig       Install SysConfig"
    echo "      --ti_clang        Install TI ARM Clang"
    echo "      --gcc_a53         Install GCC for Cortex A53"
    echo "      --gcc_r5          Install GCC for Cortex R5"
    echo "      --all             Install all tools and MCU+ SDK repository"

}

# Check if the script is running from the home directory
echo Checking if running from ${HOME_TI}...
if [ "$PWD" == "$HOME_TI" ]; then
    echo "Ok"
else
    if [ ! -d $HOME_TI ]; then mkdir -p $HOME_TI; fi
    cp "$0" "$HOME_TI"
    cd "$HOME_TI"
    exec $PWD/pru_dependencies.sh $*
fi

# Check the command line arguments
if [ -z "$1" ]; then
    show_usage
    exit
fi

PYTHON=0
DOXYGEN=0
NODEJS=0
OPENSSL=0
CLONE_SDK=0
AM243X_SDK=0
AM64XX_SDK=0
AM263X_SDK=0
AM263PX_SDK=0
AM261X_SDK=0
PRU=0
MONO_RUNTIME=0
MAKE=0
SYSCONFIG=0
CCS=0
TI_CLANG=0
GCC_A53=0
GCC_R5=0
# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        -h|help)
            show_usage
            exit
            ;;
        -V|-v|verify)
            echo "Verifying dependencies..."
            verify_dependencies
            ;;
        -I|-i|install)
            echo "Installing dependencies..."
            while [[ "$2" == --* ]]; do
                case "$2" in
                    --python)
                        PYTHON=1
                        ;;
                    --node)
                        NODEJS=1
                        ;;
                    --doxygen)
                        DOXYGEN=1
                        ;;
                    --openssl)
                        OPENSSL=1
                        ;;
                    --clone_sdk)
                        CLONE_SDK=1
                        ;;
                    --am243x_sdk)
                        AM243X_SDK=1
                        ;;
                    --am64xx_sdk)
                        AM64XX_SDK=1
                        ;;
                    --am263x_sdk)
                        AM263X_SDK=1
                        ;;
                    --am263px_sdk)
                        AM263PX_SDK=1
                        ;;
                    --am261x_sdk)
                        AM261X_SDK=1
                        ;;
                    --pru)
                        PRU=1
                        ;;
                    --mono)
                        MONO_RUNTIME=1
                        ;;
                    --make)
                        MAKE=1
                        ;;
                    --ccs)
                        CCS=1
                        ;;
                    --sysconfig)
                        SYSCONFIG=1
                        ;;
                    --ti_clang)
                        TI_CLANG=1
                        ;;
                    --gcc_a53)
                        GCC_A53=1
                        ;;
                    --gcc_r5)
                        GCC_R5=1
                        ;;
                    --all)
                        PYTHON=1
                        NODEJS=1
                        DOXYGEN=1
                        OPENSSL=1
                        CLONE_SDK=1
                        PRU=1
                        MONO_RUNTIME=1
                        MAKE=1
                        SYSCONFIG=1
                        CCS=1
                        TI_CLANG=1
                        GCC_A53=1
                        GCC_R5=1
                        ;;
                    --*)
                        echo "Unknown dependency: $2"
                        exit
                        ;;
                esac
                shift
            done
            install_dependencies
            ;;
        *)
            echo "Unknown argument: $1"
            exit 1
            ;;&
    esac
    shift
done

