CC     ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -g -pthread
BIN    := bin

all: $(BIN)/iterative $(BIN)/threaded $(BIN)/epoll $(BIN)/loadgen

$(BIN)/%: %.c common.h | $(BIN)
	$(CC) $(CFLAGS) -o $@ $<

$(BIN):
	mkdir -p $(BIN)

# start each server in the background, hit it with 10 slow clients, stop it
demo: all
	@for s in iterative:9201 threaded:9202 epoll:9203; do \
	    name=$${s%%:*}; port=$${s##*:}; \
	    echo; $(BIN)/$$name $$port & pid=$$!; sleep 0.3; \
	    $(BIN)/loadgen $$port 10 500 | tail -n 1; \
	    kill $$pid; wait $$pid 2>/dev/null; \
	done; true

clean:
	rm -rf $(BIN)

.PHONY: all demo clean
