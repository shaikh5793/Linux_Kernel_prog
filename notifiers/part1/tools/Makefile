CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra

BINARIES := memleak

all: $(BINARIES)

memleak: memleak.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(BINARIES)

.PHONY: all clean

