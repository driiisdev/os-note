CC     ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -g
BIN    := bin

all: $(BIN)/cpu

$(BIN)/%: %.c | $(BIN)
	$(CC) $(CFLAGS) -o $@ $<

$(BIN):
	mkdir -p $(BIN)

demo: all
	$(BIN)/cpu regs --trace
	@echo
	$(BIN)/cpu mem --trace
	@echo
	$(BIN)/cpu mem 4

clean:
	rm -rf $(BIN)

.PHONY: all demo clean
