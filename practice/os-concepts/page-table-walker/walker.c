// a simulated x86-64 MMU: 4-level page tables (9+9+9+9 bits of index + 12 bits of offset)
// and a 16-entry TLB in front of them. nothing here touches real hardware - it's the same
// algorithm the CPU runs on every memory access, written out so you can watch it.
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ENTRIES 512                  // 4KB table / 8-byte entries
#define PRESENT  0x1ULL
#define WRITABLE 0x2ULL
#define TLB_SIZE 16

typedef uint64_t pte_t;

// "physical memory" for page tables: frame number -> 512-entry table
static pte_t *frames[1 << 20];
static uint64_t next_frame = 1;      // frame 0 means "nothing here"
static long table_pages, data_pages;

struct tlb_entry { uint64_t vpn, pfn; int valid; long last_used; };
static struct tlb_entry tlb[TLB_SIZE];
static long tick, tlb_hits, tlb_misses, page_faults;
static int verbose;

static uint64_t alloc_frame(int is_table) {
    uint64_t f = next_frame++;
    if (is_table) { frames[f] = calloc(ENTRIES, sizeof(pte_t)); table_pages++; }
    else data_pages++;
    return f;
}

static int idx(uint64_t va, int level) { return (va >> (12 + 9 * level)) & 0x1ff; }
static const char *level_name[] = { "PT  ", "PD  ", "PDPT", "PML4" };

static uint64_t cr3;                 // the register that points at the top-level table

// walk the 4 levels; with create=1 this is the OS building the mapping (on a page fault)
static pte_t *walk(uint64_t va, int create) {
    pte_t *table = frames[cr3];
    for (int level = 3; level > 0; level--) {
        pte_t *e = &table[idx(va, level)];
        if (verbose)
            printf("      %s[%3d] -> %s\n", level_name[level], idx(va, level),
                   (*e & PRESENT) ? "present" : (create ? "missing, allocating table" : "missing"));
        if (!(*e & PRESENT)) {
            if (!create) return NULL;
            *e = (alloc_frame(1) << 12) | PRESENT | WRITABLE;
        }
        table = frames[*e >> 12];
    }
    if (verbose) printf("      PT  [%3d] -> leaf entry\n", idx(va, 0));
    return &table[idx(va, 0)];
}

// the OS side: map a virtual page to a fresh physical frame
static void map_page(uint64_t va, int writable) {
    int v = verbose;
    verbose = 0;
    pte_t *leaf = walk(va, 1);
    verbose = v;
    if (!(*leaf & PRESENT)) *leaf = (alloc_frame(0) << 12) | PRESENT | (writable ? WRITABLE : 0);
}

// the CPU side: every load/store does this
static int translate(uint64_t va, int is_write, uint64_t *pa) {
    uint64_t vpn = va >> 12, off = va & 0xfff;
    tick++;
    for (int i = 0; i < TLB_SIZE; i++)
        if (tlb[i].valid && tlb[i].vpn == vpn) {
            tlb[i].last_used = tick;
            tlb_hits++;
            *pa = tlb[i].pfn << 12 | off;
            if (verbose) printf("    TLB hit  -> pa 0x%" PRIx64 "\n", *pa);
            return 0;
        }
    tlb_misses++;
    if (verbose) printf("    TLB miss -> page walk (4 extra memory reads):\n");
    pte_t *leaf = walk(va, 0);
    if (!leaf || !(*leaf & PRESENT)) {
        page_faults++;
        if (verbose) printf("    PAGE FAULT: not mapped -> OS allocates a frame, retries\n");
        map_page(va, 1);
        leaf = walk(va, 0);
    } else if (is_write && !(*leaf & WRITABLE)) {
        if (verbose) printf("    PROTECTION FAULT: write to read-only page -> SIGSEGV\n");
        return -1;
    }
    // install in the TLB, evicting the least recently used entry
    int victim = 0;
    for (int i = 1; i < TLB_SIZE; i++)
        if (!tlb[i].valid || tlb[i].last_used < tlb[victim].last_used) victim = i;
    tlb[victim] = (struct tlb_entry){ vpn, *leaf >> 12, 1, tick };
    *pa = (*leaf >> 12) << 12 | off;
    if (verbose) printf("    -> pa 0x%" PRIx64 " (cached in TLB)\n", *pa);
    return 0;
}

static void reset(void) {
    for (uint64_t f = 0; f < next_frame; f++) { free(frames[f]); frames[f] = NULL; }
    memset(tlb, 0, sizeof tlb);
    next_frame = 1;
    table_pages = data_pages = tick = tlb_hits = tlb_misses = page_faults = 0;
    cr3 = alloc_frame(1);
}

int main(void) {
    reset();
    uint64_t va = 0x00007f1234567abcULL, pa;
    printf("1) splitting a virtual address: 0x%016" PRIx64 "\n", va);
    printf("   PML4 %d | PDPT %d | PD %d | PT %d | offset 0x%03" PRIx64 "\n\n",
           idx(va, 3), idx(va, 2), idx(va, 1), idx(va, 0), va & 0xfff);

    printf("2) the same address, accessed three times\n");
    verbose = 1;
    for (int i = 1; i <= 3; i++) {
        printf("  access #%d\n", i);
        translate(va, 0, &pa);
    }
    printf("  read-only page, then a write to it:\n");
    map_page(0x400000, 0);
    translate(0x400000, 1, &pa);
    verbose = 0;

    printf("\n3) page-table memory: 512 pages (2MB) dense vs spread 1GB apart\n");
    reset();
    for (uint64_t i = 0; i < 512; i++) map_page(0x10000000ULL + i * 4096, 1);
    printf("   dense : %3ld data pages need %3ld page-table pages (%ld KB of tables)\n",
           data_pages, table_pages, table_pages * 4);
    reset();
    for (uint64_t i = 0; i < 512; i++) map_page(i << 30, 1);
    printf("   sparse: %3ld data pages need %3ld page-table pages (%ld KB of tables)\n\n",
           data_pages, table_pages, table_pages * 4);

    printf("4) TLB hit rate: 100k accesses, sequential vs random over 64MB\n");
    reset();
    for (long i = 0; i < 100000; i++) translate(0x20000000ULL + (i * 64) % (64 << 20), 0, &pa);
    printf("   sequential (64B steps): %5.1f%% TLB hits\n", 100.0 * tlb_hits / (tlb_hits + tlb_misses));
    reset();
    srand(1);
    for (long i = 0; i < 100000; i++)
        translate(0x20000000ULL + ((uint64_t)rand() * 64) % (64 << 20), 0, &pa);
    printf("   random                : %5.1f%% TLB hits (%ld page walks)\n",
           100.0 * tlb_hits / (tlb_hits + tlb_misses), tlb_misses);
    return 0;
}
