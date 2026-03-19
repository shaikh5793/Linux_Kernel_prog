# kbuild part of makefile
ifneq ($(KERNELRELEASE),)
obj-m := exporter-sync.o
obj-m += importer-sync.o
EXTRA_CFLAGS += -DDEBUG
else

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all: modules test-sync

modules:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

test-sync: test-sync.c
	$(CC) -Wall -o $@ $<

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f test-sync

.PHONY: all modules clean

endif
