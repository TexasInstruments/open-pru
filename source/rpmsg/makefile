export OPEN_PRU_PATH?=$(abspath ../..)
include $(OPEN_PRU_PATH)/imports.mak

# Define build outputs
OUTPUT_NAME := rpmsg
GEN_DIR := lib
TARGET := $(GEN_DIR)/$(OUTPUT_NAME).lib

# which directories to search for C source files?
FILES_PATH := .
vpath %.c $(FILES_PATH)
vpath %.cmd $(FILES_PATH)
vpath %.obj $(GEN_DIR)
C_FILES := $(notdir $(foreach directory, $(FILES_PATH), $(wildcard $(directory)/*.c)))

# Generate an object file for every *.c source file in FILES_PATH
OBJECTS += $(patsubst %.c, $(GEN_DIR)/%.obj, $(notdir \
	$(foreach directory, $(FILES_PATH), $(wildcard $(directory)/*.c))))

# Include paths
INCLUDE := \
	--include_path=$(CGT_TI_PRU_PATH)/include \
	--include_path=$(OPEN_PRU_PATH)/source \
	--include_path=$(OPEN_PRU_PATH)/source/include/c_code/linux

# Compiler flags (Defined in 'PRU Optimizing C/C++ Compiler User's Guide)
CFLAGS := \
	-v3 -O2 \
	--display_error_number --diag_wrap=off --diag_warning=225 \
	--asm_directory=$(GEN_DIR) --obj_directory=$(GEN_DIR) --pp_directory=$(GEN_DIR) \
	-ppd -ppa -g --endian=little

# Defines (pass instance specific definitions to the program)
DFLAGS :=

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

# Invoke the compiler on all c files in vpath to create the object files
$(GEN_DIR)/%.obj: %.c
	$(MKDIR) $(GEN_DIR)
	@echo 'Building file: $^'
	@echo 'Invoking: PRU Compiler'
	"$(CGT_TI_PRU_PATH)/bin/clpru" $(INCLUDE) $(CFLAGS) $(DFLAGS) $^
	@echo 'Finished building: "$^"'
	@echo ' '

# Invoke the archiver to make the .lib file
$(TARGET): $(OBJECTS)
	$(MKDIR) $(GEN_DIR)
	@echo 'Building target: "$@"'
	@echo 'Invoking: PRU Archiver'
	"$(CGT_TI_PRU_PATH)/bin/arpru" r $(TARGET) $(OBJECTS)
	@echo 'Finished building target: "$@"'
	@echo ' '

# To clean generated files
clean:
	-$(RMDIR) $(GEN_DIR)
	@echo 'Finished clean'
	@echo ' '
endif

.PHONY: all clean

# Include the dependencies that the compiler creates (-ppd and -ppa flags)
-include $(OBJECTS:%.obj=%.pp)
