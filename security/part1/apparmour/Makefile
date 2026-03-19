ifneq ($(KERNELRELEASE),)
    obj-m := apparmor_info.o
else

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	@echo "Building AppArmor kernel modules..."
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f apparmor_test

.PHONY: all clean

endif
