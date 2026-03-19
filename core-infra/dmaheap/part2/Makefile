# kbuild part of makefile
ifneq ($(KERNELRELEASE),)
obj-m := importer-array.o
EXTRA_CFLAGS += -DDEBUG
else

KDIR ?= /lib/modules/$(shell uname -r)/build

all: modules heap-array

modules:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

heap-array: heap-array.c
	$(CC) -Wall -o $@ $<

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean
	rm -f heap-array

.PHONY: all modules clean

endif
