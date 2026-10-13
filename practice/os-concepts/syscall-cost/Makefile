CC     ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -g
BIN    := bin

all: $(BIN)/syscost

$(BIN)/%: %.c | $(BIN)
	$(CC) $(CFLAGS) -o $@ $<

$(BIN):
	mkdir -p $(BIN)

demo: all
	$(BIN)/syscost

# count every syscall a familiar command makes (needs: sudo apt install strace)
strace-ls:
	strace -c -f ls -la / > /dev/null

clean:
	rm -rf $(BIN)

.PHONY: all demo strace-ls clean
