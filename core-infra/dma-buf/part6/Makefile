# kbuild part of makefile
ifneq ($(KERNELRELEASE),)
obj-m := exporter-fence.o
obj-m += importer-fence.o
EXTRA_CFLAGS += -DDEBUG
else

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all: modules test-fence

modules:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

test-fence: test-fence.c
	$(CC) -Wall -o $@ $<

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f test-fence

.PHONY: all modules clean

endif
