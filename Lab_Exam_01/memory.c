#include <stdio.h>
#include <string.h>

#include "memory.h"

unsigned char Instruction[INSTR_MEM_SIZE];
unsigned char Data[DATA_MEM_SIZE];

/* Reads a hex-byte-per-line-of-4 file into buf[0..buf_size). Missing file
 * is not fatal -- buf is left zeroed (a program with no initial data is
 * legal). Returns number of bytes filled, or -1 on a malformed file. */
static int load_hex_bytes(const char *filename, unsigned char *buf, int buf_size) {
    memset(buf, 0, buf_size);

    FILE *fp = fopen(filename, "r");
    if (!fp) return 0; /* nothing to load, not an error */

    int idx = 0;
    unsigned int b0, b1, b2, b3;
    while (fscanf(fp, "%x %x %x %x", &b0, &b1, &b2, &b3) == 4) {
        if (idx + 4 > buf_size) {
            fprintf(stderr, "memory: %s exceeds %d bytes, truncating\n", filename, buf_size);
            break;
        }
        buf[idx++] = (unsigned char)b0;
        buf[idx++] = (unsigned char)b1;
        buf[idx++] = (unsigned char)b2;
        buf[idx++] = (unsigned char)b3;
    }

    fclose(fp);
    return idx;
}

int initialize(const char *program_byte_file, const char *data_byte_file) {
    if (load_hex_bytes(program_byte_file, Instruction, INSTR_MEM_SIZE) < 0) return -1;
    if (load_hex_bytes(data_byte_file, Data, DATA_MEM_SIZE) < 0) return -1;
    return 0;
}

int finalize(const char *data_byte_file) {
    FILE *fp = fopen(data_byte_file, "w");
    if (!fp) { fprintf(stderr, "memory: cannot open %s for writing\n", data_byte_file); return -1; }

    int i;
    for (i = 0; i < DATA_MEM_SIZE; i += 4) {
        fprintf(fp, "%02X %02X %02X %02X\n",
                Data[i], Data[i + 1], Data[i + 2], Data[i + 3]);
    }

    fclose(fp);
    return 0;
}
