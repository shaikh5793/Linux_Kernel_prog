# kbuild part of makefile
ifneq ($(KERNELRELEASE),)
obj-m := btint.o
else

ARCH := arm
CROSS_COMPILE := arm-linux-
KDIR := /home/raghub/raghu/buildroot/beagle/build/linux-6.1.74
PWD := $(shell pwd)

all: build_modules

build_modules:
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR) M=$(PWD) modules

clean:
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR) M=$(PWD) clean

help:
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR) M=$(PWD) help

.PHONY: all build_modules clean help

endif
