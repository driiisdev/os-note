CC     ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -g
BIN    := bin

all: $(BIN)/cow $(BIN)/demand

$(BIN)/%: %.c | $(BIN)
	$(CC) $(CFLAGS) -o $@ $<

$(BIN):
	mkdir -p $(BIN)

# run every experiment back to back
demo: all
	$(BIN)/cow
	@echo
	$(BIN)/demand 4k
	@echo
	$(BIN)/demand huge
	@echo
	$(BIN)/demand 4k read

clean:
	rm -rf $(BIN)

.PHONY: all demo clean
