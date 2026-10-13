#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "memory.h"
#include "processor.h"  
#include "load_error.h"  

unsigned char Instruction[NUM_PROCESSORS][INSTR_MEM_SIZE];
unsigned char Data[NUM_PROCESSORS][DATA_MEM_SIZE];

static int load_hex_file(const char *path, unsigned char *buf, int cap) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
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

void mem_initialize(int proc_id, const char *program_byte_file, const char *data_byte_file) {
    memset(Instruction[proc_id], 0, INSTR_MEM_SIZE);
    memset(Data[proc_id], 0, DATA_MEM_SIZE);

    if (load_hex_file(program_byte_file, Instruction[proc_id], INSTR_MEM_SIZE) == 0) {
        load_error("could not read program file '%s' for processor %d",
                    program_byte_file, proc_id);
    }
    if (data_byte_file) {
        load_hex_file(data_byte_file, Data[proc_id], DATA_MEM_SIZE);
    }
}

void mem_finalize(int proc_id, const char *data_byte_file) {
    if (!data_byte_file) return;
    FILE *fp = fopen(data_byte_file, "w");
    if (!fp) {
        fprintf(stderr, "Error: could not write data file '%s'\n", data_byte_file);
        return;
    }
    for (int i = 0; i < DATA_MEM_SIZE; i += 4) {
        fprintf(fp, "%X %X %X %X\n",
                Data[proc_id][i], Data[proc_id][i+1], Data[proc_id][i+2], Data[proc_id][i+3]);
    }
    fclose(fp);
}

int mem_read_word(int proc_id, int address) {
    if (address < 0 || address + 4 > DATA_MEM_SIZE) {
        char msg[96];
        snprintf(msg, sizeof(msg), "data read out of bounds at address %d", address);
        report_runtime_fault(proc_id, msg);
        return 0; /* task is being halted by report_runtime_fault(); this value is discarded */
    }
    unsigned char *d = Data[proc_id];
    return (int)((unsigned int)d[address]
               | ((unsigned int)d[address+1] << 8)
               | ((unsigned int)d[address+2] << 16)
               | ((unsigned int)d[address+3] << 24));
}

void mem_write_word(int proc_id, int address, int value) {
    if (address < 0 || address + 4 > DATA_MEM_SIZE) {
        char msg[96];
        snprintf(msg, sizeof(msg), "data write out of bounds at address %d", address);
        report_runtime_fault(proc_id, msg);
        return;
    }
    unsigned char *d = Data[proc_id];
    unsigned int v = (unsigned int)value;
    d[address]   = (unsigned char)(v & 0xFF);
    d[address+1] = (unsigned char)((v >> 8) & 0xFF);
    d[address+2] = (unsigned char)((v >> 16) & 0xFF);
    d[address+3] = (unsigned char)((v >> 24) & 0xFF);
}
