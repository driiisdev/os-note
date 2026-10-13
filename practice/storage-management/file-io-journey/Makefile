CC     ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -g
BIN    := bin

all: $(BIN)/writepath $(BIN)/readcache $(BIN)/atomic-save

$(BIN)/%: %.c | $(BIN)
	$(CC) $(CFLAGS) -o $@ $<

$(BIN):
	mkdir -p $(BIN)

demo: all
	$(BIN)/writepath
	@echo
	$(BIN)/readcache
	@echo
	$(BIN)/atomic-save

# watch the kernel flush dirty pages in the background (Ctrl+C to stop)
watch-dirty:
	watch -n 0.5 "grep -E '^(Dirty|Writeback):' /proc/meminfo"

clean:
	rm -rf $(BIN) testfile.bin cachefile.bin

.PHONY: all demo watch-dirty clean
