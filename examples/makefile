include ../imports.mak

SUBDIRS := \
    custom_frequency_generator empty empty_c \
    fft/split_radix_fft_4k_single_core fft/split_radix_fft_post_processing \
    LCD_interface logic_scope pru_emif pru_eqep pru_i2s spi_loopback \
    fir multicore_scheduler rpmsg_echo_linux

# projects that build a library of their own. Only these get "libs" and "libs-clean"
LIBS_SUBDIRS := pru_i2s

# "make" or "make all" builds projects that match $(DEVICE) set in imports.mak
all: ARGUMENTS = all
all: $(SUBDIRS)

# "make libs" builds the libraries of the projects in LIBS_SUBDIRS
libs: $(addprefix libs-,$(LIBS_SUBDIRS))

# "make libs-clean" cleans the libraries of the projects in LIBS_SUBDIRS
libs-clean: $(addprefix libs-clean-,$(LIBS_SUBDIRS))

$(addprefix libs-,$(LIBS_SUBDIRS)):
	$(MAKE) -C $(patsubst libs-%,%,$@) libs

$(addprefix libs-clean-,$(LIBS_SUBDIRS)):
	$(MAKE) -C $(patsubst libs-clean-%,%,$@) libs-clean

pru: ARGUMENTS = pru
pru: $(SUBDIRS)

host: ARGUMENTS = host
host: $(SUBDIRS)

# "make clean" cleans projects that match $(DEVICE) set in imports.mak
clean: ARGUMENTS = clean
clean: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ $(ARGUMENTS)

.PHONY: all clean pru host libs libs-clean $(SUBDIRS) \
    $(addprefix libs-,$(LIBS_SUBDIRS)) $(addprefix libs-clean-,$(LIBS_SUBDIRS))
