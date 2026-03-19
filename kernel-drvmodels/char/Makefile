# kbuild part of makefile
ifneq ($(KERNELRELEASE),)
obj-m :=  cdrv.o
obj-m +=  cdrv_dyn.o
obj-m +=  cdrv_udev.o
obj-m +=  cdrv_reg.o
obj-m +=  cdrv_misc.o
EXTRA_CFLAGS += -DDEBUG
else

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

# CONFIG_MODULE_SIG=n
# command to convert input files into desired target 
# this command uses kbuild scripts of kernel-header/source folder specified
build:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
	@gcc cdrvtest.c -o cdrvtest

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	@rm -rf a.out cdrvtest

endif
