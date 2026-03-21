export OPEN_PRU_PATH?=$(abspath ../../../../../..)
include $(OPEN_PRU_PATH)/imports.mak

# Define build outputs
OUTPUT_NAME := rpmsg_echo_linux_am64x-evm_icss_g1_rtu_pru0_fw

# MCU+ projects: rename & move hex array output
MCU_HEX_NAME := rtupru0_load_bin.h
HEX_ARRAY_PREFIX := RTUPRU0Firmware
MCU_HEX_PATH := $(OPEN_PRU_PATH)/examples/rpmsg_echo_linux/firmware/am64x-evm/$(MCU_HEX_NAME)

# which directories to search for assembly & C source files?
FILES_PATH := .. ../../.. .

# Linker command file
COMMAND_FILES := linker.cmd

# Silicon version (3: PRUSS, PRU-ICSS; 4: PRU_ICSSG)
PRU_VERSION := 4
EXPECTED_DEVICE := am64x

# Default values are defined in pru_rules.mak:
#   - include paths (INCLUDE)
#   - compiler flags (CFLAGS)
#   - linker flags (LFLAGS)

# Optional:
# Pre-define here to override default value
#   OPT_LEVEL, STACK_SIZE, HEAP_SIZE, or ENTRY_POINT

# pru_rules.mak has shared settings for all PRU/RTU/TX_PRU core makefiles
include $(OPEN_PRU_PATH)/pru_rules.mak

# Optional:
# Append values to INCLUDE, CFLAGS, or LFLAGS here.
# Values can be appended like this:
# INCLUDE += --include_path=<value>
INCLUDE += --include_path=$(OPEN_PRU_PATH)/source/include/linux
INCLUDE += --include_path=.

# Core-specific settings for RPMsg Echo project

# INTC CONFIGURATION
HOST_INT_BIT = 30
# The PRU_ICSSG system events used for RPMsg are defined in the Linux device tree
# RTU0 uses system event 20 (To ARM) and 21 (From ARM)
TO_ARM_HOST = 20
FROM_ARM_HOST = 21

# RPMSG CONFIGURATION
CHAN_PORT = 36

# Defines (pass instance specific definitions to the program)
# see --define or -D in 'PRU Optimizing C/C++ Compiler User's Guide'
# For example, to define PRU0, ICSSG1, SOC_AM243X, and set _DEBUG_=1:
# -DPRU0 -DICSSG1 --define=SOC_AM243X -D_DEBUG_=1
DFLAGS := \
	-DCHAN_PORT=$(CHAN_PORT) -DHOST_INT_BIT=$(HOST_INT_BIT) \
	-DTO_ARM_HOST=$(TO_ARM_HOST) -DFROM_ARM_HOST=$(FROM_ARM_HOST)

# Libraries
# see --library in 'PRU Optimizing C/C++ Compiler User's Guide'
LIBS := --library=$(OPEN_PRU_PATH)/source/rpmsg/lib/rpmsg.lib

