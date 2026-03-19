ifneq ($(KERNELRELEASE),)
    obj-m := creds_info.o
else

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	@echo "Building Credentials kernel modules..."
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f test_creds

.PHONY: all clean

endif
