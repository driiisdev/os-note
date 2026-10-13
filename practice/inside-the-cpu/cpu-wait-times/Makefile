CC     ?= gcc
BIN    := bin

# no auto-vectorization and no if-conversion: we want the compiler to emit exactly the
# scalar loads, adds and conditional branches the demos are about. without these flags gcc
# turns branch.c's `if` into a branchless cmov and ilp.c's loop into SIMD, and the effects
# being measured disappear (try it: make CFLAGS=-O2 clean all)
CFLAGS ?= -O2 -fno-tree-vectorize -fno-if-conversion -fno-if-conversion2 -Wall -Wextra

all: $(BIN)/latency $(BIN)/branch $(BIN)/ilp

$(BIN)/%: %.c | $(BIN)
	$(CC) $(CFLAGS) -o $@ $<

$(BIN):
	mkdir -p $(BIN)

demo: all
	$(BIN)/latency
	@echo
	$(BIN)/branch
	@echo
	$(BIN)/ilp

clean:
	rm -rf $(BIN)

.PHONY: all demo clean
