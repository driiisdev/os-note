CC     ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -g -pthread
BIN    := bin

all: $(BIN)/pvt

$(BIN)/%: %.c | $(BIN)
	$(CC) $(CFLAGS) -o $@ $<

$(BIN):
	mkdir -p $(BIN)

clean:
	rm -rf $(BIN)

.PHONY: all clean
