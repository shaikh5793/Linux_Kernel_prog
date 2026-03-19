obj-m := vnet_multiqueue.o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
EXTRA_SYMBOLS := $(PWD)/../vnet-platform/Module.symvers

all:
	$(MAKE) -C $(KDIR) M=$(PWD) KBUILD_EXTRA_SYMBOLS=$(EXTRA_SYMBOLS) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
