@echo off
@REM ===========================================================================
@REM File: pru_dependencies.bat
@REM Description: Installation script for TI tools
@REM ---------------------------------------------------------------------------

@REM Set version numbers for TI tools
set HOME=C:\ti
set CCS_VERSION=12.8.1
set CCS_PATH_VERSION=%CCS_VERSION:.=%
set SYSCONFIG_WEB=1.21.2.3837
for /f "tokens=1-3 delims=." %%a in ("%SYSCONFIG_WEB%") do set SYSCONFIG_VERSION=%%a.%%b.%%c
set TI_ARM_VERSION=4.0.1.LTS
set DOXY_VERSION=1.8.20
set GCC_AARCH64_VERSION=9.2-2019.12
set GCC_ARM_VERSION=7-2017-q4-major-win32
set GCC_ARM_VERSION_FOLDER=7-2017q4
set PRU_VERSION=2.3.3
set GMAKE_PATH=%HOME%\ccs%CCS_PATH_VERSION%\ccs\utils\bin\
set MCU_SDK_VERSION=11_00_00_15

@REM Check if the script is running from the home directory
echo Checking if running from %HOME%...
if "%cd%" == "%HOME%" (
    echo OK
) else (
    @REM If not running from the home directory, move the script to the home directory
    if not exist "%HOME%" (
        mkdir "%HOME%"
    )
    copy /y "%~f0" "%HOME%"
    cd /d "%HOME%"
    start "" "%~f0"
    exit /b
)

@REM Check the command line arguments
if "%1" == "" goto usage
if "%1" == "-h" goto usage
if "%1" == "help" goto usage
if "%1" == "verify" goto verify
if "%1" == "-V" goto verify
if "%1" == "-v" goto verify
if "%1" == "install" goto install_dependencies
if "%1" == "-I" goto install_dependencies
if "%1" == "-i" goto install_dependencies
goto usage

@REM Verify the dependencies
:verify
echo Verifying dependencies...
echo ##### Python #####
where python > nul 2>&1 && (echo found) || (echo Please install Python using the command pru_dependencies.bat -I --python)
echo ##### pip #####
where pip > nul 2>&1 && (echo found) || (echo Please install pip using the command pru_dependencies.bat -I --python)
echo ##### west #####
where west > nul 2>&1 && (echo found) || (echo Please install west using the command pru_dependencies.bat -I --python)
echo ##### Node.js #####
node --version > nul 2>&1 && (echo found) || (echo Please install Node.js using the command pru_dependencies.bat -I --node)
echo ##### OpenSSL #####
openssl --version > nul 2>&1 && (echo found) || (echo Please install OpenSSL using the command pru_dependencies.bat -I --openssl)
echo ##### Doxygen #####
doxygen --version > nul 2>&1 && (echo found) || (echo Please install Doxygen using the command pru_dependencies.bat -I --doxygen)
echo ##### MCU+ SDK #####
if exist "%HOME%\mcu_plus_sdk" (
    echo MCU+ SDK already cloned.
) else (
    echo MCU+ SDK not found in system.
    echo Please install MCU+ SDK using the command pru_dependencies.bat -I --clone_sdk
)
if exist "%HOME%\mcu_plus_sdk_am243x_%MCU_SDK_VERSION%" (
    echo Latest version of MCU+ SDK for AM243x already installed.
) else (
    echo MCU+ SDK for AM243x not found in system.
    echo Please install MCU+ SDK using the command pru_dependencies.bat -I --am243x_sdk
)
if exist "%HOME%\mcu_plus_sdk_am64x_%MCU_SDK_VERSION%" (
    echo Latest version of MCU+ SDK for AM64xx already installed.
) else (
    echo MCU+ SDK for AM64xx not found in system.
    echo Please install MCU+ SDK using the command pru_dependencies.bat -I --am243x_sdk
)
echo ##### Code Composer Studio #####
if exist "%HOME%\ccs%CCS_PATH_VERSION%" (
    echo CCS already installed.
) else (
    echo CCS was not found inside %HOME%. Please download from https://www.ti.com/tool/download/CCSTUDIO/%CCS_VERSION%
    echo Install CCS in %HOME%.
)
echo ##### SysConfig #####
if exist "%HOME%\sysconfig_%SYSCONFIG_VERSION%" (
    echo SysConfig already installed.
) else (
    echo SysConfig was not found inside %HOME%. Please download from https://www.ti.com/tool/download/SYSCONFIG/%SYSCONFIG_WEB%
    echo Install SysConfig in %HOME%.
)
echo ##### TI ARM Clang #####
if exist "%HOME%\ti-cgt-armllvm_%TI_ARM_VERSION%" (
    echo TI ARM Clang already installed.
) else (
    echo TI ARM Clang was not found inside %HOME%. Please download from https://www.ti.com/tool/download/ARM-CGT-CLANG/%TI_ARM_VERSION%
    echo Install the compiler in %HOME%.
)
echo ##### GCC for Cortex A53 #####
if exist "%HOME%\gcc-arm-%GCC_AARCH64_VERSION%" (
    echo GCC for Cortex A53 already installed.
) else ( 
    echo GCC for Cortex A53 was not found inside %HOME%. Please download from https://developer.arm.com/-/media/Files/downloads/gnu-a/%GCC_AARCH64_VERSION%/binrel/gcc-arm-%GCC_AARCH64_VERSION%-mingw-w64-i686-aarch64-none-elf.tar.xz
    echo Unzip the compiler in %HOME%\gcc-arm-%GCC_AARCH64_VERSION%.
)
echo ##### GCC for Cortex R5 #####
if exist "%HOME%\gcc-arm-none-eabi-%GCC_ARM_VERSION%" (
    echo GCC for ARM Cortex R5 already installed.
) else ( 
    echo GCC for ARM Cortex R5 was not found inside %HOME%. Please download from https://developer.arm.com/-/media/Files/downloads/gnu-rm/%GCC_ARM_VERSION_FOLDER%/gcc-arm-none-eabi-%GCC_ARM_VERSION%.zip
    echo Unzip the compiler in %HOME%\gcc-arm-none-eabi-%GCC_ARM_VERSION%.
)
echo ##### PRU-CGT #####
if exist "%HOME%\ti-cgt-pru_%PRU_VERSION%" (
    echo PRU CGT already installed.
) else (
    echo PRU-CGT not found inside %HOME%.
    echo Please install PRU-CGT using the command pru_dependencies.bat -I --pru
)

