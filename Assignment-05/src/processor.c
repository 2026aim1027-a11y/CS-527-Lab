#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
    #include <windows.h>  /* Sleep() - <time.h>'s nanosleep() isn't reliably
                            * available across MinGW toolchains on Windows */
#else
    #include <time.h>     /* nanosleep() */
#endif
#include "processor.h"
#include "memory.h"

int Register[NUM_PROCESSORS][NUM_INT_REG];
int VecRegister[NUM_PROCESSORS][NUM_VEC_REG][VEC_LEN];

int PC[NUM_PROCESSORS];
int opcode, dest, src1, src2;
int flagZ[NUM_PROCESSORS], flagN[NUM_PROCESSORS], flagC[NUM_PROCESSORS], flagV[NUM_PROCESSORS];
int end_of_simulation[NUM_PROCESSORS];
int task_fault[NUM_PROCESSORS];
char task_fault_msg[NUM_PROCESSORS][128];

int proc_id = 0;

/* address of the instruction currently being executed on `proc_id`
 * (set by fetch, used by execute() to compute branch targets) */
static int current_instr_addr[NUM_PROCESSORS];

/* Shared OS-wide log file that Print writes to, opened once in append
 * mode the first time any processor is reset - matches the handout's
 * "FILE *fd_log" / "Reset() ... Open a log file, in append mode." */
static FILE *fd_log = NULL;
#define LOG_FILE_NAME "os.log"

void reset(int p) {
    for (int i = 0; i < NUM_INT_REG; i++) Register[p][i] = 0;
    for (int i = 0; i < NUM_VEC_REG; i++)
        for (int j = 0; j < VEC_LEN; j++) VecRegister[p][i][j] = 0;
    PC[p] = 0;
    flagZ[p] = flagN[p] = flagC[p] = flagV[p] = 0;
    end_of_simulation[p] = 0;
    task_fault[p] = 0;
    task_fault_msg[p][0] = '\0';

    if (!fd_log) {
        fd_log = fopen(LOG_FILE_NAME, "a");
        if (!fd_log) {
            fprintf(stderr, "Warning: could not open '%s' for Print logging\n", LOG_FILE_NAME);
        }
    }
}

void report_runtime_fault(int proc_id, const char *msg) {
    if (!task_fault[proc_id]) { /* keep the *first* fault; later ones during
                                  * the same already-stopped task are noise */
        task_fault[proc_id] = 1;
        snprintf(task_fault_msg[proc_id], sizeof(task_fault_msg[proc_id]), "%s", msg);
        if (fd_log) {
            fprintf(fd_log, "Process id: %d  FAULT: %s (task halted, other tasks unaffected)\n",
                    proc_id, msg);
            fflush(fd_log);
        }
    }
    end_of_simulation[proc_id] = 1; /* stop *this* task only */
}

void fetch(void) {
    current_instr_addr[proc_id] = PC[proc_id];
    /* Lab 5: instructions now come from paged memory via the MMU
     * instead of a private per-processor array. mem_fetch_byte()
     * reports a runtime fault (and returns a harmless 0) on a bad PC -
     * e.g. a corrupt branch target - so we bail out to a clean halt
     * immediately rather than assembling an instruction out of a mix
     * of real and placeholder bytes. */
    opcode = mem_fetch_byte(proc_id, PC[proc_id]);
    if (end_of_simulation[proc_id]) { opcode = OP_HALT; dest = src1 = src2 = 0; return; }
    dest = mem_fetch_byte(proc_id, PC[proc_id] + 1);
    if (end_of_simulation[proc_id]) { opcode = OP_HALT; dest = src1 = src2 = 0; return; }
    src1 = mem_fetch_byte(proc_id, PC[proc_id] + 2);
    if (end_of_simulation[proc_id]) { opcode = OP_HALT; dest = src1 = src2 = 0; return; }
    src2 = mem_fetch_byte(proc_id, PC[proc_id] + 3);
    if (end_of_simulation[proc_id]) { opcode = OP_HALT; dest = src1 = src2 = 0; return; }
    PC[proc_id] += 4;
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
    flagZ[proc_id] = (ur == 0);
    flagN[proc_id] = ((ur >> 31) & 1);
    flagC[proc_id] = (ur < ua) || (ur < ub);
    int signA = (a >> 31) & 1, signB = (b >> 31) & 1, signR = (int)((ur >> 31) & 1);
    flagV[proc_id] = (signA == signB) && (signR != signA);
}

