#ifndef PROCESSOR_H
#define PROCESSOR_H
#include "opcodes.h"

extern int Register[NUM_INT_REG];
extern int VecRegister[NUM_VEC_REG][VEC_LEN];

extern int PC;
extern int opcode, dest, src1, src2;
extern int flagZ, flagN, flagC, flagV;
extern int end_of_simulation;

void reset(void);
void fetch(void);
void decode(void);
void execute(void);

#endif
