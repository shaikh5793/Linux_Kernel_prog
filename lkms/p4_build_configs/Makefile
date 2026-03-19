# This Makefile demonstrates how to use EXTRA_CFLAGS to change the build.

ifneq ($(KERNELRELEASE),)
    obj-m := config.o

    # The EXTRA_CFLAGS variable allows you to add custom flags to the C compiler.
    # You can uncomment the line below to enable debug messages by default.
    # EXTRA_CFLAGS += -DDEBUG

else

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

.PHONY: all clean

# The default target.
all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

# A specific target for a debug build.
# This shows how to pass CFLAGS from the command line.
# Usage: make debug
debug:
	$(MAKE) -C $(KDIR) M=$(PWD) \"EXTRA_CFLAGS=-DDEBUG\" modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

endif

