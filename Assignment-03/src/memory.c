#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "memory.h"

unsigned char Instruction[INSTR_MEM_SIZE];
unsigned char Data[DATA_MEM_SIZE];

/* Reads a "N bytes, 4 hex bytes per line" file into buf (capacity cap).
 * Returns the number of bytes actually filled in.  Missing bytes are
 * left at whatever they already were (caller zero-initialises first). */
static int load_hex_file(const char *path, unsigned char *buf, int cap) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        /* Not fatal: a program with no data file just runs on a blank
         * data memory, and a missing program file is a genuine error
         * we report and exit from. */
        return 0;
    }
    char line[256];
    int idx = 0;
    while (fgets(line, sizeof(line), fp)) {
        unsigned int b0, b1, b2, b3;
        int n = sscanf(line, "%x %x %x %x", &b0, &b1, &b2, &b3);
        if (n <= 0) continue; /* blank line */
        unsigned int vals[4] = {b0, b1, b2, b3};
        for (int i = 0; i < n && idx < cap; i++) {
            buf[idx++] = (unsigned char)(vals[i] & 0xFF);
        }
    }
    fclose(fp);
    return idx;
}

void mem_initialize(const char *program_byte_file, const char *data_byte_file) {
    memset(Instruction, 0, sizeof(Instruction));
    memset(Data, 0, sizeof(Data));

    if (load_hex_file(program_byte_file, Instruction, INSTR_MEM_SIZE) == 0) {
        fprintf(stderr, "Error: could not read program file '%s'\n", program_byte_file);
        exit(1);
    }
    if (data_byte_file) {
        load_hex_file(data_byte_file, Data, DATA_MEM_SIZE);
    }
}

void mem_finalize(const char *data_byte_file) {
    if (!data_byte_file) return;
    FILE *fp = fopen(data_byte_file, "w");
    if (!fp) {
        fprintf(stderr, "Error: could not write data file '%s'\n", data_byte_file);
        return;
    }
    for (int i = 0; i < DATA_MEM_SIZE; i += 4) {
        fprintf(fp, "%X %X %X %X\n", Data[i], Data[i+1], Data[i+2], Data[i+3]);
    }
    fclose(fp);
}

int mem_read_word(int address) {
    if (address < 0 || address + 4 > DATA_MEM_SIZE) {
        fprintf(stderr, "Error: data read out of bounds at address %d\n", address);
        exit(1);
    }
    /* little-endian: Data[address] is the least significant byte */
    return (int)((unsigned int)Data[address]
               | ((unsigned int)Data[address+1] << 8)
               | ((unsigned int)Data[address+2] << 16)
               | ((unsigned int)Data[address+3] << 24));
}

void mem_write_word(int address, int value) {
    if (address < 0 || address + 4 > DATA_MEM_SIZE) {
        fprintf(stderr, "Error: data write out of bounds at address %d\n", address);
        exit(1);
    }
    unsigned int v = (unsigned int)value;
    Data[address]   = (unsigned char)(v & 0xFF);
    Data[address+1] = (unsigned char)((v >> 8) & 0xFF);
    Data[address+2] = (unsigned char)((v >> 16) & 0xFF);
    Data[address+3] = (unsigned char)((v >> 24) & 0xFF);
}
