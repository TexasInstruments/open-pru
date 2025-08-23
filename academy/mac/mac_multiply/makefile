include ../../../imports.mak

# project information
PROJECT_NAME := mac_multiply
SUPPORTED_PROCESSORS := am261x am263px am263x

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
am261x:
# am261x-lp
	$(MAKE) -C firmware/am261x-lp/icss_m0_pru0_fw/ti-pru-cgt $(ARGUMENTS)
	$(MAKE) -C firmware/am261x-lp/icss_m0_pru1_fw/ti-pru-cgt $(ARGUMENTS)
# am261x-som
	$(MAKE) -C firmware/am261x-som/icss_m0_pru0_fw/ti-pru-cgt $(ARGUMENTS)
	$(MAKE) -C firmware/am261x-som/icss_m0_pru1_fw/ti-pru-cgt $(ARGUMENTS)

am263px:
# am263px-lp
	$(MAKE) -C firmware/am263px-lp/icss_m0_pru0_fw/ti-pru-cgt $(ARGUMENTS)
	$(MAKE) -C firmware/am263px-lp/icss_m0_pru1_fw/ti-pru-cgt $(ARGUMENTS)
# am263px-cc
	$(MAKE) -C firmware/am263px-cc/icss_m0_pru0_fw/ti-pru-cgt $(ARGUMENTS)
	$(MAKE) -C firmware/am263px-cc/icss_m0_pru1_fw/ti-pru-cgt $(ARGUMENTS)

am263x:
# am263x-lp
	$(MAKE) -C firmware/am263x-lp/icss_m0_pru0_fw/ti-pru-cgt $(ARGUMENTS)
	$(MAKE) -C firmware/am263x-lp/icss_m0_pru1_fw/ti-pru-cgt $(ARGUMENTS)
# am263x-cc
	$(MAKE) -C firmware/am263x-cc/icss_m0_pru0_fw/ti-pru-cgt $(ARGUMENTS)
	$(MAKE) -C firmware/am263x-cc/icss_m0_pru1_fw/ti-pru-cgt $(ARGUMENTS)

.PHONY: all clean pre_build_message $(DEVICE) $(SUPPORTED_PROCESSORS)