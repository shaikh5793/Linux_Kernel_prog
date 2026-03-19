# kbuild part of makefile
ifneq ($(KERNELRELEASE),)
obj-m := importer-multisg.o
EXTRA_CFLAGS += -DDEBUG
else

KDIR ?= /lib/modules/$(shell uname -r)/build

all: modules heap-multisg

modules:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

heap-multisg: heap-multisg.c
	$(CC) -Wall -o $@ $<

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean
	rm -f heap-multisg

.PHONY: all modules clean

endif
