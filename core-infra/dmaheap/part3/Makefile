# kbuild part of makefile
ifneq ($(KERNELRELEASE),)
obj-m := importer-sync.o
EXTRA_CFLAGS += -DDEBUG
else

KDIR ?= /lib/modules/$(shell uname -r)/build

all: modules heap-sync

modules:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

heap-sync: heap-sync.c
	$(CC) -Wall -o $@ $<

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean
	rm -f heap-sync

.PHONY: all modules clean

endif
