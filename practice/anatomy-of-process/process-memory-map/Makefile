CC     ?= gcc
CFLAGS ?= -O0 -Wall -Wextra -g
BIN    := bin

all: $(BIN)/layout $(BIN)/heap

$(BIN)/%: %.c | $(BIN)
	$(CC) $(CFLAGS) -o $@ $<

$(BIN):
	mkdir -p $(BIN)

# the program on disk: section sizes and headers, before it is ever a process
inspect: $(BIN)/layout
	size $(BIN)/layout
	readelf -S -W $(BIN)/layout | grep -E ' \.(text|rodata|data|bss) '

clean:
	rm -rf $(BIN)

.PHONY: all inspect clean
