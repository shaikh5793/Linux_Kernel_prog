ifneq ($(KERNELRELEASE),)
    obj-m := cap_info.o
else

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	@echo "Building Capabilities kernel modules..."
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f test_caps

.PHONY: all clean

endif
