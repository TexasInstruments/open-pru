################################################################################
# Automatically-generated file. Do not edit!
################################################################################
#   Required input arguments :
#   CCS_PROJECT_DEBUG=${CWD}   Use this input argument to include files from external paths

export OPEN_PRU_PATH?=$(abspath ../../../../../..)
include $(OPEN_PRU_PATH)/imports.mak
SYSCFG_DIR := $(CCS_PROJECT_DEBUG)/syscfg
CG_TOOL_ROOT := $(CGT_TI_PRU_PATH)

# Add inputs and outputs from these tool invocations to the build variables
CMD_SRCS += \
	AM261x_PRU0.cmd \

ASM_SRCS += \
	butterfly_length_2.asm \
	fft_windowed_4k.asm \
	window_function_4k.asm \
	lut_load.asm \
	main.asm \

OBJS += \
	butterfly_length_2.obj \
	fft_windowed_4k.obj \
	window_function_4k.obj \
	lut_load.obj \
	main.obj \

ASM_DEPS += \
	butterfly_length_2.d \
	fft_windowed_4k.d \
	window_function_4k.d \
	lut_load.d \
	main.d \

OBJDIR := .

FILES_PATH_common = \
	.. \
	../../.. \
	. \

FILES_PATH := $(FILES_PATH_common)

vpath %.asm $(FILES_PATH)
vpath %.obj $(OBJDIR)
vpath %.cmd $(FILES_PATH)


# Add inputs and outputs from these tool invocations to the build variables
EXE_OUTPUTS += \
split_radix_fft_4k_single_core_am261x-lp_icss_m0_pru0_fw_ti-pru-cgt.out \

EXE_OUTPUTS__QUOTED += \
"split_radix_fft_4k_single_core_am261x-lp_icss_m0_pru0_fw_ti-pru-cgt.out" \

BIN_OUTPUTS += \
split_radix_fft_4k_single_core_am261x-lp_icss_m0_pru0_fw_ti-pru-cgt.hex \

BIN_OUTPUTS__QUOTED += \
"split_radix_fft_4k_single_core_am261x-lp_icss_m0_pru0_fw_ti-pru-cgt.hex" \

# All Target
all: $(OBJS) $(CMD_SRCS)
	@$(MAKE) --no-print-directory -Onone "split_radix_fft_4k_single_core_am261x-lp_icss_m0_pru0_fw_ti-pru-cgt.out"

# Each subdirectory must supply rules for building sources it contributes
$(OBJDIR)/%.obj %.obj: %.asm
	@echo 'Building file: "$^"'
	@echo 'Invoking: PRU Compiler'
	"$(CGT_TI_PRU_PATH)/bin/clpru" -DPRU0 -DSLICE0 -v4 --define=SOC_AM261X --include_path="${CG_TOOL_ROOT}/include" --include_path="${MCU_PLUS_SDK_PATH}/source" --include_path="${OPEN_PRU_PATH}/source" --include_path="${MCU_PLUS_SDK_PATH}/source/pru_io/firmware/common"  --define=_DEBUG_=1 -g --diag_warning=225 --diag_wrap=off --display_error_number --endian=little --preproc_with_compile --preproc_dependency="$(basename $(<F)).d_raw" $^
	@echo 'Finished building: "$^"'
	@echo ' '

# Tool invocations
split_radix_fft_4k_single_core_am261x-lp_icss_m0_pru0_fw_ti-pru-cgt.out: $(OBJS) $(CMD_SRCS)
	@echo 'Building target: "$@"'
	@echo 'Invoking: PRU Linker'
	"$(CGT_TI_PRU_PATH)/bin/clpru" -DPRU0 -DSLICE0 -v4 --define=SOC_AM261X  --define=_DEBUG_=1 -g --diag_warning=225 --diag_wrap=off --display_error_number --endian=little -z -m"split_radix_fft_4k_single_core_am261x-lp_icss_m0_pru0_fw_ti-pru-cgt.map" --disable_auto_rts   --diag_wrap=off --display_error_number --warn_sections --xml_link_info="split_radix_fft_4k_single_core_am261x-lp_icss_m0_pru0_fw_ti-pru-cgt_linkInfo.xml" --disable_auto_rts --entry_point=main --diag_suppress=10063-D  --rom_model -o "split_radix_fft_4k_single_core_am261x-lp_icss_m0_pru0_fw_ti-pru-cgt.out" $^
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
	-@echo ' '

.PHONY: all clean
