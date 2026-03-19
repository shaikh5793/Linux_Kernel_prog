# kbuild part of makefile
ifneq ($(KERNELRELEASE),)
obj-m := btuio.o
else

KDIR := /home/raghub/raghu/buildroot/beagle/build/linux-6.1.74
PWD := $(shell pwd)

all: build_modules

build_modules:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

# Build alias
build: all
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -rf *.o *.ko *.mod* Module.symvers modules.order .*.cmd

.PHONY: all build build_modules clean

endif