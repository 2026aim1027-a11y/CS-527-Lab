#ifndef MEMORY_H
#define MEMORY_H
#include "opcodes.h"   /* MEMSIZE, PAGESIZE, NUM_*_PAGES, MAX_PROC */

/* Lab 5: logical <-> physical memory split.
 *
 * There is now exactly ONE physical memory shared by every processor
 * (the handout's own diagram: "Memory / Size MEMSIZE byte" sits below
 * *all* the processor boxes, not one per processor). Each task's
 * program/data bytes are addressed *logically* (0..1023 for
 * instructions, 0..4095 for data, exactly as in Lab 4) but those
 * logical addresses now get translated through a per-processor-slot
 * page table into physical byte offsets inside `memory[]`.
 *
 * This replaces Lab 4's Instruction[NUM_PROCESSORS][...] /
 * Data[NUM_PROCESSORS][...] arrays entirely - those were a fixed
 * private memory per processor, which is exactly what Lab 5 removes.
 * (The handout's own processor-spec text still lists
 * "extern char Instruction[NP][256], Data[NP][4096]" verbatim, but that
 * reads as leftover boilerplate carried over from earlier labs' text:
 * the same section introduces the single `char memory[MEMSIZE]` array
 * moments earlier, and the whole point of getPhysicallAddress() is that
 * fetch()/execute() no longer index a private per-processor array
 * directly. Keeping both would mean maintaining two copies of every
 * byte with no defined relationship between them, so this
 * implementation keeps only the unified `memory[MEMSIZE]`.)
 */
extern char memory[MEMSIZE];

/* pageTable[p][i] = physical frame number backing logical page i of
 * whatever task currently owns processor slot p, or 0 if that logical
 * page has never been mapped. 0 doubles safely as "unmapped" because
 * frame 0 is reserved and can never be a real assignment (handout:
 * "Frame 0 is reserved, and will never be allocated"). Logical page
 * index i: 0..NUM_INSTR_PAGES-1 for instructions, NUM_INSTR_PAGES..
 * NUM_LOGICAL_PAGES-1 for data - exactly the split
 * getPhysicalAddress()'s own index formula in the handout describes. */
extern unsigned char pageTable[MAX_PROC][NUM_LOGICAL_PAGES];

/* freePages[f] = 1 if physical frame f is currently allocated to some
 * task, 0 if free. A single global array: physical memory is shared by
 * every processor, so allocation state is system-wide, not per-task. */
extern unsigned char freePages[NUM_PHYSICAL_PAGES];

/* Called once, at OS startup, before any task is ever loaded: marks
 * frame 0 permanently reserved and every other frame free. */
void mmu_init_global(void);

/* Returns the number of a currently-free physical frame (1..
 * NUM_PHYSICAL_PAGES-1; never 0, that's reserved) and marks it
 * allocated, or -1 if none are free. Callers decide what "-1" means
 * for them (a load-time out-of-memory vs. a runtime page fault) -
 * this function itself never prints or exits. */
int get_free_page(void);

/* Frees every frame currently mapped in pageTable[proc_id][...] back
 * into freePages[], and resets that page table to all-unmapped. Used
 * by mem_finalize() when a task completes ("returns the pages used by
 * a task" per the handout) and defensively by mem_initialize() before
 * a (possibly reused) processor slot is bound to a new task. */
void mmu_release_all(int proc_id);

/* Translates a logical address into a byte offset into memory[], per
 * the handout's getPhysicallAddress(proc_id, isFetch, address):
 * isFetch=1 selects the instruction address space (0..1023), isFetch=0
 * selects the data address space (0..4095). Returns -1 - instead of
 * calling exit() - if the address is out of range for its space, or
 * lands on a logical page that was never mapped for this processor;
 * the caller (memory.c's own byte accessors, below) is responsible for
 * turning that into a per-task fault via report_runtime_fault(),
 * consistent with how every other runtime error is isolated to just
 * the offending task (see processor.h). */
int get_physical_address(int proc_id, int isFetch, int address);

/* Loads program_byte_file into freshly-allocated instruction frames and
 * (if non-NULL) data_byte_file into freshly-allocated data frames for
 * proc_id, allocating only as many pages as each file's actual content
 * needs (rounding up), per the handout's initialize(): "finds how many
 * PAGES are there in program.byte ... for every page, get a physical
 * page using getFreePage()". A data page covering logical addresses
 * 0-511 is always allocated even with no data file at all, so tasks
 * that never shipped one (every Lab 1-4 test program) can still safely
 * use a small amount of scratch data memory - see the README for the
 * reasoning. Calls load_error() (a recoverable, per-task failure - see
 * load_error.h), not exit(), if a file can't be read or physical
 * memory is exhausted, so one bad/oversized task can't take the whole
 * OS down. */
void mem_initialize(int proc_id, const char *program_byte_file, const char *data_byte_file);

/* Writes proc_id's full logical data image (0..4095, unmapped pages
 * read back as all-zero) out to data_byte_file in the usual "4 hex
 * bytes per line" format, then calls mmu_release_all(proc_id) to
 * return every frame it was using - the handout's "Write data.byte /
 * reset page table / Free pages from freePage array", in that order. */
void mem_finalize(int proc_id, const char *data_byte_file);

/* Byte/word accessors used by processor.c, all going through
 * get_physical_address() first. On a translation failure they report a
 * runtime fault for proc_id (halting only that task) and return a
 * harmless placeholder (0) rather than touching memory[] at all. */
unsigned char mem_fetch_byte(int proc_id, int logical_addr);      /* isFetch=1 */
int  mem_read_word(int proc_id, int address);                     /* isFetch=0 */
void mem_write_word(int proc_id, int address, int value);         /* isFetch=0 */

#endif
