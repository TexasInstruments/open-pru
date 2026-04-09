export OPEN_PRU_PATH?=$(abspath ../../../../../../..)
include $(OPEN_PRU_PATH)/imports.mak

# Define build outputs
OUTPUT_NAME := uart_loopback_am62x-sk_pruss0_pru0_fw

# NOT SUPPORTED on AM62x
# AM62x does not support initializing PRU cores from an MCU+ core
# MCU+ projects: rename & move hex array output
MCU_HEX_NAME := pru0_load_bin.h
HEX_ARRAY_PREFIX := PRU0Firmware
MCU_HEX_PATH := $(OPEN_PRU_PATH)/academy/uart/uart_loopback/firmware/am62x-sk/$(MCU_HEX_NAME)

# which directories to search for assembly & C source files?
FILES_PATH := .. ../../.. .

# Linker command file
COMMAND_FILES := linker.cmd

# Silicon version (3: PRUSS, PRU-ICSS; 4: PRU_ICSSG)
PRU_VERSION := 3
EXPECTED_DEVICE := am62x

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

# Defines (pass instance specific definitions to the program)
# see --define or -D in 'PRU Optimizing C/C++ Compiler User's Guide'
# For example, to define PRU0, ICSSG1, SOC_AM243X, and set _DEBUG_=1:
# -DPRU0 -DICSSG1 --define=SOC_AM243X -D_DEBUG_=1
DFLAGS :=

# Libraries
# see --library in 'PRU Optimizing C/C++ Compiler User's Guide'
LIBS :=

