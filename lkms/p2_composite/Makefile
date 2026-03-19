# This Makefile demonstrates how to build a single kernel module (mymod.ko)
# from multiple source files.

ifneq ($(KERNELRELEASE),)
    # obj-m specifies the final module name.
    obj-m := mymod.o

    # mymod-y lists all the object files that need to be compiled and linked
    # together to create mymod.ko. kbuild handles the rest.
    mymod-y := main.o data.o routine.o

else

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

.PHONY: build clean

build:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

endif
