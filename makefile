OPEN_PRU_PATH ?= $(abspath .)
# Default mcu_plus_sdk path
MCU_PLUS_SDK_PATH ?= C:\ti\mcu_plus_sdk

include imports.mak

# Default device
DEVICE ?= am64x

# debug, release
PROFILE?=release

# GP, HS
DEVICE_TYPE?=GP

ifeq ($(DEVICE),$(filter $(DEVICE), am261x))
    SYSCFG_DEVICE = AM261x_ZCZ
    # default syscfg CPU to use,
    # options on am261x are r5fss0-0, r5fss0-1
    SYSCFG_CPU = r5fss0-0
  endif
ifeq ($(DEVICE),$(filter $(DEVICE), am263px))
    SYSCFG_DEVICE = AM263Px
    # default syscfg CPU to use,
    # options on am263px are r5fss0-0, r5fss0-1, r5fss1-0, r5fss1-1
    SYSCFG_CPU = r5fss0-0
  endif
ifeq ($(DEVICE),$(filter $(DEVICE), am64x))
  SYSCFG_DEVICE = AM64x
  # default syscfg CPU to use,
  # options on am64x are r5fss0-0, r5fss0-1, r5fss1-0, r5fss1-1, m4fss0-0
  SYSCFG_CPU = r5fss0-0
endif
ifeq ($(DEVICE),$(filter $(DEVICE), am243x))
  SYSCFG_DEVICE = AM243x_ALV_beta
  # default syscfg CPU to use,
  # options on am64x are r5fss0-0, r5fss0-1, r5fss1-0, r5fss1-1, m4fss0-0
  SYSCFG_CPU = r5fss0-0
endif
ifeq ($(DEVICE),$(filter $(DEVICE), am263x))
  SYSCFG_DEVICE = AM263x_beta
  # default syscfg CPU to use,
  # options on am263x are r5fss0-0, r5fss0-1, r5fss1-0, r5fss1-1
  SYSCFG_CPU = r5fss0-0
endif
ifeq ($(DEVICE),$(filter $(DEVICE), am273x))
  SYSCFG_DEVICE = AM273x
  # default syscfg CPU to use,
  # options on am273x are r5fss0-0, r5fss0-1, c66ss0
  SYSCFG_CPU = r5fss0-0
endif
ifeq ($(DEVICE),$(filter $(DEVICE), awr294x))
  SYSCFG_DEVICE = AWR294X
  # default syscfg CPU to use,
  # options on awr294x are r5fss0-0, r5fss0-1, c66ss0
  SYSCFG_CPU = r5fss0-0
endif
ifeq ($(DEVICE),$(filter $(DEVICE), am62x))
  SYSCFG_DEVICE = AM62x
  # default syscfg CPU to use,
  # options on am62x are m4fss0-0
  SYSCFG_CPU = m4fss0-0
endif
ifeq ($(DEVICE),$(filter $(DEVICE), am65x))
  SYSCFG_DEVICE = AM65xx_SR2.0_beta
  # default syscfg CPU to use,
  # options on am65x are r5fss0-0, r5fss0-1
  SYSCFG_CPU = r5fss0-0
endif
all:
	$(MAKE) -C . -f makefile.$(DEVICE) all PROFILE=$(PROFILE)

clean:
	$(MAKE) -C . -f makefile.$(DEVICE) clean PROFILE=$(PROFILE)

scrub:
	$(MAKE) -C . -f makefile.$(DEVICE) scrub PROFILE=$(PROFILE)

scrub_gcc:
	$(MAKE) -C . -f makefile.$(DEVICE) scrub_gcc PROFILE=$(PROFILE)

libs:
	$(MAKE) -C . -f makefile.$(DEVICE) libs PROFILE=$(PROFILE) DEVICE_TYPE=$(DEVICE_TYPE)

libs-clean:
	$(MAKE) -C . -f makefile.$(DEVICE) libs-clean PROFILE=$(PROFILE)

libs-scrub:
	$(MAKE) -C . -f makefile.$(DEVICE) libs-scrub PROFILE=$(PROFILE)

examples:
	$(MAKE) -C . -f makefile.$(DEVICE) examples PROFILE=$(PROFILE)

examples-clean:
	$(MAKE) -C . -f makefile.$(DEVICE) examples-clean PROFILE=$(PROFILE)

examples-scrub:
	$(MAKE) -C . -f makefile.$(DEVICE) examples-scrub PROFILE=$(PROFILE)

help:
	$(MAKE) -C . -f makefile.$(DEVICE) -s help PROFILE=$(PROFILE)

sbl:
	$(MAKE) -C . -f makefile.$(DEVICE) sbl PROFILE=$(PROFILE)

sbl-hs:
ifeq ($(DEVICE),$(filter $(DEVICE), am243x am64x))
	$(MAKE) -C . -f makefile.$(DEVICE) sbl-hs PROFILE=$(PROFILE)
else
	@echo sbl-hs target is not supported for this device
endif

sbl-clean:
	$(MAKE) -C . -f makefile.$(DEVICE) sbl-clean PROFILE=$(PROFILE)

sbl-scrub:
	$(MAKE) -C . -f makefile.$(DEVICE) sbl-scrub PROFILE=$(PROFILE)

