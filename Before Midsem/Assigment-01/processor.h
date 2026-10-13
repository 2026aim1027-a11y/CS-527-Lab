#ifndef PROCESSOR
#define PROCESSOR
#include "memory.h"
extern int Register[256];
extern int PC;
extern int opcode;
extern int dest;
extern int src1;
extern int src2;
extern int end_of_simulation;
void reset();
void fetch();
void decode();
void execute();
#endif