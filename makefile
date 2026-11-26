include imports.mak

SUBDIRS := source academy examples

# "make" or "make all" builds projects that match $(DEVICE) set in imports.mak
# this also builds all libraries in the source directory and the project
# libraries ("make libs")
all:
	$(MAKE) -C source
	$(MAKE) libs
	$(MAKE) pru
	$(MAKE) host

# "make libs" builds the libraries owned by projects (for example pru_i2s)
# the projects link these libraries, but "make pru" and "make host" do not build them
libs:
	$(MAKE) -C examples libs

# "make libs-clean" cleans the libraries owned by projects. "make clean" does not
libs-clean:
	$(MAKE) -C examples libs-clean

# "make pru" builds PRU firmware for all projects matching $(DEVICE)
pru: ARGUMENTS := pru
pru: academy examples

# "make host" builds host (non-PRU) code for all projects matching $(DEVICE)
host: ARGUMENTS := host
host: academy examples

# "make clean" cleans projects that match $(DEVICE) set in imports.mak
# this also cleans all libraries in the source directory
clean: ARGUMENTS := clean
clean: $(SUBDIRS)

academy examples source:
	$(MAKE) -C $@ $(ARGUMENTS)

help:
	@echo  "****************"
	@echo  "How to use make:"
	@echo  "****************"
	@echo
	@echo  "Linux:   $(MAKE) = make"
	@echo  "Windows: $(MAKE) = gmake"
	@echo  "    gmake is present in CCS. Add the path to the CCS gmake at"
	@echo  "    C:/ti/ccsxxxx/ccs/utils/bin to your Windows PATH"
	@echo
	@echo  "imports.mak"
	@echo  "These entries determine which projects get built:"
	@echo  "- DEVICE: select which processor to build for"
	@echo  "- BUILD_MCUPLUS: build MCU+ projects? y/n"
	@echo  "- BUILD_LINUX: build Linux projects? y/n"
	@echo
	@echo  "-s is used to suppress output. Remove to see all make prints"
	@echo
	@echo  "Set PROFILE=debug or PROFILE=release in imports.mak to build with"
	@echo  "the debug or release profile"
	@echo
	@echo  "-j<thread_number> can reduce build time with parallel builds."
	@echo  "- The -j option will scramble the print order. Do not use for debug"
	@echo  "- Windows command prompt may not work or require a different"
	@echo  "  format, like -j without a thread number"
	@echo
	@echo  "Overall build targets:"
	@echo  "======================"
	@echo  "$(MAKE) help          // show this help menu"
	@echo
	@echo  "$(MAKE) -s            // build all projects that match DEVICE"
	@echo  "$(MAKE) -s pru        // build only PRU firmware for all projects"
	@echo  "$(MAKE) -s host       // build only host code for all projects"
	@echo  "$(MAKE) -s clean      // clean all projects that match DEVICE"
	@echo  "$(MAKE) -s libs       // build only the libraries owned by projects"
	@echo  "$(MAKE) -s libs-clean // clean only the libraries owned by projects"
	@echo
	@echo  "The overall build targets also build & clean all libraries in the"
	@echo  "source/ directory"
	@echo
	@echo  "Libraries owned by a project (for example the pru_i2s driver library)"
	@echo  "are built by make and make libs. make clean does not remove them, use"
	@echo  "make libs-clean. Rebuild them with make libs after changing the library"
	@echo  "source code."
	@echo
	@echo  "Note: make pru and make host assume source/ libraries and project"
	@echo  "libraries are already built. Run make first on a fresh clone to build"
	@echo  "everything in order."
	@echo
	@echo  "Build a single project:"
	@echo  "======================="
	@echo  "You can build the code for a single project or library like this:"
	@echo  "cd examples/empty  // go to the project directory"
	@echo  "$(MAKE) -s         // build PRU firmware first, then host code"
	@echo  "$(MAKE) -s pru     // build only PRU firmware"
	@echo  "$(MAKE) -s host    // build only host code"
	@echo  "$(MAKE) -s clean   // clean code for cores that match DEVICE"
	@echo
	@echo  "Build code for a single core:"
	@echo  "============================="
	@echo  "Build code for a single core like this:"
	@echo  "$(MAKE) -s -C examples/empty/firmware/am64x-evm/icss_g0_pru0_fw/ti-pru-cgt [clean]"
	@echo  "$(MAKE) -s -C examples/empty/firmware/am64x-evm/icss_g0_pru1_fw/ti-pru-cgt [clean]"
	@echo  "$(MAKE) -s -C examples/empty/firmware/am64x-evm/icss_g0_rtu_pru0_fw/ti-pru-cgt [clean]"
	@echo  "$(MAKE) -s -C examples/empty/firmware/am64x-evm/icss_g0_rtu_pru1_fw/ti-pru-cgt [clean]"
	@echo  "$(MAKE) -s -C examples/empty/firmware/am64x-evm/icss_g0_tx_pru0_fw/ti-pru-cgt [clean]"
	@echo  "$(MAKE) -s -C examples/empty/firmware/am64x-evm/icss_g0_tx_pru1_fw/ti-pru-cgt [clean]"
	@echo  "$(MAKE) -s -C examples/empty/am64x-evm/r5fss0-0_freertos/ti-arm-clang [clean syscfg-gui syscfg]"
	@echo
	@echo  "Note that PRU firmware should be built before any RTOS code that includes"
	@echo  "the PRU firmware."
	@echo

.PHONY: all clean pru host libs libs-clean help academy examples source
