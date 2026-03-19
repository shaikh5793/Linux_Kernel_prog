# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2024 TECH VEDA

obj-m := vnet_hw_platform.o

KERNEL_DIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean

install:
	sudo insmod vnet_hw_platform.ko

remove:
	sudo rmmod vnet_hw_platform

.PHONY: all clean install remove
