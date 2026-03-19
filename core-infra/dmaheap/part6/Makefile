# kbuild part of makefile
ifneq ($(KERNELRELEASE),)
obj-m := importer-poll.o
EXTRA_CFLAGS += -DDEBUG
else

KDIR ?= /lib/modules/$(shell uname -r)/build

all: modules heap-poll

modules:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

heap-poll: heap-poll.c
	$(CC) -Wall -o $@ $<

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean
	rm -f heap-poll

.PHONY: all modules clean

endif
