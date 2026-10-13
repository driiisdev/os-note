CC     ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -g
BIN    := bin

all: $(BIN)/server $(BIN)/client

$(BIN)/%: %.c | $(BIN)
	$(CC) $(CFLAGS) -o $@ $<

$(BIN):
	mkdir -p $(BIN)

# the two scenarios that need no root. the keepalive / silent-drop one is in the README.
demo: all
	@echo "== 1) connect timeout: a SYN sent into the void (10.255.255.1 drops it)"
	-$(BIN)/client --host 10.255.255.1 --connect-timeout 2000
	@echo
	@echo "== 2) read timeout: the server accepts the request and never answers"
	@$(BIN)/server hang & pid=$$!; sleep 0.3; \
	    $(BIN)/client --read-timeout 3000; \
	    kill $$pid

# the kernel defaults you get when you set no timeout at all
defaults:
	sysctl net.ipv4.tcp_syn_retries net.ipv4.tcp_retries2 \
	    net.ipv4.tcp_keepalive_time net.ipv4.tcp_keepalive_intvl net.ipv4.tcp_keepalive_probes

clean:
	rm -rf $(BIN)

.PHONY: all demo defaults clean
