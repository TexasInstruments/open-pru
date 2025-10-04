include ../imports.mak

SUBDIRS := \
    custom_frequency_generator empty empty_c \
    fft/split_radix_fft_4k_single_core fft/split_radix_fft_post_processing \
    logic_scope pru_emif spi_loopback

# "make" or "make all" builds projects that match $(DEVICE) set in imports.mak
all: ARGUMENTS = all
all: $(SUBDIRS)

# "make clean" cleans projects that match $(DEVICE) set in imports.mak
clean: ARGUMENTS = clean
clean: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ $(ARGUMENTS)

.PHONY: all clean $(SUBDIRS)
