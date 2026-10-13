CC     ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -g
BIN    := bin
ALPINE := https://dl-cdn.alpinelinux.org/alpine/v3.20/releases/x86_64/alpine-minirootfs-3.20.3-x86_64.tar.gz

all: $(BIN)/container $(BIN)/hog

$(BIN)/%: %.c | $(BIN)
	$(CC) $(CFLAGS) -o $@ $<

$(BIN):
	mkdir -p $(BIN)

# a ~3MB Alpine Linux userland to use as the container's root filesystem.
# hog is linked statically so it runs against Alpine's musl libc instead of the host's glibc
rootfs: $(BIN)/hog
	mkdir -p rootfs
	curl -fsSL $(ALPINE) | tar -xz -C rootfs
	$(CC) -O2 -static -o rootfs/hog hog.c

run: all
	sudo $(BIN)/container ./rootfs 64 /bin/sh

clean:
	rm -rf $(BIN)

distclean: clean
	rm -rf rootfs

.PHONY: all rootfs run clean distclean
