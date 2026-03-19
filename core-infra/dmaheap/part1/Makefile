# kbuild part of makefile
ifneq ($(KERNELRELEASE),)
obj-m := importer-map.o
EXTRA_CFLAGS += -DDEBUG
else

KDIR ?= /lib/modules/$(shell uname -r)/build

all: modules heap-alloc

modules:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

heap-alloc: heap-alloc.c
	$(CC) -Wall -o $@ $<

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean
	rm -f heap-alloc

.PHONY: all modules clean

endif