static void update_sub_flags(int a, int b, long long result) {
    unsigned int ur = (unsigned int)result;
    flagZ[proc_id] = (ur == 0);
    flagN[proc_id] = ((ur >> 31) & 1);
    flagC[proc_id] = ((unsigned int)a >= (unsigned int)b);
    int signA = (a >> 31) & 1, signB = (b >> 31) & 1, signR = (int)((ur >> 31) & 1);
    flagV[proc_id] = (signA != signB) && (signR == signB);
}

static int branch_condition_met(int code) {
    int Z = flagZ[proc_id], N = flagN[proc_id], C = flagC[proc_id], V = flagV[proc_id];
    switch (code) {
        case BC_EQ: return Z;
        case BC_NE: return !Z;
        case BC_CS: return C;
        case BC_CC: return !C;
        case BC_MI: return N;
        case BC_PL: return !N;
        case BC_VS: return V;
        case BC_VC: return !V;
        case BC_HI: return C && !Z;
        case BC_LS: return !C || Z;
        case BC_GE: return N == V;
        case BC_LT: return N != V;
        case BC_GT: return !Z && (N == V);
        case BC_LE: return Z || (N != V);
        case BC_AL: return 1;
        default: return 0;
    }
}

void execute(void) {
    int *R = Register[proc_id];
    int (*VR)[VEC_LEN] = VecRegister[proc_id];

    switch (opcode) {
        case OP_HALT:
            end_of_simulation[proc_id] = 1;
            break;

        case OP_PRINT:
            /* dest and src1 are 0 per spec; src2 holds the register number */
            if (fd_log) {
                fprintf(fd_log, "Process id: %d  x%d : %X\n", proc_id, src2, R[src2]);
                fflush(fd_log);
            }
            break;

        case OP_ADD_VAR: {
            long long r = (long long)R[src1] + R[src2];
            update_add_flags(R[src1], R[src2], r);
            R[dest] = (int)r;
            break;
        }
        case OP_ADD_CONST: {
            long long r = (long long)R[src1] + src2;
            update_add_flags(R[src1], src2, r);
            R[dest] = (int)r;
            break;
        }
        case OP_SUB_VAR: {
            long long r = (long long)R[src1] - R[src2];
            update_sub_flags(R[src1], R[src2], r);
            R[dest] = (int)r;
            break;
        }
        case OP_SUB_CONST: {
            long long r = (long long)R[src1] - src2;
            update_sub_flags(R[src1], src2, r);
            R[dest] = (int)r;
            break;
        }
        case OP_MUL_VAR:
            R[dest] = R[src1] * R[src2];
            break;
        case OP_MUL_CONST:
            R[dest] = R[src1] * src2;
            break;
        case OP_DIV_VAR:
            if (R[src2] == 0) { report_runtime_fault(proc_id, "divide by zero"); break; }
            R[dest] = R[src1] / R[src2];
            break;
        case OP_DIV_CONST:
            if (src2 == 0) { report_runtime_fault(proc_id, "divide by zero"); break; }
            R[dest] = R[src1] / src2;
            break;

        case OP_DATAMOVE_CONST:
            R[dest] = src2;
            break;

        case OP_MEMREAD_VAR:
            R[dest] = mem_read_word(proc_id, R[src2]);
            break;
        case OP_MEMREAD_CONST:
            R[dest] = mem_read_word(proc_id, src2);
            break;
        case OP_MEMWRITE_VAR:
            mem_write_word(proc_id, R[dest], R[src2]);
            break;
        case OP_MEMWRITE_CONST:
            mem_write_word(proc_id, dest, R[src2]);
            break;

        case OP_VADD_VEC:
            for (int i = 0; i < VEC_LEN; i++) VR[dest][i] = VR[src1][i] + VR[src2][i];
            break;
        case OP_VSUB_VEC:
            for (int i = 0; i < VEC_LEN; i++) VR[dest][i] = VR[src1][i] - VR[src2][i];
            break;
        case OP_VMUL_VEC:
            for (int i = 0; i < VEC_LEN; i++) VR[dest][i] = VR[src1][i] * VR[src2][i];
            break;

        case OP_VADD_SCALARREG:
            for (int i = 0; i < VEC_LEN; i++) VR[dest][i] = VR[src1][i] + R[src2];
            break;
        case OP_VSUB_SCALARREG:
            for (int i = 0; i < VEC_LEN; i++) VR[dest][i] = VR[src1][i] - R[src2];
            break;
        case OP_VMUL_SCALARREG:
            for (int i = 0; i < VEC_LEN; i++) VR[dest][i] = VR[src1][i] * R[src2];
            break;

        case OP_VADD_CONST:
            for (int i = 0; i < VEC_LEN; i++) VR[dest][i] = VR[src1][i] + src2;
            break;
        case OP_VSUB_CONST:
            for (int i = 0; i < VEC_LEN; i++) VR[dest][i] = VR[src1][i] - src2;
            break;
        case OP_VMUL_CONST:
            for (int i = 0; i < VEC_LEN; i++) VR[dest][i] = VR[src1][i] * src2;
            break;

        case OP_VMEMREAD_VAR: {
            int addr = R[src2];
            for (int i = 0; i < VEC_LEN; i++, addr += 4) VR[dest][i] = mem_read_word(proc_id, addr);
            break;
        }
        case OP_VMEMREAD_CONST: {
            int addr = src2;
            for (int i = 0; i < VEC_LEN; i++, addr += 4) VR[dest][i] = mem_read_word(proc_id, addr);
            break;
        }
        case OP_VMEMWRITE_VAR: {
            int addr = R[dest];
            for (int i = 0; i < VEC_LEN; i++, addr += 4) mem_write_word(proc_id, addr, VR[src2][i]);
            break;
        }
        case OP_VMEMWRITE_CONST: {
            int addr = dest;
            for (int i = 0; i < VEC_LEN; i++, addr += 4) mem_write_word(proc_id, addr, VR[src2][i]);
            break;
        }

        default:
            if (opcode >= OP_BRANCH_BASE && opcode <= (OP_BRANCH_BASE + 0xE)) {
                int cond = opcode - OP_BRANCH_BASE;
                if (branch_condition_met(cond)) {
                    int offset_instrs = as_signed_byte(src2);
                    PC[proc_id] = current_instr_addr[proc_id] + offset_instrs * 4;
                }
            } else {
                char msg[96];
                snprintf(msg, sizeof(msg), "unknown opcode 0x%02X at PC=%d", opcode, current_instr_addr[proc_id]);
                report_runtime_fault(proc_id, msg);
            }
            break;
    }
}

void process_instructions(int p, int instruction_count) {
    proc_id = p;
    for (int i = 0; i < instruction_count && !end_of_simulation[p]; i++) {
        fetch();
        decode();
        execute();
    }
    /* "A sleep statement is added to see real time execution" - ~10us,
     * via the platform's own sleep primitive (nanosleep on POSIX; Sleep()
     * on Windows only takes whole milliseconds, so we round up to 1ms -
     * still a real, visible time slice, just coarser than on POSIX). */
#ifdef _WIN32
    Sleep(1);
#else
    struct timespec ts = {0, 10000L};
    nanosleep(&ts, NULL);
#endif
}
