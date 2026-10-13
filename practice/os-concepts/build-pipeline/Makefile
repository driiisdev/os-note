CC    ?= gcc
B     := build

# --- every stage of the pipeline, one file per stage --------------------------------------
stages: | $(B)
	$(CC) -E main.c -o $(B)/main.i                      # 1. preprocess: #include/#define expanded
	$(CC) -S -O1 -fno-asynchronous-unwind-tables $(B)/main.i -o $(B)/main.s   # 2. compile: C -> assembly
	$(CC) -c $(B)/main.s -o $(B)/main.o                 # 3. assemble: assembly -> machine code
	$(CC) -c -O1 mathx.c -o $(B)/mathx.o
	$(CC) $(B)/main.o $(B)/mathx.o -o $(B)/app          # 4. link: resolve symbols -> executable
	@echo; wc -l main.c $(B)/main.i $(B)/main.s
	@echo; $(B)/app

# --- what the linker sees ------------------------------------------------------------------
symbols: stages
	@echo "== main.o: U = undefined, needs someone else to provide it"
	nm $(B)/main.o
	@echo; echo "== mathx.o: T = code (.text), B = zero-initialized data (.bss), D = initialized data"
	nm $(B)/mathx.o
	@echo; echo "== relocations: holes in main.o the linker must patch with real addresses"
	readelf -r -W $(B)/main.o | grep -E 'square|cube|calls|printf'
	@echo; echo "== after linking: the same symbols now have addresses"
	nm $(B)/app | grep -E ' (square|cube|calls|main)$$'

# --- static vs shared library --------------------------------------------------------------
static: | $(B)
	$(CC) -c -O1 mathx.c -o $(B)/mathx.o
	ar rcs $(B)/libmathx.a $(B)/mathx.o                 # a static lib is just an archive of .o files
	$(CC) main.c -L$(B) -lmathx -o $(B)/app-static      # library code is COPIED into the binary
	ls -l $(B)/app-static
	ldd $(B)/app-static
	$(B)/app-static

shared: | $(B)
	$(CC) -O1 -fPIC -shared mathx.c -o $(B)/libmathx.so # position-independent, loaded at runtime
	$(CC) main.c -L$(B) -lmathx -Wl,-rpath,'$$ORIGIN' -o $(B)/app-shared
	ls -l $(B)/app-shared
	ldd $(B)/app-shared                                 # libmathx.so is a runtime dependency now
	$(B)/app-shared
	@echo; echo "== the call to square() now goes through the PLT, resolved by ld.so"
	objdump -d --no-show-raw-insn $(B)/app-shared | grep -A3 '<main>:' | head -4
	objdump -d --no-show-raw-insn $(B)/app-shared | grep 'call.*square'

demo: stages symbols static shared

$(B):
	mkdir -p $(B)

clean:
	rm -rf $(B)

.PHONY: stages symbols static shared demo clean
