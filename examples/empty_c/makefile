include ../../imports.mak

# project information
PROJECT_NAME := empty_c
SUPPORTED_PROCESSORS := am243x am261x am263px am263x am64x

# Only build project if $(DEVICE) is in $(SUPPORTED_PROCESSORS)
ifeq (,$(findstring $(DEVICE),$(SUPPORTED_PROCESSORS)))
all clean:
	@echo Project $(PROJECT_NAME) does not have a build option for $(DEVICE)

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
# am243x-evm
	$(MAKE) -C firmware/am243x-evm/icss_g0_pru0_fw/ti-pru-cgt $(ARGUMENTS)
	$(MAKE) -C firmware/am243x-evm/icss_g0_pru1_fw/ti-pru-cgt $(ARGUMENTS)
	$(MAKE) -C firmware/am243x-evm/icss_g0_rtu_pru0_fw/ti-pru-cgt $(ARGUMENTS)
	$(MAKE) -C firmware/am243x-evm/icss_g0_rtu_pru1_fw/ti-pru-cgt $(ARGUMENTS)
	$(MAKE) -C firmware/am243x-evm/icss_g0_tx_pru0_fw/ti-pru-cgt $(ARGUMENTS)
	$(MAKE) -C firmware/am243x-evm/icss_g0_tx_pru1_fw/ti-pru-cgt $(ARGUMENTS)
	$(MAKE) -C mcuplus/am243x-evm/r5fss0-0_freertos/ti-arm-clang $(ARGUMENTS)
# am243x-lp
	$(MAKE) -C firmware/am243x-lp/icss_g0_pru0_fw/ti-pru-cgt $(ARGUMENTS)
	$(MAKE) -C firmware/am243x-lp/icss_g0_pru1_fw/ti-pru-cgt $(ARGUMENTS)
	$(MAKE) -C firmware/am243x-lp/icss_g0_rtu_pru0_fw/ti-pru-cgt $(ARGUMENTS)
	$(MAKE) -C firmware/am243x-lp/icss_g0_rtu_pru1_fw/ti-pru-cgt $(ARGUMENTS)
	$(MAKE) -C firmware/am243x-lp/icss_g0_tx_pru0_fw/ti-pru-cgt $(ARGUMENTS)
	$(MAKE) -C firmware/am243x-lp/icss_g0_tx_pru1_fw/ti-pru-cgt $(ARGUMENTS)
	$(MAKE) -C mcuplus/am243x-lp/r5fss0-0_freertos/ti-arm-clang $(ARGUMENTS)

am261x:
# am261x-lp
	$(MAKE) -C firmware/am261x-lp/icss_m0_pru0_fw/ti-pru-cgt $(ARGUMENTS)
	$(MAKE) -C firmware/am261x-lp/icss_m0_pru1_fw/ti-pru-cgt $(ARGUMENTS)
	$(MAKE) -C mcuplus/am261x-lp/r5fss0-0_freertos/ti-arm-clang $(ARGUMENTS)
# am261x-som
	$(MAKE) -C firmware/am261x-som/icss_m0_pru0_fw/ti-pru-cgt $(ARGUMENTS)
	$(MAKE) -C firmware/am261x-som/icss_m0_pru1_fw/ti-pru-cgt $(ARGUMENTS)
	$(MAKE) -C mcuplus/am261x-som/r5fss0-0_freertos/ti-arm-clang $(ARGUMENTS)

am263px:
# am263px-lp
	$(MAKE) -C firmware/am263px-lp/icss_m0_pru0_fw/ti-pru-cgt $(ARGUMENTS)
	$(MAKE) -C firmware/am263px-lp/icss_m0_pru1_fw/ti-pru-cgt $(ARGUMENTS)
	$(MAKE) -C mcuplus/am263px-lp/r5fss0-0_freertos/ti-arm-clang $(ARGUMENTS)
# am263px-cc
	$(MAKE) -C firmware/am263px-cc/icss_m0_pru0_fw/ti-pru-cgt $(ARGUMENTS)
	$(MAKE) -C firmware/am263px-cc/icss_m0_pru1_fw/ti-pru-cgt $(ARGUMENTS)
	$(MAKE) -C mcuplus/am263px-cc/r5fss0-0_freertos/ti-arm-clang $(ARGUMENTS)

am263x:
# am263x-lp
	$(MAKE) -C firmware/am263x-lp/icss_m0_pru0_fw/ti-pru-cgt $(ARGUMENTS)
	$(MAKE) -C firmware/am263x-lp/icss_m0_pru1_fw/ti-pru-cgt $(ARGUMENTS)
	$(MAKE) -C mcuplus/am263x-lp/r5fss0-0_freertos/ti-arm-clang $(ARGUMENTS)
# am263x-cc
	$(MAKE) -C firmware/am263x-cc/icss_m0_pru0_fw/ti-pru-cgt $(ARGUMENTS)
	$(MAKE) -C firmware/am263x-cc/icss_m0_pru1_fw/ti-pru-cgt $(ARGUMENTS)
	$(MAKE) -C mcuplus/am263x-cc/r5fss0-0_freertos/ti-arm-clang $(ARGUMENTS)

am64x:
# am64x-evm
	$(MAKE) -C firmware/am64x-evm/icss_g0_pru0_fw/ti-pru-cgt $(ARGUMENTS)
	$(MAKE) -C firmware/am64x-evm/icss_g0_pru1_fw/ti-pru-cgt $(ARGUMENTS)
	$(MAKE) -C firmware/am64x-evm/icss_g0_rtu_pru0_fw/ti-pru-cgt $(ARGUMENTS)
	$(MAKE) -C firmware/am64x-evm/icss_g0_rtu_pru1_fw/ti-pru-cgt $(ARGUMENTS)
	$(MAKE) -C firmware/am64x-evm/icss_g0_tx_pru0_fw/ti-pru-cgt $(ARGUMENTS)
	$(MAKE) -C firmware/am64x-evm/icss_g0_tx_pru1_fw/ti-pru-cgt $(ARGUMENTS)
	$(MAKE) -C mcuplus/am64x-evm/r5fss0-0_freertos/ti-arm-clang $(ARGUMENTS)

.PHONY: all clean pre_build_message $(DEVICE) $(SUPPORTED_PROCESSORS)
