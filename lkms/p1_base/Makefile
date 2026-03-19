# This is a standard kbuild Makefile, which is the idiomatic way to build
# kernel modules. It uses the ifneq ($(KERNELRELEASE),) construct to separate
# the two stages of the build process.

# --- Stage 2: Executed by kbuild --- #
# This part of the Makefile is executed when make is invoked from the kernel
# source directory (i.e., when KERNELRELEASE is defined).
ifneq ($(KERNELRELEASE),)
    # obj-m is the variable used by kbuild to identify which modules to build.
    # Each .o file listed here will be compiled from a corresponding .c file
    # and linked into a .ko (kernel object) file.
    obj-m := hello.o

# --- Stage 1: Executed by the user --- #
# This part is executed when you run `make` in this directory.
else

# KDIR is the path to the kernel source/headers. `uname -r` gets the current
# kernel version.
KDIR ?= /lib/modules/$(shell uname -r)/build
# PWD is the path to the current directory.
PWD := $(shell pwd)

# Phony targets are not real files, just names for command recipes.
.PHONY: build clean

# The default target, 'build', will be executed if you just run 'make'.
build:
	# This command recursively calls make, telling it to run in the kernel
	# directory (KDIR) but to find the module source in our directory (PWD).
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	# This does the same for the 'clean' target in kbuild.
	$(MAKE) -C $(KDIR) M=$(PWD) clean

endif
