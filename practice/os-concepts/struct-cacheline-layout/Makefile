CC     ?= gcc
CFLAGS ?= -O2 -fno-tree-vectorize -Wall -Wextra -g -pthread
BIN    := bin

all: $(BIN)/layout $(BIN)/falseshare

$(BIN)/%: %.c | $(BIN)
	$(CC) $(CFLAGS) -o $@ $<

$(BIN):
	mkdir -p $(BIN)

demo: all
	$(BIN)/layout
	@echo
	$(BIN)/falseshare

# pahole prints a struct's layout with cache-line boundaries - the tool used for the real
# kernel patches (sudo apt install dwarves)
pahole: all
	pahole -C scattered $(BIN)/layout
	pahole -C grouped $(BIN)/layout

clean:
	rm -rf $(BIN)

.PHONY: all demo pahole clean