syscfg-gui:
	$(SYSCFG_NWJS) $(SYSCFG_PATH) --product $(SYSCFG_SDKPRODUCT) --product $(SYSCFG_MCU_PLUS_SDK_PRODUCT) --device $(SYSCFG_DEVICE) --context $(SYSCFG_CPU)

devconfig:
	$(SYSCFG_NWJS) $(SYSCFG_PATH) --product $(MCU_PLUS_SDK_PATH)/devconfig/devconfig.json --device $(SYSCFG_DEVICE) --context $(SYSCFG_CPU) --output devconfig/ $(MCU_PLUS_SDK_PATH)/devconfig/devconfig.syscfg

.PHONY: all clean scrub
.PHONY: libs libs-clean libs-scrub
.PHONY: examples examples-clean examples-scrub
.PHONY: help
.PHONY: sbl sbl-clean sbl-scrub
.PHONY: syscfg-gui
.PHONY: devconfig


################ Internal make targets - not to be used by customers ################
NODE=node
################ GEN_BUILDFILES_TARGET is used by only mcu plus sdk product #########
GEN_BUILDFILES_TARGET?=production
INSTRUMENTATION_MODE?=disable

DOC_COMBO = r5f.ti-arm-clang
# default combo for doc generation
ifeq ($(DEVICE),$(filter $(DEVICE), am62x))
  DOC_COMBO = m4f.ti-arm-clang
endif

projectspec-help:
	$(MAKE) -C . -f makefile_projectspec.$(DEVICE) -s help PROFILE=$(PROFILE)

docs:
	$(MAKE) -C docs_src/docs/api_guide all DEVICE=$(DEVICE) DOC_COMBO=$(DOC_COMBO)
	@echo "<script id=\"searchdata\" type=\"text/xmldata\">" >> ./docs/api_guide_$(DEVICE)/search.html
	$(COPY) docs_src/docs/api_guide/search.js ./docs/api_guide_$(DEVICE)/search/search.js
	$(CAT) ./docs/api_guide_$(DEVICE)/searchdata.xml >> ./docs/api_guide_$(DEVICE)/search.html
	@echo "</script>" >> ./docs/api_guide_$(DEVICE)/search.html

docs-clean:
	$(MAKE) -C docs_src/docs/api_guide clean DEVICE=$(DEVICE) DOC_COMBO=$(DOC_COMBO)

gen-buildfiles:
	$(NODE) ./.project/project.js --device $(DEVICE) --target $(GEN_BUILDFILES_TARGET) --instrumentation $(INSTRUMENTATION_MODE) --mcu_plus_sdk_path $(MCU_PLUS_SDK_PATH)

gen-buildfiles-clean:
	$(NODE) ./.project/project.js --device $(DEVICE) --target clean

tests: libs
	$(MAKE) -C test -f makefile.$(DEVICE) all PROFILE=$(PROFILE)

tests-clean:
	$(MAKE) -C test -f makefile.$(DEVICE) clean PROFILE=$(PROFILE)

tests-scrub:
	$(MAKE) -C test -f makefile.$(DEVICE) scrub PROFILE=$(PROFILE)

tests-libs: libs
	$(MAKE) -C test -f makefile.$(DEVICE) libs PROFILE=$(PROFILE)

tests-libs-clean:
	$(MAKE) -C test -f makefile.$(DEVICE) libs-clean PROFILE=$(PROFILE)

tests-libs-scrub:
	$(MAKE) -C test -f makefile.$(DEVICE) libs-scrub PROFILE=$(PROFILE)

syscfg-tests:
ifeq ($(DEVICE),$(filter $(DEVICE), am64x))
	-$(SYSCFG_NODE) $(SYSCFG_CLI_PATH)/tests/sanityTests.js -s $(SYSCFG_SDKPRODUCT) -d $(SYSCFG_DEVICE) -c a53ss0-0 --excludeTests="migrateToAnyTarget"
endif
ifeq ($(DEVICE),$(filter $(DEVICE), am64x am243x am62x))
	-$(SYSCFG_NODE) $(SYSCFG_CLI_PATH)/tests/sanityTests.js -s $(SYSCFG_SDKPRODUCT) -d $(SYSCFG_DEVICE) -c m4fss0-0 --excludeTests="migrateToAnyTarget"
endif
	-$(SYSCFG_NODE) $(SYSCFG_CLI_PATH)/tests/sanityTests.js -s $(SYSCFG_SDKPRODUCT) -d $(SYSCFG_DEVICE) -c r5fss0-0 --excludeTests="migrateToAnyTarget"
ifeq ($(DEVICE),$(filter $(DEVICE), am273x awr294x))
	-$(SYSCFG_NODE) $(SYSCFG_CLI_PATH)/tests/sanityTests.js -s $(SYSCFG_SDKPRODUCT) -d $(SYSCFG_DEVICE) -c c66ss0 --excludeTests="migrateToAnyTarget"
endif

.PHONY: projectspec-help docs docs-clean
.PHONY: gen-buildfiles gen-buildfiles-clean
.PHONY: tests tests-clean tests-scrub tests-libs tests-libs-clean tests-libs-scrub
.PHONY: syscfg-tests
