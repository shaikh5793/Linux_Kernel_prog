ifneq ($(KERNELRELEASE),)
    obj-m := keyring.o

else

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

# Userspace programs
USERSPACE_PROGS = test_keyring keyctl key_ops
CC = gcc
CFLAGS = -Wall -O2
LDFLAGS = -lkeyutils

all: modules userspace

modules:
	@echo "Building kernel modules..."
	$(MAKE) -C $(KDIR) M=$(PWD) modules

userspace:
	@echo "Building userspace programs..."
	@$(CC) $(CFLAGS) test_keyring.c -o test_keyring $(LDFLAGS) 2>/dev/null || \
		echo "  Warning: test_keyring failed (libkeyutils-dev not installed?)"
	@$(CC) $(CFLAGS) keyctl.c -o keyctl_prog $(LDFLAGS) 2>/dev/null || \
		echo "  Warning: keyctl_prog failed (libkeyutils-dev not installed?)"
	@$(CC) $(CFLAGS) key_ops.c -o key_ops $(LDFLAGS) 2>/dev/null || \
		echo "  Warning: key_ops failed (libkeyutils-dev not installed?)"
	@echo "  Tip: Install with: sudo apt install libkeyutils-dev"

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f $(USERSPACE_PROGS) keyctl_prog

.PHONY: all modules userspace clean

endif
