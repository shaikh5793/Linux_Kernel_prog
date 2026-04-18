ifneq ($(KERNELRELEASE),)
obj-m := alloc.o
obj-m += mask.o
obj-m += zones.o
obj-m += limits.o
obj-m += pfn.o
obj-m += numa.o
obj-m += cma_large.o

EXTRA_CFLAGS += -DDEBUG
else

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install

uninstall:
	rm -f /lib/modules/$(shell uname -r)/extra/*.ko

info:
	@echo "=== Page Allocator Examples ==="

endif
