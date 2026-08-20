#include <stdio.h>
#include <stdlib.h>
#include "processor.h"
#include "memory.h"

int Register[NUM_INT_REG];
int VecRegister[NUM_VEC_REG][VEC_LEN];

int PC;
int opcode, dest, src1, src2;
int flagZ, flagN, flagC, flagV;
int end_of_simulation = 0;

/* address of the instruction currently being executed (set by fetch,
 * used by execute() to compute branch targets) */
static int current_instr_addr;

void reset(void) {
    for (int i = 0; i < NUM_INT_REG; i++) Register[i] = 0;
    for (int i = 0; i < NUM_VEC_REG; i++)
        for (int j = 0; j < VEC_LEN; j++) VecRegister[i][j] = 0;
    PC = 0;
    flagZ = flagN = flagC = flagV = 0;
    end_of_simulation = 0;
}

void fetch(void) {
    current_instr_addr = PC;
    opcode = Instruction[PC];
    dest   = Instruction[PC + 1];
    src1   = Instruction[PC + 2];
    src2   = Instruction[PC + 3];
    PC += 4;
}

void decode(void) {
    /* no-op at this stage, as specified */
}

/* interpret a 0-255 byte field as an 8-bit two's-complement signed value,
 * used only for branch offsets (which can be negative) */
static int as_signed_byte(int b) {
    return (b > 127) ? (b - 256) : b;
}

static void update_add_flags(int a, int b, long long result) {
    unsigned int ua = (unsigned int)a, ub = (unsigned int)b;
    unsigned int ur = (unsigned int)result;
    flagZ = (ur == 0);
    flagN = ((ur >> 31) & 1);
    flagC = (ur < ua) || (ur < ub); /* unsigned result smaller than either input */
    int signA = (a >> 31) & 1, signB = (b >> 31) & 1, signR = (int)((ur >> 31) & 1);
    flagV = (signA == signB) && (signR != signA);
}

static void update_sub_flags(int a, int b, long long result) {
    unsigned int ur = (unsigned int)result;
    flagZ = (ur == 0);
    flagN = ((ur >> 31) & 1);
    flagC = ((unsigned int)a >= (unsigned int)b); /* operand1 >= operand2, unsigned */
    int signA = (a >> 31) & 1, signB = (b >> 31) & 1, signR = (int)((ur >> 31) & 1);
    flagV = (signA != signB) && (signR == signB);
}

static int branch_condition_met(int code) {
    switch (code) {
        case BC_EQ: return flagZ;
        case BC_NE: return !flagZ;
        case BC_CS: return flagC;
        case BC_CC: return !flagC;
        case BC_MI: return flagN;
        case BC_PL: return !flagN;
        case BC_VS: return flagV;
        case BC_VC: return !flagV;
        case BC_HI: return flagC && !flagZ;
        case BC_LS: return !flagC || flagZ;
        case BC_GE: return flagN == flagV;
        case BC_LT: return flagN != flagV;
        case BC_GT: return !flagZ && (flagN == flagV);
        case BC_LE: return flagZ || (flagN != flagV);
        case BC_AL: return 1;
        default: return 0;
    }
}

