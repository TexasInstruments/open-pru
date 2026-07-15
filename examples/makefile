include ../imports.mak

SUBDIRS := \
    custom_frequency_generator empty empty_c \
    fft/split_radix_fft_4k_single_core fft/split_radix_fft_post_processing \
    LCD_interface logic_scope pru_emif pru_i2s spi_loopback \
    fir multicore_scheduler rpmsg_echo_linux

# "make" or "make all" builds projects that match $(DEVICE) set in imports.mak
all: ARGUMENTS = all
all: $(SUBDIRS)

pru: ARGUMENTS = pru
pru: $(SUBDIRS)

host: ARGUMENTS = host
host: $(SUBDIRS)

# "make clean" cleans projects that match $(DEVICE) set in imports.mak
clean: ARGUMENTS = clean
clean: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ $(ARGUMENTS)

.PHONY: all clean pru host $(SUBDIRS)
