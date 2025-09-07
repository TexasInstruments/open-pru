include imports.mak

SUBDIRS := examples academy

# "make" or "make all" builds projects that match $(DEVICE) set in imports.mak
all: ARGUMENTS = all
all: $(SUBDIRS)

# "make clean" cleans projects that match $(DEVICE) set in imports.mak
clean: ARGUMENTS = clean
clean: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ $(ARGUMENTS)

help:
	@echo  Notes:
	@echo  .
	@echo  These entries in imports.mak determine which projects get built:
	@echo  - DEVICE: select which processor to build for
	@echo  - BUILD_MCUPLUS: build MCU+ projects? y/n
	@echo  - BUILD_LINUX: build Linux projects? y/n
	@echo  .
	@echo  "-s" is used to suppress output. Remove to see all make prints
	@echo  .
	@echo  Set PROFILE=debug or PROFILE=release in imports.mak to build with
	@echo  the debug or release profile
	@echo  .
	@echo  "-j<thread_number>" can reduce build time with parallel builds.
	@echo  The -j option may not work with the windows command prompt
	@echo  .
	@echo  Overall build targets,
	@echo  ======================
	@echo  $(MAKE) help          // show this help menu
	@echo  .
	@echo  $(MAKE) -s            // build all projects that match DEVICE
	@echo  $(MAKE) -s clean      // clean all projects that match DEVICE
	@echo  .
	@echo  Build a single project,
	@echo  =======================
	@echo  You can build the code for a single project like this:
	@echo  cd examples/empty  // go to the project directory
	@echo  $(MAKE) -s         // build code for cores that match DEVICE
	@echo  $(MAKE) -s clean   // clean code for cores that match DEVICE
	@echo  .
	@echo  Build code for a single core,
	@echo  =============================
	@echo  Build code for a single core like this:
	@echo  $(MAKE) -s -C examples/empty/firmware/am64x-evm/icss_g0_pru0_fw/ti-pru-cgt [clean syscfg-gui syscfg]
	@echo  $(MAKE) -s -C examples/empty/firmware/am64x-evm/icss_g0_pru1_fw/ti-pru-cgt [clean syscfg-gui syscfg]
	@echo  $(MAKE) -s -C examples/empty/firmware/am64x-evm/icss_g0_rtu_pru0_fw/ti-pru-cgt [clean syscfg-gui syscfg]
	@echo  $(MAKE) -s -C examples/empty/firmware/am64x-evm/icss_g0_rtu_pru1_fw/ti-pru-cgt [clean syscfg-gui syscfg]
	@echo  $(MAKE) -s -C examples/empty/firmware/am64x-evm/icss_g0_tx_pru0_fw/ti-pru-cgt [clean syscfg-gui syscfg]
	@echo  $(MAKE) -s -C examples/empty/firmware/am64x-evm/icss_g0_tx_pru1_fw/ti-pru-cgt [clean syscfg-gui syscfg]
	@echo  $(MAKE) -s -C examples/empty/am64x-evm/r5fss0-0_freertos/ti-arm-clang [clean syscfg-gui syscfg]
	@echo  .
	@echo  Note that PRU firmware should be built before any RTOS code that includes
	@echo  the PRU firmware.
	@echo  .

.PHONY: all clean help $(SUBDIRS)
