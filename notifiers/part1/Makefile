# Part 1: System Event Notifiers Makefile

obj-m += cpu_hotplug.o
obj-m += oom_notifier.o
obj-m += module_notifier.o
obj-m += pm_notifier.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	$(MAKE) -C tools clean || true

install:
	sudo insmod cpu_hotplug.ko
	sudo insmod oom_notifier.ko
	sudo insmod module_notifier.ko
	sudo insmod pm_notifier.ko

uninstall:
	sudo rmmod pm_notifier
	sudo rmmod module_notifier
	sudo rmmod oom_notifier
	sudo rmmod cpu_hotplug

reload: uninstall install

info:
	@echo "=== Part 1: System Event Notifiers ==="
	@echo "Modules:"
	@ls -lh *.ko 2>/dev/null || echo "  (not built yet)"
	@echo ""
	@echo "Loaded modules:"
	@lsmod | grep -E "(cpu_hotplug|oom_notifier|module_notifier|pm_notifier)" || echo "  (none loaded)"
	@echo ""
	@echo "Userspace triggers (build with: make tools):"
	@ls -lh tools/* 2>/dev/null | sed 's/^/  /' || echo "  (none built)"

.PHONY: all clean install uninstall reload info

# Build userspace OOM trigger programs
tools:
	$(MAKE) -C tools
