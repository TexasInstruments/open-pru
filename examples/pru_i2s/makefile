include ../../imports.mak

#######################
# Library build tools #
#######################
ifeq ($(CG_TOOL_ROOT),)
  CC=$(CGT_TI_ARM_CLANG_PATH)/bin/tiarmclang
  AR=$(CGT_TI_ARM_CLANG_PATH)/bin/tiarmar
else
  CC=$(CG_TOOL_ROOT)/bin/tiarmclang
  AR=$(CG_TOOL_ROOT)/bin/tiarmar
endif

CFLAGS_AM261X := -mcpu=cortex-r5 -mfloat-abi=hard -mfpu=vfpv3-d16 -mthumb -Wall -Werror -g -Wno-gnu-variable-sized-type-not-at-end -Wno-unused-function -Os
CFLAGS_AM263X := -mcpu=cortex-r5 -mfloat-abi=hard -mfpu=vfpv3-d16 -mthumb -Wall -Werror -g -Wno-gnu-variable-sized-type-not-at-end -Wno-unused-function -Os

INCLUDES_AM261X := \
	-I$(MCU_PLUS_SDK_PATH)/source \
	-I$(OPEN_PRU_PATH)/source \
	-I$(MCU_PLUS_SDK_PATH)/source/kernel/freertos/FreeRTOS-Kernel/include \
	-I$(MCU_PLUS_SDK_PATH)/source/kernel/freertos/portable/TI_ARM_CLANG/ARM_CR5F \
	-I$(MCU_PLUS_SDK_PATH)/source/kernel/freertos/config/am261x/r5f \
	-I./include \
	-I./firmware \
	-I. \
	-I.. \
	-I$(OPEN_PRU_PATH)/examples

INCLUDES_AM263X := \
	-I$(MCU_PLUS_SDK_PATH)/source \
	-I$(OPEN_PRU_PATH)/source \
	-I$(MCU_PLUS_SDK_PATH)/source/kernel/freertos/FreeRTOS-Kernel/include \
	-I$(MCU_PLUS_SDK_PATH)/source/kernel/freertos/portable/TI_ARM_CLANG/ARM_CR5F \
	-I$(MCU_PLUS_SDK_PATH)/source/kernel/freertos/config/am263x/r5f \
	-I./include \
	-I./firmware \
	-I. \
	-I.. \
	-I$(OPEN_PRU_PATH)/examples

DEFINES_AM261X := -DSOC_AM261X -DOS_FREERTOS
DEFINES_AM263X := -DSOC_AM263X -DOS_FREERTOS

#######################
# project information #
#######################

PROJECT_NAME := pru_i2s
SUPPORTED_PROCESSORS := am261x am263x
# Does the PRU code have dependencies outside of the open-pru repo?
PRU_DEPENDENCIES := mcuplus
# Use NON_PRU_DEPENDENCIES to select which makefiles to call for non-PRU cores
NON_PRU_DEPENDENCIES := mcuplus

###################
# Prebuild checks #
###################
BUILD_PROJECT := y
DEVICE_NON_PRU :=

# Only build project if $(DEVICE) is in $(SUPPORTED_PROCESSORS)
ifeq (,$(findstring $(DEVICE),$(SUPPORTED_PROCESSORS)))
BUILD_PROJECT := n
MESSAGE := Project $(PROJECT_NAME) does not have a build option for $(DEVICE)
endif

# Only build project if PRU dependencies exist
ifneq (,$(PRU_DEPENDENCIES))
# MCU+ SDK dependency?
ifeq (mcuplus,$(findstring mcuplus,$(PRU_DEPENDENCIES)))
ifneq ($(BUILD_MCUPLUS),y)
BUILD_PROJECT := n
MESSAGE ?= Project $(PROJECT_NAME) depends on MCU+ SDK, but BUILD_MCUPLUS != y.
endif
endif
# Additional PRU dependency checks go here. For example:
#ifeq (dependency,$(findstring dependency,$(PRU_DEPENDENCIES)))
#if (dependency check fails)
#BUILD_PROJECT := n
#MESSAGE ?= Project $(PROJECT_NAME) depends on dependency, but dependency does not exist.
#endif
#endif
endif

# Only build non-PRU code if the non-PRU code is enabled in imports.mak
ifeq ($(BUILD_MCUPLUS),y)
ifeq (mcuplus,$(findstring mcuplus,$(NON_PRU_DEPENDENCIES)))
DEVICE_NON_PRU += $(DEVICE)_mcuplus
endif
endif
ifeq ($(BUILD_LINUX),y)
ifeq (linux,$(findstring linux,$(NON_PRU_DEPENDENCIES)))
DEVICE_NON_PRU += $(DEVICE)_linux
endif
endif

###########################
# Make and clean commands #
###########################

# "make pru" builds PRU firmware for all supported devices
pru: ARGUMENTS_PRU = all
pru: ARGUMENTS_MCUPLUS = all
pru: MESSAGE = "Building $(PROJECT_NAME) PRU firmware"
pru: pre_build_message $(SUPPORTED_PROCESSORS)

# "make host" builds host (R5F/ARM) code for specified DEVICE
host: ARGUMENTS_PRU = pru
host: ARGUMENTS_MCUPLUS = all
host: MESSAGE = "Building $(PROJECT_NAME) host code for $(DEVICE)"
host: pre_build_message $(DEVICE_NON_PRU)

