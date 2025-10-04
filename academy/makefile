include ../imports.mak

SUBDIRS := \
    crc/crc \
    gpio/gpio_toggle \
    intc/intc_mcu \
    mac/mac mac/mac_multiply

# "make" or "make all" builds projects that match $(DEVICE) set in imports.mak
all: ARGUMENTS = all
all: $(SUBDIRS)

# "make clean" cleans projects that match $(DEVICE) set in imports.mak
clean: ARGUMENTS = clean
clean: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ $(ARGUMENTS)

.PHONY: all clean $(SUBDIRS)
