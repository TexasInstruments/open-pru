include ../imports.mak

# which projects will be built?
SUBDIRS := fft
SUBDIRS_MCUPLUS := empty pru_emif spi_loopback
SUBDIRS_LINUX :=

ifeq ($(BUILD_MCUPLUS),y)
    SUBDIRS += $(SUBDIRS_MCUPLUS)
endif

ifeq ($(BUILD_LINUX),y)
    SUBDIRS += $(SUBDIRS_LINUX)
endif

# "make" or "make all" builds projects that match $(DEVICE) set in imports.mak
all: ARGUMENTS = all
all: $(SUBDIRS)

# "make clean" cleans projects that match $(DEVICE) set in imports.mak
clean: ARGUMENTS = clean
clean: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ $(ARGUMENTS)

.PHONY: all clean $(SUBDIRS)
