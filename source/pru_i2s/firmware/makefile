# PRU_CGT environment variable must point to the TI PRU code gen tools directory. E.g.:
#(Desktop Linux) export PRU_CGT=/path/to/pru/code/gen/tools/ti-cgt-pru_2.x.y
#(Windows) set PRU_CGT=C:/path/to/pru/code/gen/tools/ti-cgt-pru_2.x.y
#(ARM Linux*) export PRU_CGT=/usr/share/ti/cgt-pru
#
# *ARM Linux also needs to create a symbolic link to the /usr/bin/ directory in
# order to use the same Makefile
#(ARM Linux) ln -s /usr/bin/ /usr/share/ti/cgt-pru/bin

# ifndef PRU_CGT
# define ERROR_BODY

# *******************************************************************************
# PRU_CGT environment variable is not set. Examples given:
# (Desktop Linux) export PRU_CGT=/path/to/pru/code/gen/tools/ti-cgt-pru_2.1.2
# (Windows) set PRU_CGT=C:/path/to/pru/code/gen/tools/ti-cgt-pru_2.1.2
# (ARM Linux*) export PRU_CGT=/usr/share/ti/cgt-pru

# *ARM Linux also needs to create a symbolic link to the /usr/bin/ directory in
# order to use the same Makefile
# (ARM Linux) ln -s /usr/bin/ /usr/share/ti/cgt-pru/bin
# *******************************************************************************

# endef
# $(error $(ERROR_BODY))
# endif

ifndef PRU_CGT
define ERROR_BODY

*******************************************************************************
PRU_CGT environment variable is not set. Examples given:
(Desktop Linux) export PRU_CGT=/path/to/pru/code/gen/tools/ti-cgt-pru_2.1.2
(Windows) set PRU_CGT=C:/path/to/pru/code/gen/tools/ti-cgt-pru_2.1.2
(ARM Linux*) export PRU_CGT=/usr/share/ti/cgt-pru

*ARM Linux also needs to create a symbolic link to the /usr/bin/ directory in
order to use the same Makefile
(ARM Linux) ln -s /usr/bin/ /usr/share/ti/cgt-pru/bin
*******************************************************************************

endef
$(error $(ERROR_BODY))
endif

#PRU_CGT := C:/ti/ti-cgt-pru_2.3.3
PRU_CORE ?= 0
I2S_TX ?= 0
I2S_RX ?= 0
I2S_RX_DETECT_OVERFLOW ?= 0
I2S_TX_DETECT_UNDERFLOW ?= 0
NUMBER_OF_TX_3 ?= 0
NUMBER_OF_TX_2 ?= 0
 TOOLS_PATH?=C:/ti
CCS_PATH?=$(TOOLS_PATH)/ccs1270/ccs
CCS_ECLIPSE=$(CCS_PATH)/eclipse/eclipsec
CYGWIN_PATH?=$(CCS_PATH)/utils/cygwin
MKDIR=$(CYGWIN_PATH)/mkdir -p
RMDIR=$(CYGWIN_PATH)/rm -rf
RM=$(CYGWIN_PATH)/rm -f
COPY=$(CYGWIN_PATH)/cp
MOVE=$(CYGWIN_PATH)/mv
CAT=$(CYGWIN_PATH)/cat

MKFILE_PATH := $(abspath $(lastword $(MAKEFILE_LIST)))
CURRENT_DIR := $(notdir $(patsubst %/,%,$(dir $(MKFILE_PATH))))
PROJ_NAME = prui2s$(PRU_CORE)

LINKER_COMMAND_FILE=./i2s_PRU0.cmd

LIBS=

#INCLUDE=--include_path="./"
STACK_SIZE=0x100
HEAP_SIZE=0x100
GEN_DIR=gen

#CDEFINES = -DSOC_AM64X
CDEFINES += -DPRU$(PRU_CORE)
ifeq ($(I2S_TX),1)
CDEFINES += -DI2S_TX
endif
ifeq ($(I2S_RX),1)
CDEFINES += -DI2S_RX
endif
ifeq ($(I2S_RX_DETECT_OVERFLOW),1)
CDEFINES += -DI2S_RX_DETECT_OVERFLOW
endif
ifeq ($(I2S_TX_DETECT_UNDERFLOW),1)
CDEFINES += -DI2S_TX_DETECT_UNDERFLOW
endif
ifeq ($(NUMBER_OF_TX_3),1)
CDEFINES += -DNUMBER_OF_TX_3
endif
ifeq ($(NUMBER_OF_TX_2),1)
CDEFINES += -DNUMBER_OF_TX_2
endif

