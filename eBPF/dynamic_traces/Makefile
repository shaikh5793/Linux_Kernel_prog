CLANG ?= clang
BPFTOOL ?= bpftool

CFLAGS := -g -O2 -Wall -Werror

all: bpf/dynamic_traces.skel.h user/dynamic_traces

bpf/dynamic_traces.bpf.o: bpf/dynamic_traces.bpf.c
	$(CLANG) -O2 -g -Wall -Werror -target bpf \
		-D__TARGET_ARCH_x86 -I bpf -c $< -o $@

bpf/dynamic_traces.skel.h: bpf/dynamic_traces.bpf.o
	$(BPFTOOL) gen skeleton $< > $@

user/dynamic_traces: user/dynamic_traces.c bpf/dynamic_traces.skel.h
	cc $(CFLAGS) -I bpf $< -o $@ -lbpf

.PHONY: clean
clean:
	rm -f bpf/*.o bpf/*.skel.h user/dynamic_traces

.PHONY: all clean