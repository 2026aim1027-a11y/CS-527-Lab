#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "memory.h"
#include "processor.h"   /* report_runtime_fault() for runtime translation failures */
#include "load_error.h"  /* load_error() for load-time failures (bad file, OOM frames) */

char memory[MEMSIZE];
unsigned char pageTable[MAX_PROC][NUM_LOGICAL_PAGES];
unsigned char freePages[NUM_PHYSICAL_PAGES];
// int ptbr[MAX_PROC]

void mmu_init_global(void) {
    for (int f = 0; f < NUM_PHYSICAL_PAGES; f++) freePages[f] = 0;
    freePages[0] = 1; /* "Frame 0 is reserved, and will never be allocated" */
    memset(pageTable, 0, sizeof(pageTable)); //NOt needed
    // for (int p = 0; p < MAX_PROC; p++) {
    //     int frame = PT_BASE_FRAME + p;
    //     freePages[frame] = 1;       
    //     ptbr[p] = frame;
    //     memset(&memory[frame * PAGESIZE], 0, PAGESIZE); 
    // }
    memset(memory, 0, sizeof(memory));
}

int get_free_page(void) {
    for (int f = 1; f < NUM_PHYSICAL_PAGES; f++) { /* start at 1: frame 0 is reserved */
        if (!freePages[f]) {
            freePages[f] = 1;
            return f;
        }
    }
    return -1; /* out of physical memory */
}

void mmu_release_all(int proc_id) {
    // int pt_frame = ptbr[proc_id];
    for (int i = 0; i < NUM_LOGICAL_PAGES; i++) {
        unsigned char frame = pageTable[proc_id][i];
        // unsigned char frame = (unsigned char)memory[pt_frame * PAGESIZE + i];
        if (frame != 0) {
            freePages[frame] = 0;
            // memory[pt_frame * PAGESIZE + i] = 0;
            pageTable[proc_id][i] = 0;
        }
    }
}

int get_physical_address(int proc_id, int isFetch, int address) {
    int space_size = isFetch ? INSTR_LOGICAL_SIZE : DATA_LOGICAL_SIZE;
    if (address < 0 || address >= space_size) return -1;

    /* Exactly the handout's own formula: instruction pages occupy page
     * indices 0..NUM_INSTR_PAGES-1 of the table, data pages start right
     * after them at NUM_INSTR_PAGES (== "1024/PAGESIZE" in the handout's
     * own numbers). */
    int page_index = address / PAGESIZE + (isFetch ? 0 : NUM_INSTR_PAGES);
    if (page_index < 0 || page_index >= NUM_LOGICAL_PAGES) return -1;

    // int pt_entry_addr = ptbr[proc_id] * PAGESIZE + page_index;
    // unsigned char frame = (unsigned char)memory[pt_entry_addr];


    unsigned char frame = pageTable[proc_id][page_index];
    if (frame == 0) return -1; /* never mapped - a page fault, not a bug */

    return (int)frame * PAGESIZE + (address % PAGESIZE);
}

/* ---------- load-time page allocation -------------------------------- */

/* Reads path's "4 hex bytes per line" content into buf (capacity cap),
 * returning how many bytes were actually present. Shared by the
 * instruction- and data-file loading below. */
static int read_hex_file(const char *path, unsigned char *buf, int cap) {
    FILE *fp = fopen(path, "r");
    if (!fp) return 0; /* caller decides whether a missing file is fatal */
    char line[256];
    int idx = 0;
    while (fgets(line, sizeof(line), fp)) {
        unsigned int b0, b1, b2, b3;
        int n = sscanf(line, "%x %x %x %x", &b0, &b1, &b2, &b3);
        if (n <= 0) continue;
        unsigned int vals[4] = {b0, b1, b2, b3};
        for (int i = 0; i < n && idx < cap; i++) buf[idx++] = (unsigned char)(vals[i] & 0xFF);
    }
    fclose(fp);
    return idx;
}

/* Allocates ceil(content_len/PAGESIZE) frames (at least min_pages),
 * maps them into pageTable[proc_id][page_base..], and copies content
 * (zero-padding the tail of the last page) into those frames.
 *
 * If physical memory runs out partway through, any frames THIS call
 * already grabbed for proc_id must be released before load_error()
 * unwinds us out of here - otherwise they'd stay marked allocated in
 * freePages[] indefinitely (a real, confirmed leak: reproduced by
 * directly instrumenting mem_initialize() and watching free-frame
 * counts around a deliberately-triggered out-of-memory failure).
 * mmu_release_all(proc_id) is safe to call here because mem_initialize()
 * always clears proc_id's page table before its first call to this
 * function, so anything found mapped for proc_id at this point can only
 * have come from THIS load attempt. */
