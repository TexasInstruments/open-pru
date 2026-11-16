include ../../../imports.mak

PROJECT_NAME := jpeg_host
SUPPORTED_PROCESSORS := am243x am261x

ifeq (,$(filter $(DEVICE),$(SUPPORTED_PROCESSORS)))
all host clean:
	@echo $(PROJECT_NAME) does not support $(DEVICE)
else
ifeq ($(BUILD_MCUPLUS),y)
all host:
	$(MAKE) -C mcuplus/$(DEVICE)-lp/r5fss0-0_freertos/ti-arm-clang all

clean:
	$(MAKE) -C mcuplus/$(DEVICE)-lp/r5fss0-0_freertos/ti-arm-clang scrub
else
all host clean:
	@echo $(PROJECT_NAME) requires BUILD_MCUPLUS=y
endif
endif

.PHONY: all clean host pru

pru:
	@echo $(PROJECT_NAME) contains R5F code only
