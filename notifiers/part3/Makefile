# Part 3: Storage Notifiers Makefile (modern-compatible)

obj-m += storage_bus_notifier.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

install:
	sudo insmod storage_bus_notifier.ko

uninstall:
	sudo rmmod storage_bus_notifier

reload: uninstall install

info:
	@echo "=== Part 3: Storage Notifiers ==="
	@echo "Modules:"
	@ls -lh *.ko 2>/dev/null || echo "  (not built yet)"
	@echo ""
	@echo "Loaded modules:"
	@lsmod | grep storage_bus_notifier || echo "  (none loaded)"

.PHONY: all clean install uninstall reload info
