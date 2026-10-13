CC     ?= gcc
BIN    := bin

# -fno-tree-vectorize: keep both traversals scalar, so the only difference left between
# them is the memory access pattern (otherwise row-major also gets a SIMD speedup)
CFLAGS ?= -O2 -fno-tree-vectorize -Wall -Wextra

all: $(BIN)/locality

$(BIN)/%: %.c | $(BIN)
	$(CC) $(CFLAGS) -o $@ $<

$(BIN):
	mkdir -p $(BIN)

# count actual cache misses (needs linux-tools / perf; not available inside WSL by default)
perf: $(BIN)/locality
	perf stat -e cache-references,cache-misses,L1-dcache-load-misses $(BIN)/locality

clean:
	rm -rf $(BIN)

.PHONY: all perf clean
