# kbuild part of makefile
ifneq ($(KERNELRELEASE),)
obj-m := importer-async.o
EXTRA_CFLAGS += -DDEBUG
else

KDIR ?= /lib/modules/$(shell uname -r)/build

all: modules heap-async

modules:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

heap-async: heap-async.c
	$(CC) -Wall -o $@ $<

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean
	rm -f heap-async

.PHONY: all modules clean

endif
