include ../imports.mak

SUBDIRS := \
    crc/crc \
    getting_started_labs \
    gpio/gpio_toggle \
    intc/intc_mcu \
    mac/mac mac/mac_multiply mac/mac_c \
    uart/uart_loopback

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