goto end

@REM Install dependencies
:install_dependencies
shift
set "PYTHON=0"
set "DOXYGEN=0"
set "NODEJS=0"
set "OPENSSL=0"
set "MCU_SDK=0"
set "PRU_CGT=0"
set "AM243x_SDK=0"
set "AM64xx_SDK=0"

@REM Parse command line arguments
:parse_args
if "%1" == "" goto install
if "%1" == "--python" set "PYTHON=1"
if "%1" == "--doxygen" set "DOXYGEN=1"
if "%1" == "--node" set "NODEJS=1"
if "%1" == "--openssl" set "OPENSSL=1"
if "%1" == "--clone_sdk" set "MCU_SDK=1"
if "%1" == "--am243x_sdk" set "AM243x_SDK=1"
if "%1" == "--am64xx_sdk" set "AM64xx_SDK=1"
if "%1" == "--pru" set "PRU_CGT=1"
if "%1" == "--all" (
    set "PYTHON=1"
    set "DOXYGEN=1"
    set "NODEJS=1"
    set "OPENSSL=1"
    set "PRU_CGT=1"
    set "MCU_SDK=1"
)
shift
goto parse_args

@REM Install dependencies
:install
set arch=Win32
if "%PROCESSOR_ARCHITECTURE%" == "x86" (
    if defined PROCESSOR_ARCHITEW6432 (
        set arch=Win64
    )
) else if "%PROCESSOR_ARCHITECTURE%" == "AMD64" (
    set arch=Win64
)

