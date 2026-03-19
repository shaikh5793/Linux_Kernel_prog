# Part 5: Custom Notifiers Makefile

obj-m += custom_notifier.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

install:
	sudo insmod custom_notifier.ko

uninstall:
	sudo rmmod custom_notifier

reload: uninstall install

info:
	@echo "=== Part 5: Custom Notifiers ==="
	@echo "Modules:"
	@ls -lh *.ko 2>/dev/null || echo "  (not built yet)"
	@echo ""
	@echo "Loaded modules:"
	@lsmod | grep custom_notifier || echo "  (none loaded)"

.PHONY: all clean install uninstall reload info
