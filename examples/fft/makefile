include ../../imports.mak

# project information
PROJECT_NAME := fft
SUPPORTED_PROCESSORS := am243x am261x

# Only build project if $(DEVICE) is in $(SUPPORTED_PROCESSORS)
ifeq (,$(findstring $(DEVICE),$(SUPPORTED_PROCESSORS)))
all clean:
# do nothing

else
# "make" or "make all" builds projects that match $(DEVICE) set in imports.mak
all: ARGUMENTS = all
all: MESSAGE = "Building $(PROJECT_NAME) for $(DEVICE)"
all: pre_build_message $(DEVICE)

# "make clean" cleans projects that match $(DEVICE) set in imports.mak
clean: ARGUMENTS = clean
clean: MESSAGE = "Cleaning $(PROJECT_NAME) for $(DEVICE)"
clean: pre_build_message $(DEVICE)
endif

pre_build_message:
	@echo $(MESSAGE)

# provide target definitions for each supported processor
# PRU firmware should be built before any RTOS code that includes it
am243x:
# am243x-lp
	$(MAKE) -C firmware/am243x-lp/icss_g0_pru0_fw/ti-pru-cgt $(ARGUMENTS)
	$(MAKE) -C am243x-lp/r5fss0-0_freertos/ti-arm-clang $(ARGUMENTS)

am261x:
# am261x-lp
	$(MAKE) -C firmware/am261x-lp/icss_m0_pru0_fw/ti-pru-cgt $(ARGUMENTS)
	$(MAKE) -C am261x-lp/r5fss0-0_freertos/ti-arm-clang $(ARGUMENTS)

.PHONY: all clean pre_build_message $(DEVICE) $(SUPPORTED_PROCESSORS)