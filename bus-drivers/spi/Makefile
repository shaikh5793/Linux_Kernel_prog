# Original variables
KDIR= /home/raghu/buildroot/beagle/build/linux-6.1.74

# Build with license validation
all: build_targets

build_targets:
obj-m  := spiflash.o 

KDIR= /home/raghu/buildroot/beagle/build/linux-6.1.74

all:
	$(MAKE) -C $(KDIR) M=$(PWD)  modules
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -rf $(wildcard *.o *.ko *.mo* Mo* *.order)
	

.PHONY: all build_targets
