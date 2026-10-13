CC     ?= gcc
BIN    := bin

# -O0 keeps every local variable on the stack and every call a real call - optimization would
# turn add() into a constant and the whole point of the demo would disappear
CFLAGS ?= -O0 -g -Wall -Wextra -fno-omit-frame-pointer

all: $(BIN)/stack $(BIN)/add

$(BIN)/%: %.c | $(BIN)
	$(CC) $(CFLAGS) -o $@ $<

$(BIN):
	mkdir -p $(BIN)

# the compiler's output, readable: prologue, locals at [rbp-N], call, epilogue, ret
asm: | $(BIN)
	$(CC) -O0 -S -masm=intel -fno-asynchronous-unwind-tables -fcf-protection=none -o $(BIN)/add.s add.c
	cat $(BIN)/add.s

step: $(BIN)/add
	gdb -q -x step.gdb $(BIN)/add

clean:
	rm -rf $(BIN)

.PHONY: all asm step clean
