# Part 1: Crash Handling Notifiers Makefile

obj-m += panic_notifier.o
obj-m += reboot_notifier.o
obj-m += die_notifier.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

install:
	sudo insmod panic_notifier.ko
	sudo insmod reboot_notifier.ko
	sudo insmod die_notifier.ko

uninstall:
	sudo rmmod die_notifier
	sudo rmmod reboot_notifier
	sudo rmmod panic_notifier

reload: uninstall install

info:
	@echo "=== Part 1: Crash Handling Notifiers ==="
	@echo "Modules:"
	@ls -lh *.ko 2>/dev/null || echo "  (not built yet)"
	@echo ""
	@echo "Loaded modules:"
	@lsmod | grep -E "(panic|reboot|die)_notifier" || echo "  (none loaded)"

.PHONY: all clean install uninstall reload info