void execute(void) {
    switch (opcode) {
        case OP_HALT:
            end_of_simulation = 1;
            break;

        case OP_ADD_VAR: {
            long long r = (long long)Register[src1] + Register[src2];
            update_add_flags(Register[src1], Register[src2], r);
            Register[dest] = (int)r;
            break;
        }
        case OP_ADD_CONST: {
            long long r = (long long)Register[src1] + src2;
            update_add_flags(Register[src1], src2, r);
            Register[dest] = (int)r;
            break;
        }
        case OP_SUB_VAR: {
            long long r = (long long)Register[src1] - Register[src2];
            update_sub_flags(Register[src1], Register[src2], r);
            Register[dest] = (int)r;
            break;
        }
        case OP_SUB_CONST: {
            long long r = (long long)Register[src1] - src2;
            update_sub_flags(Register[src1], src2, r);
            Register[dest] = (int)r;
            break;
        }
        case OP_MUL_VAR:
            Register[dest] = Register[src1] * Register[src2];
            break;
        case OP_MUL_CONST:
            Register[dest] = Register[src1] * src2;
            break;
        case OP_DIV_VAR:
            if (Register[src2] == 0) { fprintf(stderr, "Error: divide by zero\n"); exit(1); }
            Register[dest] = Register[src1] / Register[src2];
            break;
        case OP_DIV_CONST:
            if (src2 == 0) { fprintf(stderr, "Error: divide by zero\n"); exit(1); }
            Register[dest] = Register[src1] / src2;
            break;

        case OP_DATAMOVE_CONST:
            Register[dest] = src2;
            break;

        case OP_MEMREAD_VAR:
            Register[dest] = mem_read_word(Register[src2]);
            break;
        case OP_MEMREAD_CONST:
            Register[dest] = mem_read_word(src2);
            break;
        case OP_MEMWRITE_VAR:
            mem_write_word(Register[dest], Register[src2]);
            break;
        case OP_MEMWRITE_CONST:
            mem_write_word(dest, Register[src2]);
            break;

        case OP_VADD_VEC:
            for (int i = 0; i < VEC_LEN; i++)
                VecRegister[dest][i] = VecRegister[src1][i] + VecRegister[src2][i];
            break;
        case OP_VSUB_VEC:
            for (int i = 0; i < VEC_LEN; i++)
                VecRegister[dest][i] = VecRegister[src1][i] - VecRegister[src2][i];
            break;
        case OP_VMUL_VEC:
            for (int i = 0; i < VEC_LEN; i++)
                VecRegister[dest][i] = VecRegister[src1][i] * VecRegister[src2][i];
            break;

        case OP_VADD_SCALARREG:
            for (int i = 0; i < VEC_LEN; i++)
                VecRegister[dest][i] = VecRegister[src1][i] + Register[src2];
            break;
        case OP_VSUB_SCALARREG:
            for (int i = 0; i < VEC_LEN; i++)
                VecRegister[dest][i] = VecRegister[src1][i] - Register[src2];
            break;
        case OP_VMUL_SCALARREG:
            for (int i = 0; i < VEC_LEN; i++)
                VecRegister[dest][i] = VecRegister[src1][i] * Register[src2];
            break;

        case OP_VADD_CONST:
            for (int i = 0; i < VEC_LEN; i++)
                VecRegister[dest][i] = VecRegister[src1][i] + src2;
            break;
        case OP_VSUB_CONST:
            for (int i = 0; i < VEC_LEN; i++)
                VecRegister[dest][i] = VecRegister[src1][i] - src2;
            break;
        case OP_VMUL_CONST:
            for (int i = 0; i < VEC_LEN; i++)
                VecRegister[dest][i] = VecRegister[src1][i] * src2;
            break;

        case OP_VMEMREAD_VAR: {
            int addr = Register[src2];
            for (int i = 0; i < VEC_LEN; i++, addr += 4)
                VecRegister[dest][i] = mem_read_word(addr);
            break;
        }
        case OP_VMEMREAD_CONST: {
            int addr = src2;
            for (int i = 0; i < VEC_LEN; i++, addr += 4)
                VecRegister[dest][i] = mem_read_word(addr);
            break;
        }
        case OP_VMEMWRITE_VAR: {
            int addr = Register[dest];
            for (int i = 0; i < VEC_LEN; i++, addr += 4)
                mem_write_word(addr, VecRegister[src2][i]);
            break;
        }
        case OP_VMEMWRITE_CONST: {
            int addr = dest;
            for (int i = 0; i < VEC_LEN; i++, addr += 4)
                mem_write_word(addr, VecRegister[src2][i]);
            break;
        }

        default:
            if (opcode >= OP_BRANCH_BASE && opcode <= (OP_BRANCH_BASE + 0xE)) {
                int cond = opcode - OP_BRANCH_BASE;
                if (branch_condition_met(cond)) {
                    /* Verified against the handout's worked example: the
                     * encoded offset is a signed instruction-count delta
                     * (target_index - branch_index), not a byte delta, so
                     * it must be scaled by 4 to become an address delta. */
                    int offset_instrs = as_signed_byte(src2);
                    PC = current_instr_addr + offset_instrs * 4;
                }
                /* else: PC already advanced past the branch by fetch() */
            } else {
                fprintf(stderr, "Error: unknown opcode 0x%02X at PC=%d\n", opcode, current_instr_addr);
                exit(1);
            }
            break;
    }
}