#Common compiler and linker flags (Defined in 'PRU Optimizing C/C++ Compiler User's Guide)
#CFLAGS= $(CDEFINES) -v3 -o2 --display_error_number --endian=little --hardware_mac=on --asm_directory=$(GEN_DIR) --obj_directory=$(GEN_DIR) --pp_directory=$(GEN_DIR) -ppd -ppa --define=PRU0
CFLAGS= $(CDEFINES) -v3 -o2 --display_error_number --endian=little --hardware_mac=on --asm_directory=$(GEN_DIR) --obj_directory=$(GEN_DIR) --pp_directory=$(GEN_DIR) -ppd -ppa
#Linker flags (Defined in 'PRU Optimizing C/C++ Compiler User's Guide)
LFLAGS=--reread_libs --warn_sections --stack_size=$(STACK_SIZE) --heap_size=$(HEAP_SIZE) --entry_point=I2S_MAIN


TARGET=$(GEN_DIR)/$(PROJ_NAME)_array.h
PRU_IMAGE_PREFIX=pru_$(PROJ_NAME)_image

EXE=$(GEN_DIR)/$(PROJ_NAME).out
MAP=$(GEN_DIR)/$(PROJ_NAME).map
OBJECTS=$(patsubst %.asm,$(GEN_DIR)/%.object,$(wildcard *.asm))
OBJECTS+=$(patsubst %.c,$(GEN_DIR)/%.object,$(wildcard *.c))


all: printStart $(TARGET) printEnd

printStart:
	@echo ''
	@echo '************************************************************'
	@echo 'Building project: $(PROJ_NAME)'

printEnd:
	@echo ''
	@echo 'Output files can be found in the "$(GEN_DIR)" directory'
	@echo ''
	@echo 'Finished building project: $(PROJ_NAME)'
	@echo '************************************************************'
	@echo ''


# Invoke the hex format converter to translate the .out file to a C header file
$(TARGET): $(EXE)
	@echo ''
	@echo 'Building image header file: $@'
	@echo 'Invoking: PRU Hex Format Converter'
	$(PRU_CGT)/bin/hexpru --array --array:name_prefix=$(PRU_IMAGE_PREFIX) $(EXE)
	$(MOVE) $(subst .h,.c,$(PROJ_NAME)_array.h) $(TARGET)
	$(COPY) $(TARGET) ./
	@echo 'Finished image header file: $@'
    
# Invokes the linker (-z flag) to make the .out file
$(EXE): $(OBJECTS) $(LINKER_COMMAND_FILE)
	@echo ''
	@echo 'Building executable: $@'
	@echo 'Invoking: PRU Linker'
	$(PRU_CGT)/bin/clpru $(CFLAGS) -z -i$(PRU_CGT)/lib -i$(PRU_CGT)/include $(LFLAGS) -o $(EXE) $(OBJECTS) -m$(MAP) $(LINKER_COMMAND_FILE) $(LIBS)
	@echo 'Finished building executable: $@'

# Invokes the compiler on all assembly files in the directory to create the object files
$(GEN_DIR)/%.object: %.asm
	$(MKDIR) $(GEN_DIR)
	@echo ''
	@echo 'Building file: $<'
	@echo 'Invoking: PRU Compiler'
	$(PRU_CGT)/bin/clpru --include_path=$(PRU_CGT)/include $(INCLUDE) $(CFLAGS) -fe $@ $<

# Invokes the compiler on all c files in the directory to create the object files
$(GEN_DIR)/%.object: %.c
	$(MKDIR) $(GEN_DIR)
	@echo ''
	@echo 'Building file: $<'
	@echo 'Invoking: PRU Compiler'
	$(PRU_CGT)/bin/clpru -k --include_path=$(PRU_CGT)/include $(INCLUDE) $(CFLAGS) -fe $@ $<

.PHONY: all clean

# Remove the $(GEN_DIR) directory
clean:
	@echo ''
	@echo '************************************************************'
	@echo 'Cleaning project: $(PROJ_NAME)'
	@echo ''
	@echo 'Removing files in the "$(GEN_DIR)" directory'
	$(RMDIR) $(GEN_DIR)
	$(RM) $(subst .h,.c,$(PROJ_NAME)_array.h)
	@echo ''
	@echo 'Finished cleaning project: $(PROJ_NAME)'
	@echo '************************************************************'
	@echo ''

# Includes the dependencies that the compiler creates (-ppd and -ppa flags)
-include $(OBJECTS:%.object=%.pp)

