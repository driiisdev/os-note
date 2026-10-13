CC     ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -g
BIN    := bin

all: $(BIN)/hello

$(BIN)/%: %.c | $(BIN)
	$(CC) $(CFLAGS) -o $@ $<

$(BIN):
	mkdir -p $(BIN)

# count the write() calls each version makes (needs: sudo apt install strace)
trace: all
	strace -e trace=write $(BIN)/hello

clean:
	rm -rf $(BIN)

.PHONY: all trace clean
