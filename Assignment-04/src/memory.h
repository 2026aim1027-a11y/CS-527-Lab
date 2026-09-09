#ifndef MEMORY_H
#define MEMORY_H
#include "opcodes.h"   

#define INSTR_MEM_SIZE 256  
#define DATA_MEM_SIZE  4096  


extern unsigned char Instruction[NUM_PROCESSORS][INSTR_MEM_SIZE];
extern unsigned char Data[NUM_PROCESSORS][DATA_MEM_SIZE];

void mem_initialize(int proc_id, const char *program_byte_file, const char *data_byte_file);

void mem_finalize(int proc_id, const char *data_byte_file);

int  mem_read_word(int proc_id, int address);
void mem_write_word(int proc_id, int address, int value);

#endif
