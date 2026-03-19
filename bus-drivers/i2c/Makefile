# Makefile for AT24 EEPROM drivers
# Compatible with both local builds and buildroot

# Kernel module objects
ifneq ($(KERNELRELEASE),)
obj-m := at24_cdrv.o at24_sys.o
else

# Default kernel directory (for local builds)
KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

# Default target - build kernel modules
all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install

.PHONY: all clean install

endif
