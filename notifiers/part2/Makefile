# Part 3: Network Notifiers Makefile

obj-m += netdev_notifier.o
obj-m += inet_notifier.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

install:
	sudo insmod netdev_notifier.ko
	sudo insmod inet_notifier.ko

uninstall:
	sudo rmmod inet_notifier
	sudo rmmod netdev_notifier

reload: uninstall install

info:
	@echo "=== Part 3: Network Notifiers ==="
	@echo "Modules:"
	@ls -lh *.ko 2>/dev/null || echo "  (not built yet)"
	@echo ""
	@echo "Loaded modules:"
	@lsmod | grep -E "(netdev|inet)_notifier" || echo "  (none loaded)"

.PHONY: all clean install uninstall reload info
