export OPEN_PRU_PATH?=$(abspath ../..)
include $(OPEN_PRU_PATH)/imports.mak

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

# Debug: no optimization (-O0) for faster compile/debug; Release: size-optimized (-Os)
CFLAGS_OPT_debug := -O0
CFLAGS_OPT_release := -Os
CFLAGS_OPT := $(CFLAGS_OPT_$(PROFILE))

CFLAGS_common := \
	-mcpu=cortex-r5 \
	-mfloat-abi=hard \
	-mfpu=vfpv3-d16 \
	-mthumb \
	-Wall \
	-Werror \
	-g \
	-Wno-gnu-variable-sized-type-not-at-end \
	-Wno-unused-function \
	$(CFLAGS_OPT)

INCLUDES_common := \
	-I$(MCU_PLUS_SDK_PATH)/source \
	-I$(OPEN_PRU_PATH)/source \
	-I$(OPEN_PRU_PATH)/source/i2s \
	-I$(OPEN_PRU_PATH)/examples/pru_i2s/firmware \
	-I$(OPEN_PRU_PATH)/examples \
	-I$(MCU_PLUS_SDK_PATH)/source/kernel/freertos/FreeRTOS-Kernel/include \
	-I$(MCU_PLUS_SDK_PATH)/source/kernel/freertos/portable/TI_ARM_CLANG/ARM_CR5F

# Default library profile is Release (uppercase) so library names match linker refs
PROFILE?=Release
GEN_DIR := $(OPEN_PRU_PATH)/examples/pru_i2s/lib

# Only build if MCU+ SDK is available
ifeq ($(BUILD_MCUPLUS),y)

all: build_am261x build_am263x

build_am261x:
	$(MKDIR) $(GEN_DIR)
	@echo "Building pru_i2s library for am261x..."
	$(CC) -c $(CFLAGS_common) \
		$(INCLUDES_common) \
		-I$(MCU_PLUS_SDK_PATH)/source/kernel/freertos/config/am261x/r5f \
		-DSOC_AM261X -DOS_FREERTOS \
		-o $(GEN_DIR)/pru_i2s_drv_am261x.obj pru_i2s_drv.c
	$(CC) -c $(CFLAGS_common) \
		$(INCLUDES_common) \
		-I$(MCU_PLUS_SDK_PATH)/source/kernel/freertos/config/am261x/r5f \
		-DSOC_AM261X -DOS_FREERTOS \
		-o $(GEN_DIR)/pru_i2s_app_config_am261x.obj pru_i2s_app_config.c
	$(AR) rc $(GEN_DIR)/pru_i2s.am261x.r5f.ti-arm-clang.$(PROFILE).lib \
		$(GEN_DIR)/pru_i2s_drv_am261x.obj \
		$(GEN_DIR)/pru_i2s_app_config_am261x.obj
	$(RM) $(GEN_DIR)/pru_i2s_drv_am261x.obj
	$(RM) $(GEN_DIR)/pru_i2s_app_config_am261x.obj
	@echo "Library build complete."

build_am263x:
	$(MKDIR) $(GEN_DIR)
	@echo "Building pru_i2s library for am263x..."
	$(CC) -c $(CFLAGS_common) \
		$(INCLUDES_common) \
		-I$(MCU_PLUS_SDK_PATH)/source/kernel/freertos/config/am263x/r5f \
		-DSOC_AM263X -DOS_FREERTOS \
		-o $(GEN_DIR)/pru_i2s_drv_am263x.obj pru_i2s_drv.c
	$(CC) -c $(CFLAGS_common) \
		$(INCLUDES_common) \
		-I$(MCU_PLUS_SDK_PATH)/source/kernel/freertos/config/am263x/r5f \
		-DSOC_AM263X -DOS_FREERTOS \
		-o $(GEN_DIR)/pru_i2s_app_config_am263x.obj pru_i2s_app_config.c
	$(AR) rc $(GEN_DIR)/pru_i2s.am263x.r5f.ti-arm-clang.$(PROFILE).lib \
		$(GEN_DIR)/pru_i2s_drv_am263x.obj \
		$(GEN_DIR)/pru_i2s_app_config_am263x.obj
	$(RM) $(GEN_DIR)/pru_i2s_drv_am263x.obj
	$(RM) $(GEN_DIR)/pru_i2s_app_config_am263x.obj
	@echo "Library build complete."

clean:
	-$(RMDIR) $(GEN_DIR)
	@echo "Finished clean"

else
all clean:
	@echo "Skipping pru_i2s library build: BUILD_MCUPLUS != y"
endif

.PHONY: all build_am261x build_am263x clean
