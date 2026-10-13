CC     ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -g
BIN    := bin

all: $(BIN)/msh

$(BIN)/%: %.c | $(BIN)
	$(CC) $(CFLAGS) -o $@ $<

$(BIN):
	mkdir -p $(BIN)

# non-interactive smoke test: pipe a script into the shell
demo: all
	printf 'echo hello from msh\necho one two three | tr a-z A-Z | wc -w\nls /does-not-exist\nnope\ncd /tmp\npwd\n' | MSH_TRACE=1 $(BIN)/msh

clean:
	rm -rf $(BIN)

.PHONY: all demo clean
