# kbuild part of makefile
ifneq ($(KERNELRELEASE),)
obj-m := exporter-fd.o
obj-m += importer-fd.o
EXTRA_CFLAGS += -DDEBUG
else

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all: modules userfd

modules:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

userfd: userfd.c
	$(CC) -Wall -o $@ $<

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f userfd

.PHONY: all modules clean

endif
