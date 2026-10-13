#ifndef MEMORY_H
#define MEMORY_H

#define INSTR_MEM_SIZE 256   
#define DATA_MEM_SIZE  4096  

extern unsigned char Instruction[INSTR_MEM_SIZE];
extern unsigned char Data[DATA_MEM_SIZE];


void mem_initialize(const char *program_byte_file, const char *data_byte_file);

void mem_finalize(const char *data_byte_file);

/* Small helpers used by the processor to read/write a 32-bit word from
the byte-addressable data memory (little-endian). */
int  mem_read_word(int address);
void mem_write_word(int address, int value);

#endif
