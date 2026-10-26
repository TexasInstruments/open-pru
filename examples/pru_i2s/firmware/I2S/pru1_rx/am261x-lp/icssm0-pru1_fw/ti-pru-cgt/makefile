################################################################################
# PRU I2S - I2S Protocol - PRU1 RX - AM261x-LP
################################################################################

export OPEN_PRU_PATH?=$(abspath ../../../../../../../..)
include $(OPEN_PRU_PATH)/imports.mak

# Define build outputs
OUTPUT_NAME := pru_i2s_am261x-lp_icssm0-pru1_fw

# MCU+ projects: rename & move hex array output
MCU_HEX_NAME := pru_i2s_pru1_array.h
HEX_ARRAY_PREFIX := pru_prupru_i2s1_image_0
MCU_HEX_PATH := $(OPEN_PRU_PATH)/examples/pru_i2s/firmware/I2S/$(MCU_HEX_NAME)

# which directories to search for assembly & C source files?
FILES_PATH := ../../../../

# Linker command file
COMMAND_FILES := linker.cmd

# Silicon version (3: PRUSS, PRU-ICSS; 4: PRU_ICSSG)
PRU_VERSION := 3
EXPECTED_DEVICE := am261x


# pru_rules.mak has shared settings for all PRU/RTU/TX_PRU core makefiles
include $(OPEN_PRU_PATH)/pru_rules.mak

# Additional include paths for I2S firmware headers
INCLUDE += --include_path=$(OPEN_PRU_PATH)/examples/pru_i2s/firmware/include
INCLUDE += --include_path=$(OPEN_PRU_PATH)/examples/pru_i2s/firmware/I2S

# Defines: PRU1, I2S Rx, overflow detection
DFLAGS := \
	-DPRU1 -DSLICE1 \
	--define=SOC_AM261X \
	--define=PRU1 \
	--define=I2S_RX \
	--define=I2S_RX_DETECT_OVERFLOW
