// a toy CPU: 4 registers, a program counter, one flat memory, and an 8-instruction ISA.
// every instruction goes through fetch -> decode -> execute -> memory -> write-back, and
// every trip to memory costs extra cycles - so you can watch the CPU spend most of its time
// waiting instead of computing.
//
//   ./cpu regs           sum 5..1 keeping the running total in a register
//   ./cpu mem            same sum, but load/store the total from memory every iteration
//   ./cpu mem --trace    print every stage of every instruction
//   ./cpu mem 4          pretend memory is fast (4-cycle latency, like an L1 hit)
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum opcode { HALT, LOADI, ADD, SUB, LOAD, STORE, JNZ, PRINT };
static const char *names[] = { "HALT", "LOADI", "ADD", "SUB", "LOAD", "STORE", "JNZ", "PRINT" };

// instruction encoding (32 bits):  [ opcode:8 | reg a:8 | operand b:16 ]
#define ENC(op, a, b) ((uint32_t)(op) << 24 | (uint32_t)(a) << 16 | (uint16_t)(b))

// program 1: the running total lives in a register (what an optimizing compiler would do)
static const uint32_t prog_regs[] = {
    ENC(LOADI, 0, 0),    // 0: r0 = 0        (sum)
    ENC(LOADI, 1, 5),    // 1: r1 = 5        (i)
    ENC(LOADI, 2, 1),    // 2: r2 = 1        (constant 1)
    ENC(ADD,   0, 1),    // 3: r0 += r1      <- loop
    ENC(SUB,   1, 2),    // 4: r1 -= r2
    ENC(JNZ,   1, 3),    // 5: if r1 != 0 goto 3
    ENC(STORE, 0, 200),  // 6: mem[200] = r0
    ENC(PRINT, 0, 0),    // 7: print r0
    ENC(HALT,  0, 0),
};

// program 2: the running total lives in memory (what -O0 code does with a local variable)
static const uint32_t prog_mem[] = {
    ENC(LOADI, 0, 0),    // 0: r0 = 0
    ENC(STORE, 0, 200),  // 1: mem[200] = 0  (sum)
    ENC(LOADI, 1, 5),    // 2: r1 = 5        (i)
    ENC(LOADI, 2, 1),    // 3: r2 = 1
    ENC(LOAD,  0, 200),  // 4: r0 = mem[200] <- loop
    ENC(ADD,   0, 1),    // 5: r0 += r1
    ENC(STORE, 0, 200),  // 6: mem[200] = r0
    ENC(SUB,   1, 2),    // 7: r1 -= r2
    ENC(JNZ,   1, 4),    // 8: if r1 != 0 goto 4
    ENC(LOAD,  3, 200),  // 9: r3 = mem[200]
    ENC(PRINT, 3, 0),    // 10: print r3
    ENC(HALT,  0, 0),
};

struct cpu {
    int32_t  r[4];       // general purpose registers
    uint32_t pc;         // program counter: address of the NEXT instruction
    uint32_t ir;         // instruction register: the instruction being executed
    uint32_t mem[256];   // memory holds both the program and the data (von Neumann)
    long cycles, stall_cycles, instructions;
    int mem_latency, trace;
};

static void run(struct cpu *c) {
    for (;;) {
        // ---- FETCH: read the instruction at pc into ir, advance pc --------------------
        c->ir = c->mem[c->pc];
        uint32_t at = c->pc++;
        c->cycles++;                      // assume the instruction cache always hits

        // ---- DECODE: split the bits into opcode + operands ----------------------------
        enum opcode op = c->ir >> 24;
        int a = (c->ir >> 16) & 0xff;
        int b = c->ir & 0xffff;
        c->cycles++;
        c->instructions++;

        if (c->trace) {
            char operands[16];
            if (op == ADD || op == SUB) snprintf(operands, sizeof operands, "r%d, r%d", a, b);
            else if (op == HALT || op == PRINT) snprintf(operands, sizeof operands, "r%d", a);
            else snprintf(operands, sizeof operands, "r%d, %d", a, b);
            printf("  pc=%02u ir=0x%08x | %-5s %-8s | ", at, c->ir, names[op], operands);
        }

        // ---- EXECUTE / MEMORY / WRITE-BACK -------------------------------------------
        c->cycles++;
        switch (op) {
        case HALT:
            if (c->trace) printf("halt\n");
            return;
        case LOADI:
            c->r[a] = b;
            if (c->trace) printf("write-back r%d = %d\n", a, b);
            break;
        case ADD:
            c->r[a] += c->r[b];           // the ALU does its one-cycle job here
            if (c->trace) printf("ALU r%d += r%d -> %d\n", a, b, c->r[a]);
            break;
        case SUB:
            c->r[a] -= c->r[b];
            if (c->trace) printf("ALU r%d -= r%d -> %d\n", a, b, c->r[a]);
            break;
        case LOAD:
            c->cycles += c->mem_latency;  // the core can do nothing until the data arrives
            c->stall_cycles += c->mem_latency;
            c->r[a] = (int32_t)c->mem[b];
            if (c->trace) printf("MEMORY read [%d] -> r%d = %d  (stalled %d cycles)\n",
                                 b, a, c->r[a], c->mem_latency);
            break;
        case STORE:
            c->cycles += c->mem_latency;
            c->stall_cycles += c->mem_latency;
            c->mem[b] = (uint32_t)c->r[a];
            if (c->trace) printf("MEMORY write [%d] = %d  (stalled %d cycles)\n",
                                 b, c->r[a], c->mem_latency);
            break;
        case JNZ:
            if (c->r[a] != 0) c->pc = b;  // a taken branch simply overwrites pc
            if (c->trace) printf("branch %s\n", c->r[a] ? "taken" : "not taken");
            break;
        case PRINT:
            if (c->trace) printf("output\n");
            printf("  >> r%d = %d\n", a, c->r[a]);
            break;
        }
    }
}

int main(int argc, char **argv) {
    struct cpu c;
    memset(&c, 0, sizeof c);
    c.mem_latency = 100;  // a RAM access is ~100 cycles on a real CPU

    const char *which = "regs";
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--trace") == 0) c.trace = 1;
        else if (strcmp(argv[i], "regs") == 0 || strcmp(argv[i], "mem") == 0) which = argv[i];
        else c.mem_latency = atoi(argv[i]);
    }
    const uint32_t *prog = strcmp(which, "mem") == 0 ? prog_mem : prog_regs;
    size_t len = strcmp(which, "mem") == 0 ? sizeof prog_mem : sizeof prog_regs;
    memcpy(c.mem, prog, len);  // "load" the program into memory at address 0

    printf("program '%s', memory latency %d cycles\n", which, c.mem_latency);
    run(&c);

    printf("\n  instructions executed : %ld\n", c.instructions);
    printf("  total cycles          : %ld\n", c.cycles);
    printf("  cycles stalled on mem : %ld (%.0f%%)\n", c.stall_cycles,
           100.0 * c.stall_cycles / c.cycles);
    printf("  instructions per cycle: %.3f\n", (double)c.instructions / c.cycles);
    return 0;
}
