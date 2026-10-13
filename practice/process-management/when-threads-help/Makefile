CC     ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -g -pthread
BIN    := bin

all: $(BIN)/threads

$(BIN)/%: %.c | $(BIN)
	$(CC) $(CFLAGS) -o $@ $<

$(BIN):
	mkdir -p $(BIN)

demo: all
	$(BIN)/threads cpu
	@echo
	$(BIN)/threads io

clean:
	rm -rf $(BIN)

.PHONY: all demo clean
