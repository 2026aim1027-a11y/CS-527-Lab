#ifndef MEMORY_H
#define MEMORY_H

#define INSTR_MEM_SIZE 256   /* bytes -> 64 instructions of 4 bytes each   */
#define DATA_MEM_SIZE  4096  /* bytes                                      */

extern unsigned char Instruction[INSTR_MEM_SIZE];
extern unsigned char Data[DATA_MEM_SIZE];

/* Reads program_byte_file into Instruction[] and data_byte_file into
 * Data[].  Both files are stored as N lines of 4 space separated hex
 * bytes (no 0x prefix), exactly as specified in the assignment. */
void mem_initialize(const char *program_byte_file, const char *data_byte_file);

/* Writes the current contents of Data[] back out to data_byte_file, in
 * the same 4-bytes-per-line hex format. */
void mem_finalize(const char *data_byte_file);

/* Small helpers used by the processor to read/write a 32-bit word from
 * the byte-addressable data memory (little-endian). */
int  mem_read_word(int address);
void mem_write_word(int address, int value);

#endif
