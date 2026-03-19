# Copyright (c) 2024 TECH VEDA(www.techveda.org)
# Author: Raghu Bharadwaj
#
# This software is dual-licensed under the MIT License and GPL v2.
# See the accompanying LICENSE file for the full text.
#
# Locking Bug Modules Makefile
obj-m += deadlock.o
obj-m += double_unlock.o
obj-m += missing_lock.o
obj-m += sleep_in_atomic.o
obj-m += spin_recursion.o
obj-m += rwlock_misuse.o
obj-m += rwsem_misuse.o
obj-m += rcu_misuse.o

KERNEL_DIR ?= /home/raghub/raghu/buildroot/output/build/linux-6.12.47
ARCH ?= arm
CROSS_COMPILE ?= /home/raghub/raghu/buildroot/output/host/bin/arm-buildroot-linux-gnueabihf-

all:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean

.PHONY: all clean
