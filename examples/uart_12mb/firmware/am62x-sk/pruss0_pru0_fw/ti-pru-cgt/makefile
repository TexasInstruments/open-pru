export OPEN_PRU_PATH?=$(abspath ../../../../../..)
include $(OPEN_PRU_PATH)/imports.mak

# Define build outputs
OUTPUT_NAME := uart_12mb_am62x-sk_pruss0_pru0_fw
HEX_ARRAY_PREFIX := PRU0Firmware
GEN_DIR := generated
TARGET := $(GEN_DIR)/$(OUTPUT_NAME).out
TARGET_HEX := $(GEN_DIR)/$(OUTPUT_NAME).h

# MCU+ projects: rename & move hex array output
MCU_HEX_NAME := pru0_load_bin.h
MCU_HEX_PATH := $(OPEN_PRU_PATH)/examples/uart_12mb/firmware/am62x-sk/$(MCU_HEX_NAME)

# which directories to search for assembly & C source files?
FILES_PATH := .. ../../.. .
vpath %.asm $(FILES_PATH)
vpath %.c $(FILES_PATH)
vpath %.cmd $(FILES_PATH)
vpath %.obj $(GEN_DIR)
C_FILES := $(notdir $(foreach directory, $(FILES_PATH), $(wildcard $(directory)/*.c)))
ASM_FILES := $(notdir $(foreach directory, $(FILES_PATH), $(wildcard $(directory)/*.asm)))

# Generate an object file for every *.asm and *.c source file in FILES_PATH
OBJECTS := $(patsubst %.asm, $(GEN_DIR)/%.obj, $(notdir \
	$(foreach directory, $(FILES_PATH), $(wildcard $(directory)/*.asm))))
OBJECTS += $(patsubst %.c, $(GEN_DIR)/%.obj, $(notdir \
	$(foreach directory, $(FILES_PATH), $(wildcard $(directory)/*.c))))

# Linker command file
COMMAND_FILES := linker.cmd

# Include paths
INCLUDE := \
	--include_path=$(CGT_TI_PRU_PATH)/include \
	--include_path=$(OPEN_PRU_PATH)/source \
	--include_path=$(OPEN_PRU_PATH)/source/include/c_code/linux \
	--include_path=$(OPEN_PRU_PATH)/source/include/c_code/am62x \
	--include_path=.
ifeq ($(BUILD_MCUPLUS),y)
INCLUDE += \
	--include_path=$(MCU_PLUS_SDK_PATH)/source \
	--include_path=$(MCU_PLUS_SDK_PATH)/source/pru_io/firmware/common
endif

# Compiler flags (Defined in 'PRU Optimizing C/C++ Compiler User's Guide)
# commonly modified flags include:
#     silicon version: -v (3: PRUSS, PRU-ICSS; 4: PRU_ICSSG)
#     C code optimization: -O (off, 0, 1, 2, 3, 4)
CFLAGS := \
	-v3 -O2 \
	--display_error_number --diag_wrap=off --diag_warning=225 \
	--asm_directory=$(GEN_DIR) --obj_directory=$(GEN_DIR) --pp_directory=$(GEN_DIR) \
	-ppd -ppa -g --endian=little

# Linker flags (Defined in 'PRU Optimizing C/C++ Compiler User's Guide)
LFLAGS := \
	-m$(GEN_DIR)/$(OUTPUT_NAME).map --xml_link_info=$(GEN_DIR)/$(OUTPUT_NAME)_linkInfo.xml \
	--display_error_number --diag_wrap=off --diag_warning=225 --diag_suppress=10063-D \
	--warn_sections --reread_libs

ifeq ($(C_FILES),)
# PRU firmware is assembly-only
# commonly modified flags include:
#     entry point
LFLAGS += \
	--entry_point=main --disable_auto_rts --ram_model

else
# PRU firmware has C code
# commonly modified flags include:
#     stack size, heap size
LFLAGS += \
	--stack_size=0x100 --heap_size=0x100 \
	-i$(CGT_TI_PRU_PATH)/lib -i$(CGT_TI_PRU_PATH)/include --library=libc.a
endif

# Defines (pass instance specific definitions to the program)
# see --define or -D in 'PRU Optimizing C/C++ Compiler User's Guide'
# For example, to define PRU0, ICSSG1, SOC_AM243X, and set _DEBUG_=1:
# -DPRU0 -DICSSG1 --define=SOC_AM243X -D_DEBUG_=1
DFLAGS :=

# Libraries
# see --library in 'PRU Optimizing C/C++ Compiler User's Guide'
LIBS :=

# Only build if tools are defined
ifeq (,$(CGT_TI_PRU_PATH))
all clean:
	@echo 'CGT_TI_PRU_PATH not set. Make sure that:'
	@echo ' 1) OPEN_PRU_PATH points to the open-pru repo'
	@echo ' 2) imports.mak exists at OPEN_PRU_PATH'
	@echo ' 3) Both the open-pru repo and imports.mak are set up as per'
	@echo '    OPEN_PRU_PATH/docs/getting_started.md'
else

# All Target
all: $(OBJECTS) $(COMMAND_FILES)
	@$(MAKE) --no-print-directory -Onone "$(TARGET)"

# Invoke the compiler on all assembly files in vpath to create the object files
$(GEN_DIR)/%.obj: %.asm
	$(MKDIR) $(GEN_DIR)
	@echo 'Building file: "$^"'
	@echo 'Invoking: PRU Compiler'
	"$(CGT_TI_PRU_PATH)/bin/clpru" $(INCLUDE) $(CFLAGS) $(DFLAGS) $^
	@echo 'Finished building: "$^"'
	@echo ' '

# Invoke the compiler on all c files in vpath to create the object files
$(GEN_DIR)/%.obj: %.c
	$(MKDIR) $(GEN_DIR)
	@echo 'Building file: $^'
	@echo 'Invoking: PRU Compiler'
	"$(CGT_TI_PRU_PATH)/bin/clpru" $(INCLUDE) $(CFLAGS) $(DFLAGS) $^
	@echo 'Finished building: "$^"'
	@echo ' '

# Invoke the linker (-z flag) to make the .out file
$(TARGET): $(OBJECTS) $(COMMAND_FILES)
	@echo 'Building target: "$@"'
	@echo 'Invoking: PRU Linker'
	"$(CGT_TI_PRU_PATH)/bin/clpru" $(CFLAGS) -z $^ $(LFLAGS) -o $(TARGET) $(LIBS)
	@echo 'Finished building target: "$@"'
	@echo ' '
	@$(MAKE) --no-print-directory post-build

# To clean generated files
clean:
	-$(RMDIR) $(GEN_DIR)
ifeq ($(BUILD_MCUPLUS),y)
	-$(RM) $(MCU_HEX_PATH)
endif
	@echo 'Finished clean'
	@echo ' '
endif

# Generate hex array
post-build:
	-$(CGT_TI_PRU_PATH)/bin/hexpru --diag_wrap=off --array --array:name_prefix=$(HEX_ARRAY_PREFIX) -o $(TARGET_HEX) $(TARGET)
	-$(CAT) $(OPEN_PRU_PATH)/source/firmware/pru_load_bin_copyright.h $(TARGET_HEX) > $(TARGET_HEX).temp
	-$(MOVE) $(TARGET_HEX).temp $(TARGET_HEX)
ifeq ($(BUILD_MCUPLUS),y)
	-$(COPY) $(TARGET_HEX) $(MCU_HEX_PATH)
endif
	@echo ' '

.PHONY: all clean

# Include the dependencies that the compiler creates (-ppd and -ppa flags)
-include $(OBJECTS:%.obj=%.pp)
