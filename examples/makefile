include ../imports.mak

SUBDIRS := \
    custom_frequency_generator empty empty_c \
    fft/split_radix_fft_4k_single_core fft/split_radix_fft_post_processing \
    LCD_interface logic_scope pru_emif pru_eqep spi_loopback \
    fir jpeg_decoder/jpeg_decoder_dual_core jpeg_decoder/jpeg_decoder_single_core jpeg_decoder/host multicore_scheduler rpmsg_echo_linux

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
