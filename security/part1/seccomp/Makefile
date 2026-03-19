ifneq ($(KERNELRELEASE),)
    obj-m := seccomp_info.o
else

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	@echo "Building Seccomp kernel modules..."
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f seccomp_basic seccomp_modes seccomp_test

.PHONY: all clean

endif
