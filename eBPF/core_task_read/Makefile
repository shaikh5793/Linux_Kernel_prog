BPFTOOL := bpftool

BPF_CLANG ?= clang
BPF_CFLAGS ?= -O2 -g -Wall -Werror -target bpf -D__TARGET_ARCH_x86

LIBBPF_CFLAGS := $(shell pkg-config --cflags libbpf 2>/dev/null || echo "")
LIBBPF_LDLIBS := $(shell pkg-config --libs libbpf 2>/dev/null || echo "-lbpf -lelf -lz")

all: user/core_task_read

bpf/vmlinux.h:
	@mkdir -p bpf
	$(BPFTOOL) btf dump file /sys/kernel/btf/vmlinux format c > bpf/vmlinux.h

bpf/core_task_read.bpf.o: bpf/core_task_read.bpf.c bpf/vmlinux.h
	$(BPF_CLANG) $(BPF_CFLAGS) -I bpf -c $< -o $@

bpf/core_task_read.skel.h: bpf/core_task_read.bpf.o
	$(BPFTOOL) gen skeleton $< > $@

user/core_task_read: user/core_task_read.c bpf/core_task_read.skel.h
	$(CC) -g -O2 -Wall -Werror -I bpf $(LIBBPF_CFLAGS) $< -o $@ $(LIBBPF_LDLIBS)

.PHONY: clean
clean:
	rm -f bpf/*.o bpf/*.skel.h bpf/vmlinux.h user/core_task_read

