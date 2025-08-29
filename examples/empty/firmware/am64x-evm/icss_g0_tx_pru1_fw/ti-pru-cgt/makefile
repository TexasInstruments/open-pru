export OPEN_PRU_PATH?=$(abspath ../../../../../..)
include $(OPEN_PRU_PATH)/imports.mak

# Add inputs and outputs from these tool invocations to the build variables
CMD_SRCS += \
	linker.cmd \

ASM_SRCS += \
	main.asm \

OUTPUT_NAME := empty_am64x-evm_icss_g0_tx_pru1_fw_ti-pru-cgt
GEN_DIR := generated
TARGET := $(GEN_DIR)/$(OUTPUT_NAME).out
MAP := $(GEN_DIR)/$(OUTPUT_NAME).map
XML_LINK_INFO := $(GEN_DIR)/$(OUTPUT_NAME)_linkInfo.xml

OBJ_DIR := $(GEN_DIR)
SYSCFG_DIR := ./syscfg
FILES_PATH := .. ../../.. .
vpath %.asm $(FILES_PATH)
vpath %.cmd $(FILES_PATH)
vpath %.obj $(OBJ_DIR)
OBJS += $(GEN_DIR)/main.obj

# All Target
all: $(OBJS) $(CMD_SRCS)
	@$(MAKE) --no-print-directory -Onone "$(TARGET)"

# Each subdirectory must supply rules for building sources it contributes
$(OBJ_DIR)/%.obj %.obj: %.asm
	@mkdir -p $(GEN_DIR)
	@echo 'Building file: "$^"'
	@echo 'Invoking: PRU Compiler'
	"$(CGT_TI_PRU_PATH)/bin/clpru" -DTX_PRU1 -DSLICE1 -v4 --define=SOC_AM64X --include_path="${CGT_TI_PRU_PATH}/include" --include_path="${MCU_PLUS_SDK_PATH}/source" --include_path="${OPEN_PRU_PATH}/source" --include_path="${MCU_PLUS_SDK_PATH}/source/pru_io/firmware/common"  --define=_DEBUG_=1 -g --diag_warning=225 --diag_wrap=off --display_error_number --endian=little --asm_directory=$(GEN_DIR) --obj_directory=$(GEN_DIR) --pp_directory=$(GEN_DIR) -ppd -ppa $^
	@echo 'Finished building: "$^"'
	@echo ' '

# Tool invocations
$(TARGET): $(OBJS) $(CMD_SRCS)
	@echo 'Building target: "$@"'
	@echo 'Invoking: PRU Linker'
	"$(CGT_TI_PRU_PATH)/bin/clpru" -DTX_PRU1 -DSLICE1 -v4 --define=SOC_AM64X  --define=_DEBUG_=1 -g --diag_warning=225 --diag_wrap=off --display_error_number --endian=little -z -m$(MAP) --disable_auto_rts   --diag_wrap=off --display_error_number --warn_sections --xml_link_info=$(XML_LINK_INFO) --disable_auto_rts --entry_point=main --diag_suppress=10063-D  --rom_model -o $(TARGET) $^
	@echo 'Finished building target: "$@"'
	@echo ' '
	@$(MAKE) --no-print-directory post-build

# To clean generated files
clean:
	-$(RMDIR) $(GEN_DIR)
	-@echo 'Finished clean'
	-@echo ' '

post-build:
		-$(CGT_TI_PRU_PATH)/bin/hexpru --diag_wrap=off --array --array:name_prefix=TXPRU1Firmware  -o txpru1_load_bin.h $(TARGET)
		-$(CAT)  ${MCU_PLUS_SDK_PATH}/source/pru_io/firmware/pru_load_bin_copyright.h txpru1_load_bin.h > ${OPEN_PRU_PATH}/examples/empty/firmware/am64x-evm/txpru1_load_bin.h 
		-$(RM) txpru1_load_bin.h
	-@echo ' '

.PHONY: all clean
