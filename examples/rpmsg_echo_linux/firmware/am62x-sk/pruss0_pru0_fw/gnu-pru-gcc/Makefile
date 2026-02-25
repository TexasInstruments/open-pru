# Copyright (c) 2026, Dimitar Dimitrov
#   All rights reserved.
#
#   Redistribution and use in source and binary forms, with or without
#   modification, are permitted provided that the following conditions are met:
#
#   * Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#   * Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in
#     the documentation and/or other materials provided with the
#     distribution.
#   * Neither the name of the copyright holders nor the names of
#     contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission.
#
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
#  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
#  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
#  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
#  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
#  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
#  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
#  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
#  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
#  POSSIBILITY OF SUCH DAMAGE.

# Very simple makefile to cross-compile for PRU


# Common flags
CROSS_COMPILE ?= pru-
CFLAGS += -Oz
CFLAGS += -Wall -Wextra

# Headers needed by the TI rpmsg library.
HEADER_DIRS := \
  ./ \
  ../../../../../../source/include/c_code/am62x \
  ../../../../../../source/include/c_code/linux

CFLAGS += $(foreach directory, $(HEADER_DIRS), -I$(directory))

# Define this to squeeze code size by removing atexit, exit, constructors
# and destructors from CRT.
CFLAGS += -minrt

# Per-PRU core flags. The -mmcu option will select the correct linker
# script and will predefine mcu-specific macros.
CFLAGS += -mmcu=am62x.pru0

# Core-specific settings for RPMsg Echo project

# INTC CONFIGURATION
HOST_INT_BIT = 30
# The PRUSS system events used for RPMsg are defined in the Linux device tree
# PRU0 uses system event 16 (To ARM) and 17 (From ARM)
TO_ARM_HOST = 16
FROM_ARM_HOST = 17

# RPMSG CONFIGURATION
CHAN_PORT = 30

CFLAGS += \
  -DCHAN_PORT=$(CHAN_PORT) -DHOST_INT_BIT=$(HOST_INT_BIT) \
  -DTO_ARM_HOST=$(TO_ARM_HOST) -DFROM_ARM_HOST=$(FROM_ARM_HOST)

# List of source files to compile for each PRU core.
SRC := \
    ../../../main.c \
    ../../../../../../source/rpmsg/pru_rpmsg.c \
    ../../../../../../source/rpmsg/pru_virtqueue.c

# GCC's -MMD does not yield the needed C dependencies when compiling all
# C source files at once. So manually list headers here.
HEADERS := $(foreach directory, $(HEADER_DIRS), $(wildcard $(directory)/*.h))

# Where to output compiled objects
OUT := generated

# Final ELF image file names
ELF := $(OUT)/rpmsg_echo_linux_am62x-sk_pruss0_pru0_fw.elf

# ============================ DO NOT TOUCH BELOW ============================
all: $(ELF)
	@echo Success: $^

%.s : %.elf
	$(CROSS_COMPILE)objdump -S -d $< > $@

# Binary images for IMEM and DMEM memories. Might be useful
# for firmware loaders that cannot handle ELF.
%.imem.bin : %.elf
	$(CROSS_COMPILE)objcopy -O binary  --only-section=.text* $< $@

%.dmem.bin : %.elf
	$(CROSS_COMPILE)objcopy -O binary              \
	  --only-section=.data*                        \
	  --only-section=.bss*                         \
	  --set-section-flags .bss=alloc,load,contents \
	  $< $@

$(OUT):
	mkdir $(OUT)

$(ELF): $(SRC) $(HEADERS) | $(OUT)
	$(CROSS_COMPILE)gcc $(CFLAGS) $(SRC) -o $@

clean:
	$(RM) -fr $(ELF) $(OUT)

cscope:
	cscope -bRk

.PHONY: all clean cscope
