#ifndef MEMORY_H
#define MEMORY_H

#define INSTR_MEM_SIZE 256
#define DATA_MEM_SIZE 4096

extern unsigned char Instruction[INSTR_MEM_SIZE];
extern unsigned char Data[DATA_MEM_SIZE];

/* Reads "program.byte" into Instruction[] and "data.byte" into Data[].
 * Both files store space separated hex byte values, 4 per line.
 * Returns 0 on success, -1 on failure. */
int initialize(const char *program_byte_file, const char *data_byte_file);

/* Writes the current contents of Data[] back out to "data.byte", in the
 * same 4-bytes-per-line hex format it was read in. */
int finalize(const char *data_byte_file);

#endif /* MEMORY_H */
