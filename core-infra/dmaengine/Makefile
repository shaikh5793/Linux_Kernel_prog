KERNEL_SRC ?= /home/raghub/raghu/buildroot/beagle/build/linux-6.1.74
ARCH ?= arm
CROSS_COMPILE ?= /home/raghub/raghu/buildroot/beagle/host/bin/arm-linux-

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean
	rm -f *.ko *.o *.mod.c *.mod.o *.symvers *.order

# Install modules (copy to BeagleBone Black)
install: all
	@echo "Copying modules to BeagleBone Black..."
	@if [ -z "$(BBB_IP)" ]; then \
		echo "Usage: make install BBB_IP=192.168.7.2"; \
		echo "Set BBB_IP to your BeagleBone Black IP address"; \
		exit 1; \
	fi
	scp *.ko debian@$(BBB_IP):/home/debian/

.PHONY: all clean install
