#ifndef PROCESSOR_H
#define PROCESSOR_H
#include "opcodes.h"

extern int Register[NUM_PROCESSORS][NUM_INT_REG];
extern int VecRegister[NUM_PROCESSORS][NUM_VEC_REG][VEC_LEN];

extern int PC[NUM_PROCESSORS];
extern int opcode, dest, src1, src2;
extern int flagZ[NUM_PROCESSORS], flagN[NUM_PROCESSORS], flagC[NUM_PROCESSORS], flagV[NUM_PROCESSORS];
extern int end_of_simulation[NUM_PROCESSORS];

extern int  task_fault[NUM_PROCESSORS];
extern char task_fault_msg[NUM_PROCESSORS][128];

void report_runtime_fault(int proc_id, const char *msg);

extern int proc_id;

void reset(int p);      
void fetch(void);      
void decode(void);
void execute(void);

void process_instructions(int p, int instruction_count);

#endif