ifeq ($(BUILD_PROJECT),y)
# "make" or "make all" builds projects that match $(DEVICE) set in imports.mak
all: ARGUMENTS_PRU = all
all: ARGUMENTS_MCUPLUS = $(ARGUMENTS_PRU)
all: MESSAGE = "Building $(PROJECT_NAME) for $(DEVICE)"
all: pre_build_message $(DEVICE) $(DEVICE_NON_PRU)

# "make clean" cleans projects that match $(DEVICE) set in imports.mak
clean: ARGUMENTS_PRU = clean
clean: ARGUMENTS_MCUPLUS = scrub
clean: MESSAGE = "Cleaning $(PROJECT_NAME) for $(DEVICE)"
clean: pre_build_message $(DEVICE) $(DEVICE_NON_PRU)

else
# if a prebuild check failed, print message and exit
all clean: pre_build_message
endif

pre_build_message:
	@echo $(MESSAGE)

######################
# Target definitions #
######################

# provide target definitions for each supported processor
# PRU firmware should be built before any RTOS code that includes it
am261x:
# am261x-lp - I2S protocol
	$(MAKE) -C firmware/I2S/pru0_tx/am261x-lp/icssm0-pru0_fw/ti-pru-cgt $(ARGUMENTS_PRU)
	$(MAKE) -C firmware/I2S/pru1_rx/am261x-lp/icssm0-pru1_fw/ti-pru-cgt $(ARGUMENTS_PRU)
# am261x-lp - TDM4 protocol
	$(MAKE) -C firmware/TDM4/pru0_tx/am261x-lp/icssm0-pru0_fw/ti-pru-cgt $(ARGUMENTS_PRU)
	$(MAKE) -C firmware/TDM4/pru1_rx/am261x-lp/icssm0-pru1_fw/ti-pru-cgt $(ARGUMENTS_PRU)

am261x_mcuplus: build_lib_am261x
# am261x-lp - R5F application
	$(MAKE) -C pru_i2s_app/am261x-lp/r5fss0-0_freertos/ti-arm-clang $(ARGUMENTS_MCUPLUS)

build_lib_am261x:
# am261x-lp - Build driver library (always rebuild by removing old lib and objects first)
	$(RM) lib/pru_i2s.am261x.r5f.ti-arm-clang.*.lib lib/pru_i2s_*_am261x.obj
	$(MKDIR) lib
	@echo "Building pru_i2s library for am261x..."
	$(CC) -c $(CFLAGS_AM261X) $(INCLUDES_AM261X) $(DEFINES_AM261X) -o lib/pru_i2s_drv_am261x.obj driver/pru_i2s_drv.c
	$(CC) -c $(CFLAGS_AM261X) $(INCLUDES_AM261X) $(DEFINES_AM261X) -o lib/pru_i2s_app_config_am261x.obj pru_i2s_app/pru_i2s_app_config.c
	$(AR) rc lib/pru_i2s.am261x.r5f.ti-arm-clang.release.lib lib/pru_i2s_drv_am261x.obj lib/pru_i2s_app_config_am261x.obj
	$(RM) lib/pru_i2s_*_am261x.obj
	@echo "Library build complete."

am263x:
# am263x-cc - I2S protocol
	$(MAKE) -C firmware/I2S/pru0_tx/am263x-cc/icssm0-pru0_fw/ti-pru-cgt $(ARGUMENTS_PRU)
	$(MAKE) -C firmware/I2S/pru1_rx/am263x-cc/icssm0-pru1_fw/ti-pru-cgt $(ARGUMENTS_PRU)
# am263x-cc - TDM4 protocol
	$(MAKE) -C firmware/TDM4/pru0_tx/am263x-cc/icssm0-pru0_fw/ti-pru-cgt $(ARGUMENTS_PRU)
	$(MAKE) -C firmware/TDM4/pru1_rx/am263x-cc/icssm0-pru1_fw/ti-pru-cgt $(ARGUMENTS_PRU)

am263x_mcuplus: build_lib_am263x
# am263x-cc - R5F application
	$(MAKE) -C pru_i2s_app/am263x-cc/r5fss0-0_freertos/ti-arm-clang $(ARGUMENTS_MCUPLUS)

build_lib_am263x:
# am263x-cc - Build driver library (always rebuild by removing old lib and objects first)
	$(RM) lib/pru_i2s.am263x.r5f.ti-arm-clang.*.lib lib/pru_i2s_*_am263x.obj
	$(MKDIR) lib
	@echo "Building pru_i2s library for am263x..."
	$(CC) -c $(CFLAGS_AM263X) $(INCLUDES_AM263X) $(DEFINES_AM263X) -o lib/pru_i2s_drv_am263x.obj driver/pru_i2s_drv.c
	$(CC) -c $(CFLAGS_AM263X) $(INCLUDES_AM263X) $(DEFINES_AM263X) -o lib/pru_i2s_app_config_am263x.obj pru_i2s_app/pru_i2s_app_config.c
	$(AR) rc lib/pru_i2s.am263x.r5f.ti-arm-clang.release.lib lib/pru_i2s_drv_am263x.obj lib/pru_i2s_app_config_am263x.obj
	$(RM) lib/pru_i2s_*_am263x.obj
	@echo "Library build complete."

.PHONY: all clean pru host pre_build_message $(DEVICE) $(DEVICE_NON_PRU) $(SUPPORTED_PROCESSORS)
