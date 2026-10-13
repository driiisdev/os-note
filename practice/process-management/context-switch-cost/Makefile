CC     ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -g
BIN    := bin

all: $(BIN)/pingpong

$(BIN)/%: %.c | $(BIN)
	$(CC) $(CFLAGS) -o $@ $<

$(BIN):
	mkdir -p $(BIN)

demo: all
	$(BIN)/pingpong
	@echo
	$(BIN)/pingpong any

clean:
	rm -rf $(BIN)

.PHONY: all demo clean
