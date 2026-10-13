CC     ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -g
BIN    := bin

all: $(BIN)/walker $(BIN)/pte-bloat

$(BIN)/%: %.c | $(BIN)
	$(CC) $(CFLAGS) -o $@ $<

$(BIN):
	mkdir -p $(BIN)

demo: all
	$(BIN)/walker
	@echo
	$(BIN)/pte-bloat 8 512

clean:
	rm -rf $(BIN)

.PHONY: all demo clean
