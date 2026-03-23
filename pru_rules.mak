# Shared build settings for all PRU/RTU/TX_PRU core makefiles
#
# Before including this file, define:
#   OUTPUT_NAME      - base name for .out and .h outputs
#   MCU_HEX_NAME     - filename for MCU+ hex array (e.g. pru0_load_bin.h)
#   HEX_ARRAY_PREFIX - array name prefix for hexpru (e.g. PRU0Firmware)
#   MCU_HEX_PATH     - full destination path for MCU+ hex array
#   FILES_PATH       - directories to search for source files
#   COMMAND_FILES    - linker command file(s)
#   PRU_VERSION      - silicon version (3: PRUSS, PRU-ICSS; 4: PRU_ICSSG)
#   EXPECTED_DEVICE  - device this firmware targets (e.g. am261x); validated
#                      against DEVICE from imports.mak to catch mismatches
#
# Optional: Before including this file, define to override defaults in:
# Compiler flags (CFLAGS):
#   OPT_LEVEL        - C code optimization: off, 0, 1, 2, 3, 4 (default: 2)
# Linker flags (LFLAGS):
#   STACK_SIZE       - C code stack size in bytes (default: 0x100)
#   HEAP_SIZE        - C code heap size in bytes (default: 0x100)
#   ENTRY_POINT      - Assembly code entry point (default: main)
#
# Optional: After including this file, append values to:
#   INCLUDE          - additional project-specific include paths
#   CFLAGS           - additional compiler flags
#   LFLAGS           - additional linker flags
#   DFLAGS           - project-specific compiler defines
#   LIBS             - project-specific libraries

# Validate DEVICE
# DEVICE from imports.mak should match EXPECTED_DEVICE declared in the
# core makefile.  Fail fast here so a wrong/missing DEVICE is caught before
# the compiler silently picks the wrong device-specific headers.
VALID_DEVICES := am243x am64x am261x am263x am263px am62x
ifeq (,$(DEVICE))
$(error DEVICE is not set. Set DEVICE in imports.mak (valid values: $(VALID_DEVICES)))
endif
ifeq (,$(filter $(DEVICE),$(VALID_DEVICES)))
$(error DEVICE='$(DEVICE)' is not recognised. Update DEVICE in imports.mak (valid values: $(VALID_DEVICES)))
endif
ifneq ($(DEVICE),$(EXPECTED_DEVICE))
$(error DEVICE='$(DEVICE)' but this firmware targets $(EXPECTED_DEVICE). Update DEVICE in imports.mak)
endif

# Build outputs
GEN_DIR := generated
TARGET := $(GEN_DIR)/$(OUTPUT_NAME).out
TARGET_HEX := $(GEN_DIR)/$(OUTPUT_NAME).h

# Standard include paths for all PRU firmware
INCLUDE := \
	--include_path=$(CGT_TI_PRU_PATH)/include \
	--include_path=$(OPEN_PRU_PATH)/source \
	--include_path=$(OPEN_PRU_PATH)/source/firmware/common \
	--include_path=$(OPEN_PRU_PATH)/source/include \
	--include_path=$(OPEN_PRU_PATH)/source/include/$(DEVICE)

# Search for assembly & C source files
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

# Compiler flags (see 'PRU Optimizing C/C++ Compiler User's Guide')
#   PRU_VERSION must be set in the core's makefile
#     silicon version: -v (3: PRUSS, PRU-ICSS; 4: PRU_ICSSG)
#   OPT_LEVEL may be overridden in the core's makefile
#     C code optimization: -O (off, 0, 1, 2, 3, 4)
OPT_LEVEL ?= 2
CFLAGS := \
	-v$(PRU_VERSION) -O$(OPT_LEVEL) \
	--display_error_number --diag_wrap=off --diag_warning=225 \
	--asm_directory=$(GEN_DIR) --obj_directory=$(GEN_DIR) --pp_directory=$(GEN_DIR) \
	-ppd -ppa -g --endian=little

# Linker flags (see 'PRU Optimizing C/C++ Compiler User's Guide')
LFLAGS := \
	-m$(GEN_DIR)/$(OUTPUT_NAME).map --xml_link_info=$(GEN_DIR)/$(OUTPUT_NAME)_linkInfo.xml \
	--display_error_number --diag_wrap=off --diag_warning=225 --diag_suppress=10063-D \
	--warn_sections --reread_libs --ram_model

ifeq ($(C_FILES),)
# PRU firmware is assembly-only
#   ENTRY_POINT may be overridden in the core's makefile
ENTRY_POINT ?= main
LFLAGS += \
	--entry_point=$(ENTRY_POINT) --disable_auto_rts

else
# PRU firmware has C code
#   STACK_SIZE may be overridden in the core's makefile
#   HEAP_SIZE may be overridden in the core's makefile
STACK_SIZE ?= 0x100
HEAP_SIZE ?= 0x100
LFLAGS += \
	--stack_size=$(STACK_SIZE) --heap_size=$(HEAP_SIZE) \
	-i$(CGT_TI_PRU_PATH)/lib -i$(CGT_TI_PRU_PATH)/include --library=libc.a
endif

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

# Create the output directory once; used as an order-only prerequisite below
$(GEN_DIR):
	$(MKDIR) $(GEN_DIR)

# Invoke the compiler on all assembly files in vpath to create the object files
$(GEN_DIR)/%.obj: %.asm | $(GEN_DIR)
	@echo 'Building file: "$<"'
	@echo 'Invoking: PRU Compiler'
	"$(CGT_TI_PRU_PATH)/bin/clpru" $(INCLUDE) $(CFLAGS) $(DFLAGS) --output_file=$@ $<
	@echo 'Finished building: "$<"'
	@echo ' '

# Invoke the compiler on all c files in vpath to create the object files
$(GEN_DIR)/%.obj: %.c | $(GEN_DIR)
	@echo 'Building file: "$<"'
	@echo 'Invoking: PRU Compiler'
	"$(CGT_TI_PRU_PATH)/bin/clpru" $(INCLUDE) $(CFLAGS) $(DFLAGS) --output_file=$@ $<
	@echo 'Finished building: "$<"'
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

