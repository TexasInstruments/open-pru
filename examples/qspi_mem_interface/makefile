include ../../imports.mak

#######################
# project information #
#######################

PROJECT_NAME := qspi_mem_interface
SUPPORTED_PROCESSORS := am261x
# Does the PRU code have dependencies outside of the open-pru repo?
PRU_DEPENDENCIES :=
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
# am261x-lp
	$(MAKE) -C firmware/am261x-lp/icss_m0_pru0_fw/ti-pru-cgt $(ARGUMENTS_PRU)
	$(MAKE) -C firmware/am261x-lp/icss_m0_pru1_fw/ti-pru-cgt $(ARGUMENTS_PRU)

am261x_mcuplus:
# am261x-lp
	$(MAKE) -C mcuplus/am261x-lp/r5fss0-0_freertos/ti-arm-clang $(ARGUMENTS_MCUPLUS)

.PHONY: all clean pre_build_message $(DEVICE) $(DEVICE_NON_PRU) $(SUPPORTED_PROCESSORS)

