CC     ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -g -pthread
BIN    := bin

all: $(BIN)/race $(BIN)/deadlock

$(BIN)/%: %.c | $(BIN)
	$(CC) $(CFLAGS) -o $@ $<

$(BIN):
	mkdir -p $(BIN)

demo: all
	$(BIN)/race none
	$(BIN)/race none
	$(BIN)/race mutex
	$(BIN)/race atomic
	$(BIN)/race local
	@echo
	-$(BIN)/deadlock
	@echo
	$(BIN)/deadlock ordered

# ThreadSanitizer instruments every memory access and reports the race with stack traces
tsan: | $(BIN)
	$(CC) -O1 -g -fsanitize=thread -pthread -o $(BIN)/race-tsan race.c
	$(BIN)/race-tsan none

clean:
	rm -rf $(BIN)

.PHONY: all demo tsan clean