static void map_and_load(int proc_id, int page_base, int max_pages,
                          const unsigned char *content, int content_len, int min_pages) {
    int pages_needed = (content_len + PAGESIZE - 1) / PAGESIZE;
    if (pages_needed < min_pages) pages_needed = min_pages;
    if (pages_needed > max_pages) pages_needed = max_pages; /* content beyond the logical space is simply unreachable */

    for (int i = 0; i < pages_needed; i++) {
        int frame = get_free_page();
        if (frame < 0) {
            mmu_release_all(proc_id); /* undo this call's own partial allocation before failing */
            load_error("out of physical memory (frames exhausted while loading task onto processor %d)",
                       proc_id);
        }
        pageTable[proc_id][page_base + i] = (unsigned char)frame;
        // memory[ptbr[proc_id] * PAGESIZE + (page_base + i)] = (char)frame;

        int off = i * PAGESIZE;
        int n = content_len - off;
        if (n > PAGESIZE) n = PAGESIZE;
        if (n < 0) n = 0;
        memset(&memory[frame * PAGESIZE], 0, PAGESIZE);
        if (n > 0) memcpy(&memory[frame * PAGESIZE], content + off, n);
    }
}

void mem_initialize(int proc_id, const char *program_byte_file, const char *data_byte_file) {
    /* Defensive: if this processor slot previously ran another task,
     * make sure none of its old frames are still marked allocated
     * before handing out new ones (mem_finalize() already does this on
     * the normal "task completed" path; this guards a slot's very
     * first use too, which is already all-zero/unmapped by static
     * initialization, so this is a no-op there). */
    mmu_release_all(proc_id);

    unsigned char instr_buf[INSTR_LOGICAL_SIZE];
    int instr_len = read_hex_file(program_byte_file, instr_buf, INSTR_LOGICAL_SIZE);
    if (instr_len == 0) {
        load_error("could not read program file '%s' for processor %d", program_byte_file, proc_id);
    }
    map_and_load(proc_id, 0, NUM_INSTR_PAGES, instr_buf, instr_len, 1);

    unsigned char data_buf[DATA_LOGICAL_SIZE];
    int data_len = 0;
    if (data_byte_file) {
        data_len = read_hex_file(data_byte_file, data_buf, DATA_LOGICAL_SIZE);
    }
    /* Always map at least page 0 of data space (logical addresses
     * 0-511), even with no data file at all: none of the Lab 1-4 test
     * programs ship one, but several (e.g. tests/counter.txt) still
     * write a final result to a low data address, relying on data
     * memory simply starting at all-zero the way it always did before
     * paging existed. See the README for the fuller reasoning. */
    map_and_load(proc_id, NUM_INSTR_PAGES, NUM_DATA_PAGES,
                 data_byte_file ? data_buf : NULL, data_len, 1);
}

void mem_finalize(int proc_id, const char *data_byte_file) {
    if (data_byte_file) {
        FILE *fp = fopen(data_byte_file, "w");
        if (!fp) {
            fprintf(stderr, "Error: could not write data file '%s'\n", data_byte_file);
        } else {
            for (int addr = 0; addr < DATA_LOGICAL_SIZE; addr += 4) {
                unsigned char b[4];
                for (int k = 0; k < 4; k++) {
                    int phys = get_physical_address(proc_id, 0, addr + k);
                    /* An unmapped data page reads back as all-zero -
                     * it was never written, so that's the correct
                     * logical content, not an error. */
                    b[k] = (phys < 0) ? 0 : (unsigned char)memory[phys];
                }
                fprintf(fp, "%X %X %X %X\n", b[0], b[1], b[2], b[3]);
            }
            fclose(fp);
        }
    }
    mmu_release_all(proc_id); /* "reset page table" + "Free pages from freePage array" */
}

/* ---------- runtime byte/word accessors ------------------------------- */

unsigned char mem_fetch_byte(int proc_id, int logical_addr) {
    int phys = get_physical_address(proc_id, 1, logical_addr);
    if (phys < 0) {
        char msg[96];
        snprintf(msg, sizeof(msg), "instruction page fault at logical address %d", logical_addr);
        report_runtime_fault(proc_id, msg);
        return 0; /* OP_HALT - task is being halted by report_runtime_fault() regardless */
    }
    return (unsigned char)memory[phys];
}

int mem_read_word(int proc_id, int address) {
    unsigned char b[4];
    for (int k = 0; k < 4; k++) {
        /* Translate each byte individually rather than assuming a
         * 4-byte word never crosses a page boundary: PAGESIZE (512) is
         * a multiple of 4 so a *word-aligned* address never does, but
         * the ISA doesn't actually enforce alignment, so a
         * pathological address could. */
        int phys = get_physical_address(proc_id, 0, address + k);
        if (phys < 0) {
            char msg[96];
            snprintf(msg, sizeof(msg), "data page fault reading address %d", address);
            report_runtime_fault(proc_id, msg);
            return 0;
        }
        b[k] = (unsigned char)memory[phys];
    }
    return (int)((unsigned int)b[0] | ((unsigned int)b[1] << 8)
                | ((unsigned int)b[2] << 16) | ((unsigned int)b[3] << 24));
}

void mem_write_word(int proc_id, int address, int value) {
    unsigned int v = (unsigned int)value;
    unsigned char b[4] = {
        (unsigned char)(v & 0xFF), (unsigned char)((v >> 8) & 0xFF),
        (unsigned char)((v >> 16) & 0xFF), (unsigned char)((v >> 24) & 0xFF)
    };
    for (int k = 0; k < 4; k++) {
        int phys = get_physical_address(proc_id, 0, address + k);
        if (phys < 0) {
            char msg[96];
            snprintf(msg, sizeof(msg), "data page fault writing address %d", address);
            report_runtime_fault(proc_id, msg);
            return;
        }
        memory[phys] = (char)b[k];
    }
}

