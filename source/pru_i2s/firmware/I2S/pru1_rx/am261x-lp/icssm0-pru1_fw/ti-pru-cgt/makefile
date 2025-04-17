################################################################################
# Automatically-generated file. Do not edit!
################################################################################
#   Required input arguments :
#   CCS_PROJECT_DEBUG=${CWD}   Use this input argument to include files from external paths

export OPEN_PRU_PATH?=$(abspath ../../../../../../../..)
include $(OPEN_PRU_PATH)/imports.mak
SYSCFG_DIR := $(CCS_PROJECT_DEBUG)/syscfg
CG_TOOL_ROOT := $(CGT_TI_PRU_PATH)

# Add inputs and outputs from these tool invocations to the build variables
CMD_SRCS += \
	pru_i2s_master_icss.cmd \

ASM_SRCS += \
	pru_i2s_main.asm \
	fw_regs.asm \

OBJS += \
	pru_i2s_main.obj \
	fw_regs.obj \

ASM_DEPS += \
	pru_i2s_main.d \
	fw_regs.d \

OBJDIR := .

FILES_PATH_common = \
	.. \
	../../.. \
	../../../.. \

FILES_PATH := $(FILES_PATH_common)

vpath %.asm $(FILES_PATH)
vpath %.obj $(OBJDIR)
vpath %.cmd $(FILES_PATH)


# Add inputs and outputs from these tool invocations to the build variables
EXE_OUTPUTS += \
pru_i2s_pru1_array_am261x-lp_icssm0-pru1_fw_ti-pru-cgt.out \

EXE_OUTPUTS__QUOTED += \
"pru_i2s_pru1_array_am261x-lp_icssm0-pru1_fw_ti-pru-cgt.out" \

BIN_OUTPUTS += \
pru_i2s_pru1_array_am261x-lp_icssm0-pru1_fw_ti-pru-cgt.hex \

BIN_OUTPUTS__QUOTED += \
"pru_i2s_pru1_array_am261x-lp_icssm0-pru1_fw_ti-pru-cgt.hex" \

# All Target
all: $(OBJS) $(CMD_SRCS)
	@$(MAKE) --no-print-directory -Onone "pru_i2s_pru1_array_am261x-lp_icssm0-pru1_fw_ti-pru-cgt.out"

# Each subdirectory must supply rules for building sources it contributes
$(OBJDIR)/%.obj %.obj: %.asm
	@echo 'Building file: "$^"'
	@echo 'Invoking: PRU Compiler'
	"$(CGT_TI_PRU_PATH)/bin/clpru" -DPRU1 -DSLICE1 -v4 --define=SOC_AM261X --define=PRU1 --define=I2S_RX --define=I2S_RX_DETECT_OVERFLOW --include_path="${CG_TOOL_ROOT}/include" --include_path="${MCU_PLUS_SDK_PATH}/source" --include_path="${OPEN_PRU_PATH}/source" --include_path="${OPEN_PRU_PATH}/source/pru_i2s/firmware/I2S"  --define=_DEBUG_=1 -g --diag_warning=225 --diag_wrap=off --display_error_number --endian=little --preproc_with_compile --preproc_dependency="$(basename $(<F)).d_raw" $^
	@echo 'Finished building: "$^"'
	@echo ' '

# Tool invocations
pru_i2s_pru1_array_am261x-lp_icssm0-pru1_fw_ti-pru-cgt.out: $(OBJS) $(CMD_SRCS)
	@echo 'Building target: "$@"'
	@echo 'Invoking: PRU Linker'
	"$(CGT_TI_PRU_PATH)/bin/clpru" -DPRU1 -DSLICE1 -v4 --define=SOC_AM261X --define=PRU1 --define=I2S_RX --define=I2S_RX_DETECT_OVERFLOW  --define=_DEBUG_=1 -g --diag_warning=225 --diag_wrap=off --display_error_number --endian=little -z -m"pru_i2s_pru1_array_am261x-lp_icssm0-pru1_fw_ti-pru-cgt.map" --disable_auto_rts   --diag_wrap=off --display_error_number --warn_sections --xml_link_info="pru_i2s_pru1_array_am261x-lp_icssm0-pru1_fw_ti-pru-cgt_linkInfo.xml" --disable_auto_rts --disable_auto_rts --diag_suppress=10063-D --entry_point=main  --rom_model -o "pru_i2s_pru1_array_am261x-lp_icssm0-pru1_fw_ti-pru-cgt.out" $^
	@echo 'Finished building target: "$@"'
	@echo ' '
	@$(MAKE) --no-print-directory post-build

# To clean generated files
clean:
	-$(RM) $(BIN_OUTPUTS__QUOTED)$(EXE_OUTPUTS__QUOTED)
	-$(RM) $(OBJS)
	-$(RM) $(ASM_DEPS)
	-@echo 'Finished clean'
	-@echo ' '

post-build:
		-$(CG_TOOL_ROOT)/bin/hexpru --diag_wrap=off --array --array:name_prefix=pru_prupru_i2s1_image_0 -o pru_i2s_pru1_array.h  pru_i2s_pru1_array_am261x-lp_icssm0-pru1_fw_ti-pru-cgt.out
		-$(CAT) ${OPEN_PRU_PATH}/mcu_plus_sdk/source/pru_io/firmware/pru_load_bin_copyright.h pru_i2s_pru1_array.h > ${OPEN_PRU_PATH}/source/pru_i2s/firmware/I2S/pru_i2s_pru1_array.h
		-$(RM) pru_i2s_pru1_array.h
	-@echo ' '

.PHONY: all clean
