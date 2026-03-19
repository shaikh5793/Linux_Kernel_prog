# kbuild part of makefile
ifneq ($(KERNELRELEASE),)
obj-m := intactx.o
#EXTRA_CFLAGS += -DDEBUG
else

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all: build_modules

build_modules:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

# Legacy build target for backward compatibility
build: all

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

.PHONY: all build_modules build clean

endif
