#include <stdio.h>
#include <stdint.h>

#include "processor.h"
#include "memory.h"

int Register[NUM_REGISTERS];
int PC;
int opcode, dest, src1, src2;
int Z, N, C, V;
int end_of_simulation = 0;

/* Same opcode table as compiler.c -- kept local/duplicated on purpose so
 * processor.c has no compile-time dependency on the compiler. */
#define OP_HALT            0x00

#define OP_ADD_VAR         0x01
#define OP_SUB_VAR         0x02
#define OP_MUL_VAR         0x03
#define OP_DIV_VAR         0x04
#define OP_MEMREAD_VAR     0x05
#define OP_MEMWRITE_VAR    0x06

#define OP_ADD_CONST       0x09
#define OP_SUB_CONST       0x0A
#define OP_MUL_CONST       0x0B
#define OP_DIV_CONST       0x0C
#define OP_MEMREAD_CONST   0x0D
#define OP_MEMWRITE_CONST  0x0E
#define OP_MOVE_CONST      0x0F

#define OP_BRANCH_BASE     0x10
#define OP_BRANCH_LAST     0x1E /* 0x10 + 14 (AL) */

void reset(void) {
    int i;
    for (i = 0; i < NUM_REGISTERS; i++) Register[i] = 0;
    PC = 0;
    Z = N = C = V = 0;
    end_of_simulation = 0;
}

void fetch(void) {
    opcode = Instruction[PC];
    dest   = Instruction[PC + 1];
    src1   = Instruction[PC + 2];
    src2   = Instruction[PC + 3];
}

void decode(void) {
    /* Nothing to do -- kept as a separate stage per the spec's fetch /
     * decode / execute pipeline shape. */
}

/* Reads a 32-bit big-endian word from Data[addr..addr+3]. */
static int mem_read32(int addr) {
    if (addr < 0 || addr + 4 > DATA_MEM_SIZE) {
        fprintf(stderr, "processor: memory read out of bounds (addr=%d)\n", addr);
        return 0;
    }
    unsigned int v = ((unsigned int)Data[addr] << 24) |
                      ((unsigned int)Data[addr + 1] << 16) |
                      ((unsigned int)Data[addr + 2] << 8) |
                      ((unsigned int)Data[addr + 3]);
    return (int)v;
}

/* Writes a 32-bit big-endian word into Data[addr..addr+3]. */
static void mem_write32(int addr, int value) {
    if (addr < 0 || addr + 4 > DATA_MEM_SIZE) {
        fprintf(stderr, "processor: memory write out of bounds (addr=%d)\n", addr);
        return;
    }
    unsigned int v = (unsigned int)value;
    Data[addr]     = (unsigned char)((v >> 24) & 0xFF);
    Data[addr + 1] = (unsigned char)((v >> 16) & 0xFF);
    Data[addr + 2] = (unsigned char)((v >> 8) & 0xFF);
    Data[addr + 3] = (unsigned char)(v & 0xFF);
}

/* Updates Z, N flags common to add and subtract. */
static void update_zn(int result) {
    Z = (result == 0) ? 1 : 0;
    N = ((result >> 31) & 1) ? 1 : 0;
}

static void do_add(int a, int b) {
    int result = a + b;
    update_zn(result);
    /* C: unsigned result is strictly less than either unsigned input */
    unsigned int ua = (unsigned int)a, ub = (unsigned int)b, ur = (unsigned int)result;
    C = (ur < ua || ur < ub) ? 1 : 0;
    /* V: both operands same sign, result sign differs from operands' sign */
    int sign_a = (a >> 31) & 1, sign_b = (b >> 31) & 1, sign_r = (result >> 31) & 1;
    V = (sign_a == sign_b && sign_r != sign_a) ? 1 : 0;
    Register[dest] = result;
}

static void do_sub(int a, int b) {
    int result = a - b;
    update_zn(result);
    /* C: operand1 is bigger than operand2 (as stated in the spec) */
    C = (a > b) ? 1 : 0;
    /* V: operands have different signs, and result's sign matches
       operand2's original sign */
    int sign_a = (a >> 31) & 1, sign_b = (b >> 31) & 1, sign_r = (result >> 31) & 1;
    V = (sign_a != sign_b && sign_r == sign_b) ? 1 : 0;
    Register[dest] = result;
}

/* Evaluates whether a branch condition code (0..14) is currently true. */
static int cond_true(int code) {
    switch (code) {
        case 0:  return Z == 1;                         /* EQ */
        case 1:  return Z == 0;                          /* NE */
        case 2:  return C == 1;                           /* CS */
        case 3:  return C == 0;                            /* CC */
        case 4:  return N == 1;                             /* MI */
        case 5:  return N == 0;                              /* PL */
        case 6:  return V == 1;                               /* VS */
        case 7:  return V == 0;                                /* VC */
        case 8:  return C == 1 && Z == 0;                       /* HI */
        case 9:  return C == 0 || Z == 1;                        /* LS */
        case 10: return N == V;                                   /* GE */
        case 11: return N != V;                                    /* LT */
        case 12: return Z == 0 && (N == V);                         /* GT */
        case 13: return Z == 1 || (N != V);                          /* LE */
        case 14: return 1;                                            /* AL */
        default: return 0;
    }
}

void execute(void) {
    if (opcode == OP_HALT) {
        end_of_simulation = 1;
        return;
    }

    int is_branch = (opcode >= OP_BRANCH_BASE && opcode <= OP_BRANCH_LAST);
    int branch_taken = 0;

    switch (opcode) {
        case OP_ADD_VAR:
            do_add(Register[src1], Register[src2]);
            break;
        case OP_ADD_CONST:
            do_add(Register[src1], src2);
            break;
        case OP_SUB_VAR:
            do_sub(Register[src1], Register[src2]);
            break;
        case OP_SUB_CONST:
            do_sub(Register[src1], src2);
            break;
        case OP_MUL_VAR:
            Register[dest] = Register[src1] * Register[src2];
            break;
        case OP_MUL_CONST:
            Register[dest] = Register[src1] * src2;
            break;
        case OP_DIV_VAR:
            Register[dest] = (Register[src2] != 0) ? Register[src1] / Register[src2] : 0;
            break;
        case OP_DIV_CONST:
            Register[dest] = (src2 != 0) ? Register[src1] / src2 : 0;
            break;
        case OP_MOVE_CONST:
            Register[dest] = src2;
            break;
        case OP_MEMREAD_VAR:
            Register[dest] = mem_read32(Register[src2]);
            break;
        case OP_MEMREAD_CONST:
            Register[dest] = mem_read32(src2);
            break;
        case OP_MEMWRITE_VAR:
            mem_write32(Register[dest], Register[src2]);
            break;
        case OP_MEMWRITE_CONST:
            mem_write32(dest, Register[src2]);
            break;
        default:
            if (is_branch) {
                int cond_code = opcode - OP_BRANCH_BASE;
                branch_taken = cond_true(cond_code);
            } else {
                fprintf(stderr, "processor: unknown opcode 0x%02X at PC=%d\n", opcode, PC);
            }
            break;
    }

    if (is_branch && branch_taken) {
        int8_t signed_offset = (int8_t)src2; /* offset stored two's-complement, instruction units */
        PC = PC + (int)signed_offset * 4;
    } else {
        PC = PC + 4;
    }
}

void run(void) {
    while (!end_of_simulation) {
        fetch();
        decode();
        execute();
    }
}
