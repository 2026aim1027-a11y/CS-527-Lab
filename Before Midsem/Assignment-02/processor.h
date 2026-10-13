#ifndef PROCESSOR_H
#define PROCESSOR_H

#define NUM_REGISTERS 256

extern int Register[NUM_REGISTERS];
extern int PC;
extern int opcode, dest, src1, src2;
extern int Z, N, C, V;          /* condition flags */
extern int end_of_simulation;

void reset(void);
void fetch(void);
void decode(void);
void execute(void);

/* Convenience driver: runs fetch/decode/execute until end_of_simulation. */
void run(void);

#endif /* PROCESSOR_H */