echo Installing dependencies...
if %PYTHON% == 1 (
    echo ##### Installing python #####
    where python > nul 2>&1 && (echo Python found. Skip installation...) || (powershell -Command "Invoke-WebRequest -Uri https://www.python.org/ftp/python/3.13.2/python-3.13.2-amd64.exe -OutFile py3_installer.exe"
                                                                             powershell -Command "Start-Process -FilePath py3_installer.exe -Wait")
    where pip > nul 2>&1 && (echo Pip found. Skip installation...) || (py -m pip install --upgrade pip)
    where west > nul 2>&1 && (echo West found. Skip installation...) || (py -m pip install west)
)
if %DOXYGEN% == 1 (
    echo ##### Installing doxygen #####
    powershell -Command "Invoke-WebRequest -Uri https://github.com/doxygen/doxygen/releases/download/Release_1_9_8/doxygen-1.9.8-setup.exe -OutFile doxygen_installer.exe"
    powershell -Command "Start-Process -FilePath doxygen_installer.exe -Wait"
)
if %NODEJS% == 1 (
    echo ##### Installing Node.js #####
    powershell -Command "Invoke-WebRequest -Uri https://nodejs.org/dist/v22.14.0/node-v22.14.0-x64.msi -OutFile nodejs.msi"
    msiexec /i nodejs.msi
)
if %OPENSSL% == 1 ( 
    echo ##### Installing OpenSSL #####
    powershell -Command "Invoke-WebRequest -Uri https://slproweb.com/download/%arch%OpenSSL_Light-3_4_1.msi -OutFile openssl.msi"
    msiexec /i openssl.msi
)
if %PRU_CGT% == 1 (
    echo ##### Installing PRU-CGT #####
    powershell -Command "Invoke-WebRequest -Uri https://dr-download.ti.com/software-development/ide-configuration-compiler-or-debugger/MD-FaNNGkDH7s/%PRU_VERSION%/ti_cgt_pru_%PRU_VERSION%_windows_installer.exe -OutFile pru-cgt_%PRU_VERSION%.exe"
    powershell -Command "Start-Process -FilePath pru-cgt_%PRU_VERSION%.exe -Wait"
)
if %MCU_SDK% == 1 (
    echo ##### Cloning MCU+SDK repository #####
    if exist "%HOME%\mcu_plus_sdk" (
        echo mcu_plus_sdk already cloned.
    ) else (
        py -m west init -m https://github.com/TexasInstruments/mcupsdk-manifests.git --mr mcupsdk_west --mf all/dev.yml
    )
    py -m west update
    echo.

    echo run npm
    cd mcu_plus_sdk
    npm ci

    echo Switch MCU+ SDK into development mode 
    if exist "%GMAKE_PATH%" (
        powershell -Command "$currentPath = [Environment]::GetEnvironmentVariable('Path', 'User'); if ($currentPath -like '*%GMAKE_PATH%*') { Write-Host 'Path already exists' } else { [Environment]::SetEnvironmentVariable('Path', $currentPath + ';%GMAKE_PATH%', 'User') }"

        %GMAKE_PATH%\gmake gen-buildfiles DEVICE=am64x GEN_BUILDFILES_TARGET=development
        %GMAKE_PATH%\gmake gen-buildfiles DEVICE=am243x GEN_BUILDFILES_TARGET=development
    )
)
if %AM243x_SDK% == 1 ( 
    echo ##### Installing MCU+SDK for AM243x #####
    powershell -Command "Invoke-WebRequest -Uri https://dr-download.ti.com/software-development/software-development-kit-sdk/MD-ouHbHEm1PK/11.00.00.15/mcu_plus_sdk_am243x_11_00_00_15-windows-x64-installer.exe"
    powershell -Command "Start-Process -FilePath mcu_plus_sdk_am243x_11_00_00_15-windows-x64-installer.exe -Wait"
)
if %AM64xx_SDK% == 1 ( 
    echo ##### Installing MCU+SDK for AM64xx #####
    powershell -Command "Invoke-WebRequest -Uri https://dr-download.ti.com/software-development/software-development-kit-sdk/MD-SfkcjYAjGS/11.00.00.15/mcu_plus_sdk_am64x_11_00_00_15-windows-x64-installer.exe"
    powershell -Command "Start-Process -FilePath mcu_plus_sdk_am64x_11_00_00_15-windows-x64-installer.exe -Wait"
)

@REM Clean uninstallers
echo Cleaning Installers...
del /F /Q "C:\ti\*.exe"
del /F /Q "C:\ti\*.msi"

goto end

@REM Print usage message
:usage
echo Usage: %0 [-V ^| -v ^| verify] [-I ^| install] [--python] [--doxygen] [--node] [--openssl] [--sdk] [--am243x_sdk] [--am64xx_sdk] [--pru] [--all]
echo.
echo  --python: install python, pip and west
echo  --doxygen: install Doxygen
echo  --node: install Node.js
echo  --openssl: install OpenSSL
echo  --clone_sdk: clone MCU+ SDK repository
echo  --am243x_sdk: install MCU+ SDK for AM243x
echo  --am64xx_sdk: install MCU+ SDK for AM64xx
echo  --pru: install PRU-CGT
echo  --all: Install all tools and MCU+ SDK repository

:end
