CC     ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -g
BIN    := bin

all: $(BIN)/buffers $(BIN)/backlog

$(BIN)/%: %.c | $(BIN)
	$(CC) $(CFLAGS) -o $@ $<

$(BIN):
	mkdir -p $(BIN)

demo: all
	$(BIN)/buffers
	@echo
	$(BIN)/backlog

# the system-wide limits that cap what listen() and the socket buffers can ask for
limits:
	sysctl net.core.somaxconn net.ipv4.tcp_max_syn_backlog net.ipv4.tcp_rmem net.ipv4.tcp_wmem

clean:
	rm -rf $(BIN)

.PHONY: all demo limits clean
