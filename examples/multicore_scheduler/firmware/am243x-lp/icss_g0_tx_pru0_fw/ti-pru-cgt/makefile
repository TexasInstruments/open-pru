export OPEN_PRU_PATH?=$(abspath ../../../../../..)
include $(OPEN_PRU_PATH)/imports.mak

# Define build outputs
OUTPUT_NAME := multicore_scheduler_am243x-lp_icss_g0_tx_pru0_fw

# MCU+ projects: rename & move hex array output
MCU_HEX_NAME := tx_pru0_load_bin.h
HEX_ARRAY_PREFIX := TXPRU0Firmware
MCU_HEX_PATH := $(OPEN_PRU_PATH)/examples/multicore_scheduler/firmware/am243x-lp/$(MCU_HEX_NAME)

# which directories to search for assembly & C source files?
FILES_PATH := .. ../../.. .

# Linker command file
COMMAND_FILES := linker.cmd

# Silicon version (3: PRUSS, PRU-ICSS; 4: PRU_ICSSG)
PRU_VERSION := 4
EXPECTED_DEVICE := am243x

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

