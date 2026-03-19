# This Makefile is for the p3_features directory. It demonstrates how to
# build multiple, separate kernel modules from a single Makefile.

ifneq ($(KERNELRELEASE),)
    # List all the kernel modules to be built.
    obj-m := coremod.o usermod.o param.o ratelimit.o print.o

else

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

# Phony targets for kernel modules
.PHONY: modules clean

modules:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

# --- Target for user-space utility ---

# The default target, 'all', will build both the kernel modules and the
# user-space tools.
all: modules printlog

# Phony target for the user-space tool
.PHONY: printlog

printlog:
	# This uses the standard gcc compiler, not kbuild.
	$(CC) printlog.c -o printlog

endif

