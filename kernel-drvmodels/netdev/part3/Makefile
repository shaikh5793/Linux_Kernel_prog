# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2024 TECH VEDA

obj-m := vnet_basic.o

KERNEL_DIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
EXTRA_SYMBOLS := $(PWD)/../vnet-platform/Module.symvers

all:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) KBUILD_EXTRA_SYMBOLS=$(EXTRA_SYMBOLS) modules

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean

install:
	sudo insmod ../vnet-platform/vnet_hw_platform.ko 2>/dev/null || true
	sudo insmod vnet_basic.ko

remove:
	sudo rmmod vnet_basic
	sudo rmmod vnet_hw_platform 2>/dev/null || true

.PHONY: all clean install remove
